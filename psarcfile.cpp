#include "PsarcFile.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <zlib.h>
#include <lzma.h>
#include <cstring>
#include <openssl/evp.h>

// Rocksmith PSARC AES-256-CFB key
static const unsigned char PSARC_KEY[] = {
    0xC5, 0x3D, 0xB2, 0x38, 0x70, 0xA1, 0xA2, 0xF7,
    0x1C, 0xAE, 0x64, 0x06, 0x1F, 0xDD, 0x0E, 0x11,
    0x57, 0x30, 0x9D, 0xC8, 0x52, 0x04, 0xD4, 0xC5,
    0xBF, 0xDF, 0x25, 0x09, 0x0D, 0xF2, 0x57, 0x2C
};

// ARC IV for TOC decryption
static const unsigned char PSARC_IV[] = {
    0xE9, 0x15, 0xAA, 0x01, 0x8F, 0xEF, 0x71, 0xFC,
    0x50, 0x81, 0x32, 0xE4, 0xBB, 0x4C, 0xEB, 0x42
};

// Rocksmith SNG file key (PC)
static const unsigned char SNG_KEY[] = {
    0xCB, 0x64, 0x8D, 0xF3, 0xD1, 0x2A, 0x16, 0xBF,
    0x71, 0x70, 0x14, 0x14, 0xE6, 0x96, 0x19, 0xEC,
    0x17, 0x1C, 0xCA, 0x5D, 0x2A, 0x14, 0x2E, 0x3E,
    0x59, 0xDE, 0x7A, 0xDD, 0xA1, 0x8A, 0x3A, 0x30
};

PsarcFile::PsarcFile(const QString& filePath)
    : m_filePath(filePath)
    , m_isOpen(false)
    , m_tocEncrypted(false)
{
}

PsarcFile::~PsarcFile()
{
    close();
}

bool PsarcFile::open()
{
    if (m_isOpen) {
        return true;
    }

    m_file = std::make_unique<QFile>(m_filePath);

    if (!m_file->open(QIODevice::ReadOnly)) {
        m_lastError = QString("Failed to open file: %1").arg(m_file->errorString());
        return false;
    }

    if (!readHeader()) {
        close();
        return false;
    }

    if (!readTOC()) {
        close();
        return false;
    }

    if (!readManifest()) {
        close();
        return false;
    }

    m_isOpen = true;
    return true;
}

void PsarcFile::close()
{
    if (m_file) {
        m_file->close();
        m_file.reset();
    }
    m_entries.clear();
    m_fileMap.clear();
    m_zLengths.clear();
    m_isOpen = false;
    m_tocEncrypted = false;
}

bool PsarcFile::isOpen() const
{
    return m_isOpen;
}

const PsarcFile::FileEntry* PsarcFile::getEntry(int index) const
{
    if (index < 0 || index >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries[index];
}

const PsarcFile::FileEntry* PsarcFile::getEntry(const QString& fileName) const
{
    auto it = m_fileMap.find(fileName);
    if (it == m_fileMap.end()) {
        return nullptr;
    }
    return &m_entries[it.value()];
}

bool PsarcFile::readHeader()
{
    m_file->seek(0);

    QDataStream stream(m_file.get());
    stream.setByteOrder(QDataStream::BigEndian);

    stream >> m_header.magic;

    if (m_header.magic != 0x50534152) {
        m_lastError = QString("Invalid PSARC file: wrong magic number (got 0x%1, expected 0x50534152)")
        .arg(m_header.magic, 8, 16, QChar('0'));
        return false;
    }

    stream >> m_header.versionMajor;
    stream >> m_header.versionMinor;
    stream.readRawData(m_header.compressionMethod, 4);
    stream >> m_header.tocLength;
    stream >> m_header.tocEntrySize;
    stream >> m_header.numFiles;
    stream >> m_header.blockSize;
    stream >> m_header.archiveFlags;

    m_tocEncrypted = (m_header.archiveFlags & 0x04) != 0;

    if (m_header.versionMajor != 1 || m_header.versionMinor != 4) {
        m_lastError = QString("Unsupported PSARC version: %1.%2 (expected 1.4)")
        .arg(m_header.versionMajor)
            .arg(m_header.versionMinor);
        return false;
    }

    QString compressionStr = QString::fromLatin1(m_header.compressionMethod, 4);

    qDebug() << "PSARC Header:";
    qDebug() << "  Magic: PSAR";
    qDebug() << "  Version:" << m_header.versionMajor << "." << m_header.versionMinor;
    qDebug() << "  Compression:" << compressionStr;
    qDebug() << "  TOC Length:" << m_header.tocLength;
    qDebug() << "  TOC Entry Size:" << m_header.tocEntrySize;
    qDebug() << "  Num Files:" << m_header.numFiles;
    qDebug() << "  Block Size:" << m_header.blockSize;
    qDebug() << "  Archive Flags:" << Qt::hex << m_header.archiveFlags << Qt::dec;
    qDebug() << "  TOC Encrypted:" << m_tocEncrypted;

    return true;
}

QByteArray PsarcFile::decryptTOC(const QByteArray& encryptedData)
{
    if (encryptedData.isEmpty()) {
        return QByteArray();
    }

    int paddedSize = encryptedData.size();
    int remainder = paddedSize % 16;
    if (remainder != 0) {
        paddedSize += (16 - remainder);
    }

    QByteArray paddedInput = encryptedData;
    if (paddedInput.size() < paddedSize) {
        paddedInput.append(QByteArray(paddedSize - paddedInput.size(), '\0'));
    }

    QByteArray decrypted(paddedSize, Qt::Uninitialized);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        m_lastError = "Failed to create cipher context";
        return QByteArray();
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cfb128(), nullptr, PSARC_KEY, PSARC_IV) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        m_lastError = "Failed to initialize AES decryption";
        return QByteArray();
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int len = 0;
    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(decrypted.data()),
                          &len, reinterpret_cast<const unsigned char*>(paddedInput.constData()),
                          paddedInput.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        m_lastError = "Failed to decrypt TOC";
        return QByteArray();
    }

    int finalLen = 0;
    EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(decrypted.data()) + len, &finalLen);

    EVP_CIPHER_CTX_free(ctx);

    decrypted.resize(encryptedData.size());
    return decrypted;
}

QByteArray PsarcFile::decryptSNG(const QByteArray& encryptedData)
{
    if (encryptedData.size() < 24) {
        m_lastError = "SNG data too short";
        return QByteArray();
    }

    QDataStream headerStream(encryptedData);
    headerStream.setByteOrder(QDataStream::LittleEndian);

    uint32_t magic;
    headerStream >> magic;

    if (magic != 0x4A) {
        m_lastError = "Invalid SNG magic";
        return QByteArray();
    }

    uint32_t flags;
    headerStream >> flags;

    QByteArray iv = encryptedData.mid(8, 16);
    QByteArray payload = encryptedData.mid(24);

    QByteArray decrypted(payload.size(), Qt::Uninitialized);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return QByteArray();
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, SNG_KEY,
                           reinterpret_cast<const unsigned char*>(iv.constData())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int len = 0;
    if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(decrypted.data()),
                          &len, reinterpret_cast<const unsigned char*>(payload.constData()),
                          payload.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    int finalLen = 0;
    EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(decrypted.data()) + len, &finalLen);

    EVP_CIPHER_CTX_free(ctx);

    decrypted.resize(len + finalLen);

    if (flags & 0x01) {
        if (decrypted.size() < 4) {
            return QByteArray();
        }

        QDataStream sizeStream(decrypted);
        sizeStream.setByteOrder(QDataStream::LittleEndian);
        uint32_t uncompressedSize;
        sizeStream >> uncompressedSize;

        QByteArray compressedData = decrypted.mid(4);
        bool success = false;
        QByteArray uncompressed = decompressZlib(compressedData, uncompressedSize, success);

        return uncompressed;
    }

    return decrypted;
}

bool PsarcFile::readTOC()
{
    qDebug() << "TOC encrypted:" << m_tocEncrypted;

    m_file->seek(32);

    qint64 tocDataSize = m_header.tocLength - 32;
    QByteArray tocData = m_file->read(tocDataSize);

    if (tocData.size() != tocDataSize) {
        m_lastError = QString("Failed to read TOC data: expected %1 bytes, got %2")
        .arg(tocDataSize).arg(tocData.size());
        return false;
    }

    qDebug() << "Read" << tocDataSize << "bytes of TOC data";

    if (m_tocEncrypted) {
        qDebug() << "Decrypting TOC...";
        tocData = decryptTOC(tocData);
        if (tocData.isEmpty()) {
            return false;
        }
        qDebug() << "TOC decrypted successfully";
    }

    QDataStream stream(tocData);
    stream.setByteOrder(QDataStream::BigEndian);

    int bNum = (m_header.tocEntrySize - 20) / 2;

    if (bNum < 1 || bNum > 8) {
        m_lastError = QString("Invalid byte width: %1 (entry size: %2)")
        .arg(bNum).arg(m_header.tocEntrySize);
        return false;
    }

    qDebug() << "Byte width for length/offset:" << bNum;

    m_entries.resize(m_header.numFiles);

    for (uint32_t i = 0; i < m_header.numFiles; ++i) {
        stream.skipRawData(16);

        uint32_t zIndex;
        stream >> zIndex;

        uint64_t length = 0;
        for (int j = 0; j < bNum; ++j) {
            uint8_t byte;
            stream >> byte;
            length = (length << 8) | byte;
        }

        uint64_t offset = 0;
        for (int j = 0; j < bNum; ++j) {
            uint8_t byte;
            stream >> byte;
            offset = (offset << 8) | byte;
        }

        m_entries[i].uncompressedSize = length;
        m_entries[i].offset = offset;
        m_entries[i].startChunkIndex = zIndex;

        if (i < 5) {
            qDebug() << "Entry" << i << ": zIndex=" << zIndex
                     << "offset=" << offset << "uncompSize=" << length;
        }
    }

    m_zLengths.clear();
    while (!stream.atEnd()) {
        uint16_t zLen;
        stream >> zLen;
        if (stream.status() != QDataStream::Ok) {
            break;
        }
        m_zLengths.append(zLen);
    }

    qDebug() << "Read" << m_zLengths.size() << "zlength entries";

    qDebug() << "TOC parsing complete";
    return true;
}

bool PsarcFile::readManifest()
{
    if (m_entries.isEmpty()) {
        m_lastError = "No entries in PSARC file";
        return false;
    }

    QByteArray manifestData = extractFileByIndex(0);

    if (manifestData.isEmpty() && m_entries[0].uncompressedSize > 0) {
        m_lastError = QString("Failed to extract manifest: %1").arg(m_lastError);
        return false;
    }

    QString manifestStr = QString::fromUtf8(manifestData);

    qDebug() << "Manifest size:" << manifestData.size() << "bytes";

    QStringList fileNames = manifestStr.split('\n', Qt::SkipEmptyParts);

    qDebug() << "Manifest lists" << fileNames.size() << "files,"
             << "archive has" << m_entries.size() << "entries";

    m_entries[0].name = "NamesBlock.bin";
    m_fileMap["NamesBlock.bin"] = 0;

    int nameIdx = 0;
    for (int i = 1; i < m_entries.size() && nameIdx < fileNames.size(); ++i, ++nameIdx) {
        QString name = fileNames[nameIdx].trimmed();
        m_entries[i].name = name;
        m_fileMap[name] = i;
    }

    qDebug() << "Mapped" << m_fileMap.size() << "file names";

    return true;
}

QByteArray PsarcFile::decompressZlib(const QByteArray& data, uint64_t uncompressedSize, bool& success)
{
    success = false;

    if (data.isEmpty()) {
        return QByteArray();
    }

    QByteArray result(static_cast<int>(uncompressedSize), Qt::Uninitialized);

    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));

    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.constData()));
    strm.avail_out = static_cast<uInt>(uncompressedSize);
    strm.next_out = reinterpret_cast<Bytef*>(result.data());

    // Try with zlib header first
    int ret = inflateInit(&strm);
    if (ret == Z_OK) {
        ret = inflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            inflateEnd(&strm);
            result.resize(static_cast<int>(strm.total_out));
            success = true;
            return result;
        }
        inflateEnd(&strm);
    }

    // Try raw deflate
    std::memset(&strm, 0, sizeof(strm));
    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.constData()));
    strm.avail_out = static_cast<uInt>(uncompressedSize);
    strm.next_out = reinterpret_cast<Bytef*>(result.data());

    ret = inflateInit2(&strm, -MAX_WBITS);
    if (ret == Z_OK) {
        ret = inflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            inflateEnd(&strm);
            result.resize(static_cast<int>(strm.total_out));
            success = true;
            return result;
        }
        inflateEnd(&strm);
    }

    // Try auto-detect
    std::memset(&strm, 0, sizeof(strm));
    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.constData()));
    strm.avail_out = static_cast<uInt>(uncompressedSize);
    strm.next_out = reinterpret_cast<Bytef*>(result.data());

    ret = inflateInit2(&strm, MAX_WBITS + 32);
    if (ret == Z_OK) {
        ret = inflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            inflateEnd(&strm);
            result.resize(static_cast<int>(strm.total_out));
            success = true;
            return result;
        }
        inflateEnd(&strm);
    }

    success = false;
    return QByteArray();
}

QByteArray PsarcFile::decompressLZMA(const QByteArray& data, uint64_t uncompressedSize, bool& success)
{
    success = false;

    if (data.isEmpty()) {
        return QByteArray();
    }

    QByteArray result(static_cast<int>(uncompressedSize), Qt::Uninitialized);

    lzma_stream strm = LZMA_STREAM_INIT;

    lzma_ret ret = lzma_alone_decoder(&strm, UINT64_MAX);
    if (ret != LZMA_OK) {
        return QByteArray();
    }

    strm.next_in = reinterpret_cast<const uint8_t*>(data.constData());
    strm.avail_in = static_cast<size_t>(data.size());
    strm.next_out = reinterpret_cast<uint8_t*>(result.data());
    strm.avail_out = static_cast<size_t>(uncompressedSize);

    ret = lzma_code(&strm, LZMA_FINISH);
    lzma_end(&strm);

    if (ret == LZMA_STREAM_END || ret == LZMA_OK) {
        result.resize(static_cast<int>(uncompressedSize - strm.avail_out));
        success = true;
        return result;
    }

    success = false;
    return QByteArray();
}

QByteArray PsarcFile::extractFileByIndex(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= m_entries.size()) {
        m_lastError = QString("Invalid entry index: %1").arg(entryIndex);
        return QByteArray();
    }

    const FileEntry& entry = m_entries[entryIndex];

    if (entry.uncompressedSize == 0) {
        return QByteArray();
    }

    // Mimic Python's read_entry exactly:
    // filestream.seek(entry['offset'])
    // while len(data) < length:
    //     if zlength[i] == 0:
    //         data += filestream.read(BLOCK_SIZE)
    //     else:
    //         chunk = filestream.read(zlength[i])
    //         try:
    //             data += zlib.decompress(chunk)
    //         except zlib.error:
    //             data += chunk   <-- KEY: on decompression failure, use raw data
    //     i += 1

    QByteArray result;
    result.reserve(static_cast<int>(entry.uncompressedSize));

    m_file->seek(entry.offset);

    uint32_t zIndex = entry.startChunkIndex;

    while (static_cast<uint64_t>(result.size()) < entry.uncompressedSize) {
        if (zIndex >= static_cast<uint32_t>(m_zLengths.size())) {
            m_lastError = QString("zIndex %1 out of range (max %2)")
            .arg(zIndex).arg(m_zLengths.size());
            return QByteArray();
        }

        uint16_t zLen = m_zLengths[zIndex];

        if (zLen == 0) {
            // Uncompressed full block - read BLOCK_SIZE bytes
            QByteArray blockData = m_file->read(m_header.blockSize);
            result.append(blockData);
        } else {
            // Read zLen bytes
            QByteArray chunk = m_file->read(zLen);

            // Try to decompress
            QString compressionStr = QString::fromLatin1(m_header.compressionMethod, 4);
            bool decompressSuccess = false;
            QByteArray decompressedData;

            // Calculate expected uncompressed size for this block
            uint64_t remaining = entry.uncompressedSize - result.size();
            uint64_t expectedSize = qMin(remaining, static_cast<uint64_t>(m_header.blockSize));

            if (compressionStr == "zlib") {
                decompressedData = decompressZlib(chunk, expectedSize, decompressSuccess);
            } else if (compressionStr == "lzma") {
                decompressedData = decompressLZMA(chunk, expectedSize, decompressSuccess);
            }

            if (decompressSuccess) {
                result.append(decompressedData);
            } else {
                // On decompression failure, use raw chunk data (like Python does)
                result.append(chunk);
            }
        }

        zIndex++;
    }

    // Truncate to exact size (in case we read too much)
    if (static_cast<uint64_t>(result.size()) > entry.uncompressedSize) {
        result.resize(static_cast<int>(entry.uncompressedSize));
    }

    // Post-process SNG files
    if (!entry.name.isEmpty()) {
        if (entry.name.contains("songs/bin/generic/") && entry.name.endsWith(".sng")) {
            result = decryptSNG(result);
        }
    }

    return result;
}

QStringList PsarcFile::getFileList() const
{
    QStringList files;
    files.reserve(m_entries.size());

    for (const auto& entry : m_entries) {
        if (!entry.name.isEmpty()) {
            files.append(entry.name);
        }
    }

    return files;
}

bool PsarcFile::fileExists(const QString& fileName) const
{
    return m_fileMap.contains(fileName);
}

QByteArray PsarcFile::extractFile(const QString& fileName)
{
    auto it = m_fileMap.find(fileName);
    if (it == m_fileMap.end()) {
        m_lastError = QString("File not found: %1").arg(fileName);
        return QByteArray();
    }

    return extractFileByIndex(it.value());
}

bool PsarcFile::extractFileTo(const QString& fileName, const QString& outputPath)
{
    QByteArray data = extractFile(fileName);

    if (data.isEmpty() && !fileExists(fileName)) {
        return false;
    }

    QFile outFile(outputPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        m_lastError = QString("Failed to create output file: %1").arg(outFile.errorString());
        return false;
    }

    qint64 written = outFile.write(data);
    outFile.close();

    if (written != data.size()) {
        m_lastError = QString("Failed to write all data: wrote %1 of %2 bytes")
        .arg(written).arg(data.size());
        return false;
    }

    return true;
}

bool PsarcFile::extractAll(const QString& outputDirectory)
{
    QDir baseDir(outputDirectory);

    if (!baseDir.exists()) {
        if (!baseDir.mkpath(".")) {
            m_lastError = "Failed to create output directory";
            return false;
        }
    }

    qDebug() << "Extracting" << m_entries.size() << "files to" << outputDirectory;

    int extracted = 0;
    int failed = 0;

    for (int i = 0; i < m_entries.size(); ++i) {
        const FileEntry& entry = m_entries[i];

        if (entry.name.isEmpty()) {
            qDebug() << "Skipping entry" << i << "- no name";
            continue;
        }

        QString relativePath = entry.name;
        relativePath.replace('/', QDir::separator());

        QString outputPath = baseDir.filePath(relativePath);

        QFileInfo fileInfo(outputPath);
        QDir parentDir = fileInfo.dir();

        if (!parentDir.exists()) {
            if (!parentDir.mkpath(".")) {
                qWarning() << "Failed to create directory:" << parentDir.path();
                failed++;
                continue;
            }
        }

        QByteArray data = extractFileByIndex(i);

        if (data.isEmpty() && entry.uncompressedSize > 0) {
            qWarning() << "Failed to extract" << entry.name << ":" << m_lastError;
            failed++;
            continue;
        }

        QFile outFile(outputPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to write" << outputPath << ":" << outFile.errorString();
            failed++;
            continue;
        }

        outFile.write(data);
        outFile.close();

        extracted++;

        if (extracted % 50 == 0) {
            qDebug() << "Progress:" << extracted << "files extracted...";
        }
    }

    qDebug() << "Extraction complete:" << extracted << "succeeded," << failed << "failed";

    return (failed == 0);
}
