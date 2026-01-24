#include "psarcfile.h"
#include <filesystem>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <zlib.h>
#include <openssl/evp.h>

namespace fs = std::filesystem;

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

PsarcFile::PsarcFile(const std::string& filePath)
    : m_filePath(filePath)
    , m_isOpen(false)
    , m_tocEncrypted(false)
{
}

PsarcFile::~PsarcFile()
{
    close();
}

void PsarcFile::log(const std::string& message) const
{
    if (m_logCallback) {
        m_logCallback(message);
    }
}

uint16_t PsarcFile::readBigEndian16()
{
    uint8_t bytes[2];
    m_file->read(reinterpret_cast<char*>(bytes), 2);
    return (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
}

uint32_t PsarcFile::readBigEndian32()
{
    uint8_t bytes[4];
    m_file->read(reinterpret_cast<char*>(bytes), 4);
    return (static_cast<uint32_t>(bytes[0]) << 24) |
           (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) |
           bytes[3];
}

uint32_t PsarcFile::readLittleEndian32()
{
    uint8_t bytes[4];
    m_file->read(reinterpret_cast<char*>(bytes), 4);
    return bytes[0] |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

bool PsarcFile::open()
{
    if (m_isOpen) {
        return true;
    }

    m_file = std::make_unique<std::ifstream>(m_filePath, std::ios::binary);

    if (!m_file->is_open()) {
        m_lastError = "Failed to open file: " + m_filePath;
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
    if (index < 0 || index >= static_cast<int>(m_entries.size())) {
        return nullptr;
    }
    return &m_entries[index];
}

const PsarcFile::FileEntry* PsarcFile::getEntry(const std::string& fileName) const
{
    auto it = m_fileMap.find(fileName);
    if (it == m_fileMap.end()) {
        return nullptr;
    }
    return &m_entries[it->second];
}

bool PsarcFile::readHeader()
{
    m_file->seekg(0);

    m_header.magic = readBigEndian32();

    if (m_header.magic != 0x50534152) {
        std::ostringstream oss;
        oss << "Invalid PSARC file: wrong magic number (got 0x"
            << std::hex << std::setfill('0') << std::setw(8) << m_header.magic
            << ", expected 0x50534152)";
        m_lastError = oss.str();
        return false;
    }

    m_header.versionMajor = readBigEndian16();
    m_header.versionMinor = readBigEndian16();
    m_file->read(m_header.compressionMethod, 4);
    m_header.tocLength = readBigEndian32();
    m_header.tocEntrySize = readBigEndian32();
    m_header.numFiles = readBigEndian32();
    m_header.blockSize = readBigEndian32();
    m_header.archiveFlags = readBigEndian32();

    m_tocEncrypted = (m_header.archiveFlags & 0x04) != 0;

    if (m_header.versionMajor != 1 || m_header.versionMinor != 4) {
        std::ostringstream oss;
        oss << "Unsupported PSARC version: " << m_header.versionMajor
            << "." << m_header.versionMinor << " (expected 1.4)";
        m_lastError = oss.str();
        return false;
    }

    std::string compressionStr(m_header.compressionMethod, 4);

    std::ostringstream logMsg;
    logMsg << "PSARC Header:\n"
           << "  Magic: PSAR\n"
           << "  Version: " << m_header.versionMajor << "." << m_header.versionMinor << "\n"
           << "  Compression: " << compressionStr << "\n"
           << "  TOC Length: " << m_header.tocLength << "\n"
           << "  TOC Entry Size: " << m_header.tocEntrySize << "\n"
           << "  Num Files: " << m_header.numFiles << "\n"
           << "  Block Size: " << m_header.blockSize << "\n"
           << "  Archive Flags: 0x" << std::hex << m_header.archiveFlags << std::dec << "\n"
           << "  TOC Encrypted: " << (m_tocEncrypted ? "true" : "false");
    log(logMsg.str());

    return true;
}

std::vector<uint8_t> PsarcFile::decryptTOC(const std::vector<uint8_t>& encryptedData)
{
    if (encryptedData.empty()) {
        return {};
    }

    size_t paddedSize = encryptedData.size();
    size_t remainder = paddedSize % 16;
    if (remainder != 0) {
        paddedSize += (16 - remainder);
    }

    std::vector<uint8_t> paddedInput = encryptedData;
    if (paddedInput.size() < paddedSize) {
        paddedInput.resize(paddedSize, 0);
    }

    std::vector<uint8_t> decrypted(paddedSize);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        m_lastError = "Failed to create cipher context";
        return {};
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cfb128(), nullptr, PSARC_KEY, PSARC_IV) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        m_lastError = "Failed to initialize AES decryption";
        return {};
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int len = 0;
    if (EVP_DecryptUpdate(ctx, decrypted.data(), &len,
                          paddedInput.data(), static_cast<int>(paddedInput.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        m_lastError = "Failed to decrypt TOC";
        return {};
    }

    int finalLen = 0;
    EVP_DecryptFinal_ex(ctx, decrypted.data() + len, &finalLen);

    EVP_CIPHER_CTX_free(ctx);

    decrypted.resize(encryptedData.size());
    return decrypted;
}

std::vector<uint8_t> PsarcFile::decryptSNG(const std::vector<uint8_t>& encryptedData)
{
    if (encryptedData.size() < 24) {
        m_lastError = "SNG data too short";
        return {};
    }

    // Read header (little-endian)
    uint32_t magic = encryptedData[0] |
                     (static_cast<uint32_t>(encryptedData[1]) << 8) |
                     (static_cast<uint32_t>(encryptedData[2]) << 16) |
                     (static_cast<uint32_t>(encryptedData[3]) << 24);

    if (magic != 0x4A) {
        m_lastError = "Invalid SNG magic";
        return {};
    }

    uint32_t flags = encryptedData[4] |
                     (static_cast<uint32_t>(encryptedData[5]) << 8) |
                     (static_cast<uint32_t>(encryptedData[6]) << 16) |
                     (static_cast<uint32_t>(encryptedData[7]) << 24);

    std::vector<uint8_t> iv(encryptedData.begin() + 8, encryptedData.begin() + 24);
    std::vector<uint8_t> payload(encryptedData.begin() + 24, encryptedData.end());

    std::vector<uint8_t> decrypted(payload.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return {};
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, SNG_KEY, iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int len = 0;
    if (EVP_DecryptUpdate(ctx, decrypted.data(), &len,
                          payload.data(), static_cast<int>(payload.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    int finalLen = 0;
    EVP_DecryptFinal_ex(ctx, decrypted.data() + len, &finalLen);

    EVP_CIPHER_CTX_free(ctx);

    decrypted.resize(len + finalLen);

    if (flags & 0x01) {
        if (decrypted.size() < 4) {
            return {};
        }

        uint32_t uncompressedSize = decrypted[0] |
                                    (static_cast<uint32_t>(decrypted[1]) << 8) |
                                    (static_cast<uint32_t>(decrypted[2]) << 16) |
                                    (static_cast<uint32_t>(decrypted[3]) << 24);

        std::vector<uint8_t> compressedData(decrypted.begin() + 4, decrypted.end());
        bool success = false;
        std::vector<uint8_t> uncompressed = decompressZlib(compressedData, uncompressedSize, success);

        return uncompressed;
    }

    return decrypted;
}

bool PsarcFile::readTOC()
{
    log("TOC encrypted: " + std::string(m_tocEncrypted ? "true" : "false"));

    m_file->seekg(32);

    int64_t tocDataSize = m_header.tocLength - 32;
    std::vector<uint8_t> tocData(tocDataSize);
    m_file->read(reinterpret_cast<char*>(tocData.data()), tocDataSize);

    if (m_file->gcount() != tocDataSize) {
        std::ostringstream oss;
        oss << "Failed to read TOC data: expected " << tocDataSize
            << " bytes, got " << m_file->gcount();
        m_lastError = oss.str();
        return false;
    }

    log("Read " + std::to_string(tocDataSize) + " bytes of TOC data");

    if (m_tocEncrypted) {
        log("Decrypting TOC...");
        tocData = decryptTOC(tocData);
        if (tocData.empty()) {
            return false;
        }
        log("TOC decrypted successfully");
    }

    // Parse TOC data
    size_t pos = 0;

    auto readBE16 = [&]() -> uint16_t {
        uint16_t val = (static_cast<uint16_t>(tocData[pos]) << 8) | tocData[pos + 1];
        pos += 2;
        return val;
    };

    auto readBE32 = [&]() -> uint32_t {
        uint32_t val = (static_cast<uint32_t>(tocData[pos]) << 24) |
                       (static_cast<uint32_t>(tocData[pos + 1]) << 16) |
                       (static_cast<uint32_t>(tocData[pos + 2]) << 8) |
                       tocData[pos + 3];
        pos += 4;
        return val;
    };

    int bNum = (m_header.tocEntrySize - 20) / 2;

    if (bNum < 1 || bNum > 8) {
        std::ostringstream oss;
        oss << "Invalid byte width: " << bNum << " (entry size: " << m_header.tocEntrySize << ")";
        m_lastError = oss.str();
        return false;
    }

    log("Byte width for length/offset: " + std::to_string(bNum));

    m_entries.resize(m_header.numFiles);

    for (uint32_t i = 0; i < m_header.numFiles; ++i) {
        pos += 16; // Skip MD5 hash

        uint32_t zIndex = readBE32();

        uint64_t length = 0;
        for (int j = 0; j < bNum; ++j) {
            length = (length << 8) | tocData[pos++];
        }

        uint64_t offset = 0;
        for (int j = 0; j < bNum; ++j) {
            offset = (offset << 8) | tocData[pos++];
        }

        m_entries[i].uncompressedSize = length;
        m_entries[i].offset = offset;
        m_entries[i].startChunkIndex = zIndex;

        if (i < 5) {
            std::ostringstream oss;
            oss << "Entry " << i << ": zIndex=" << zIndex
                << " offset=" << offset << " uncompSize=" << length;
            log(oss.str());
        }
    }

    m_zLengths.clear();
    while (pos + 1 < tocData.size()) {
        uint16_t zLen = readBE16();
        m_zLengths.push_back(zLen);
    }

    log("Read " + std::to_string(m_zLengths.size()) + " zlength entries");
    log("TOC parsing complete");

    return true;
}

bool PsarcFile::readManifest()
{
    if (m_entries.empty()) {
        m_lastError = "No entries in PSARC file";
        return false;
    }

    std::vector<uint8_t> manifestData = extractFileByIndex(0);

    if (manifestData.empty() && m_entries[0].uncompressedSize > 0) {
        m_lastError = "Failed to extract manifest: " + m_lastError;
        return false;
    }

    std::string manifestStr(manifestData.begin(), manifestData.end());

    log("Manifest size: " + std::to_string(manifestData.size()) + " bytes");

    // Split by newlines
    std::vector<std::string> fileNames;
    std::istringstream iss(manifestStr);
    std::string line;
    while (std::getline(iss, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        size_t end = line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            fileNames.push_back(line.substr(start, end - start + 1));
        }
    }

    log("Manifest lists " + std::to_string(fileNames.size()) + " files, archive has " +
        std::to_string(m_entries.size()) + " entries");

    m_entries[0].name = "NamesBlock.bin";
    m_fileMap["NamesBlock.bin"] = 0;

    size_t nameIdx = 0;
    for (size_t i = 1; i < m_entries.size() && nameIdx < fileNames.size(); ++i, ++nameIdx) {
        m_entries[i].name = fileNames[nameIdx];
        m_fileMap[fileNames[nameIdx]] = static_cast<int>(i);
    }

    log("Mapped " + std::to_string(m_fileMap.size()) + " file names");

    return true;
}

std::vector<uint8_t> PsarcFile::decompressZlib(const std::vector<uint8_t>& data, uint64_t uncompressedSize, bool& success)
{
    success = false;

    if (data.empty()) {
        return {};
    }

    std::vector<uint8_t> result(uncompressedSize);

    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));

    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_out = static_cast<uInt>(uncompressedSize);
    strm.next_out = result.data();

    // Try with zlib header first
    int ret = inflateInit(&strm);
    if (ret == Z_OK) {
        ret = inflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            inflateEnd(&strm);
            result.resize(strm.total_out);
            success = true;
            return result;
        }
        inflateEnd(&strm);
    }

    // Try raw deflate
    std::memset(&strm, 0, sizeof(strm));
    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_out = static_cast<uInt>(uncompressedSize);
    strm.next_out = result.data();

    ret = inflateInit2(&strm, -MAX_WBITS);
    if (ret == Z_OK) {
        ret = inflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            inflateEnd(&strm);
            result.resize(strm.total_out);
            success = true;
            return result;
        }
        inflateEnd(&strm);
    }

    // Try auto-detect
    std::memset(&strm, 0, sizeof(strm));
    strm.avail_in = static_cast<uInt>(data.size());
    strm.next_in = const_cast<Bytef*>(data.data());
    strm.avail_out = static_cast<uInt>(uncompressedSize);
    strm.next_out = result.data();

    ret = inflateInit2(&strm, MAX_WBITS + 32);
    if (ret == Z_OK) {
        ret = inflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_END) {
            inflateEnd(&strm);
            result.resize(strm.total_out);
            success = true;
            return result;
        }
        inflateEnd(&strm);
    }

    success = false;
    return {};
}

std::vector<uint8_t> PsarcFile::extractFileByIndex(int entryIndex)
{
    if (entryIndex < 0 || entryIndex >= static_cast<int>(m_entries.size())) {
        m_lastError = "Invalid entry index: " + std::to_string(entryIndex);
        return {};
    }

    const FileEntry& entry = m_entries[entryIndex];

    if (entry.uncompressedSize == 0) {
        return {};
    }

    std::vector<uint8_t> result;
    result.reserve(entry.uncompressedSize);

    m_file->seekg(entry.offset);

    uint32_t zIndex = entry.startChunkIndex;

    while (result.size() < entry.uncompressedSize) {
        if (zIndex >= m_zLengths.size()) {
            std::ostringstream oss;
            oss << "zIndex " << zIndex << " out of range (max " << m_zLengths.size() << ")";
            m_lastError = oss.str();
            return {};
        }

        uint16_t zLen = m_zLengths[zIndex];

        if (zLen == 0) {
            // Uncompressed full block - read BLOCK_SIZE bytes
            std::vector<uint8_t> blockData(m_header.blockSize);
            m_file->read(reinterpret_cast<char*>(blockData.data()), m_header.blockSize);
            auto bytesRead = m_file->gcount();
            blockData.resize(bytesRead);
            result.insert(result.end(), blockData.begin(), blockData.end());
        } else {
            // Read zLen bytes
            std::vector<uint8_t> chunk(zLen);
            m_file->read(reinterpret_cast<char*>(chunk.data()), zLen);

            // Try to decompress
            std::string compressionStr(m_header.compressionMethod, 4);
            bool decompressSuccess = false;
            std::vector<uint8_t> decompressedData;

            // Calculate expected uncompressed size for this block
            uint64_t remaining = entry.uncompressedSize - result.size();
            uint64_t expectedSize = std::min(remaining, static_cast<uint64_t>(m_header.blockSize));

            if (compressionStr == "zlib") {
                decompressedData = decompressZlib(chunk, expectedSize, decompressSuccess);
            } else {
                m_lastError = "Unsupported compression method: " + compressionStr;
                return {};
            }

            if (decompressSuccess) {
                result.insert(result.end(), decompressedData.begin(), decompressedData.end());
            } else {
                // On decompression failure, use raw chunk data (like Python does)
                result.insert(result.end(), chunk.begin(), chunk.end());
            }
        }

        zIndex++;
    }

    // Truncate to exact size (in case we read too much)
    if (result.size() > entry.uncompressedSize) {
        result.resize(entry.uncompressedSize);
    }

    // Post-process SNG files
    if (!entry.name.empty()) {
        if (entry.name.find("songs/bin/generic/") != std::string::npos &&
            entry.name.size() >= 4 &&
            entry.name.substr(entry.name.size() - 4) == ".sng") {
            result = decryptSNG(result);
        }
    }

    return result;
}

std::vector<std::string> PsarcFile::getFileList() const
{
    std::vector<std::string> files;
    files.reserve(m_entries.size());

    for (const auto& entry : m_entries) {
        if (!entry.name.empty()) {
            files.push_back(entry.name);
        }
    }

    return files;
}

bool PsarcFile::fileExists(const std::string& fileName) const
{
    return m_fileMap.find(fileName) != m_fileMap.end();
}

std::vector<uint8_t> PsarcFile::extractFile(const std::string& fileName)
{
    auto it = m_fileMap.find(fileName);
    if (it == m_fileMap.end()) {
        m_lastError = "File not found: " + fileName;
        return {};
    }

    return extractFileByIndex(it->second);
}

bool PsarcFile::extractFileTo(const std::string& fileName, const std::string& outputPath)
{
    std::vector<uint8_t> data = extractFile(fileName);

    if (data.empty() && !fileExists(fileName)) {
        return false;
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        m_lastError = "Failed to create output file: " + outputPath;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(data.data()), data.size());

    if (!outFile.good()) {
        m_lastError = "Failed to write all data to file";
        return false;
    }

    outFile.close();
    return true;
}

bool PsarcFile::extractAll(const std::string& outputDirectory)
{
    fs::path baseDir(outputDirectory);

    if (!fs::exists(baseDir)) {
        std::error_code ec;
        if (!fs::create_directories(baseDir, ec)) {
            m_lastError = "Failed to create output directory";
            return false;
        }
    }

    log("Extracting " + std::to_string(m_entries.size()) + " files to " + outputDirectory);

    int extracted = 0;
    int failed = 0;

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const FileEntry& entry = m_entries[i];

        if (entry.name.empty()) {
            log("Skipping entry " + std::to_string(i) + " - no name");
            continue;
        }

        // Convert forward slashes to native path separators
        std::string relativePath = entry.name;
#ifdef _WIN32
        std::replace(relativePath.begin(), relativePath.end(), '/', '\\');
#endif

        fs::path outputPath = baseDir / relativePath;

        fs::path parentDir = outputPath.parent_path();

        if (!fs::exists(parentDir)) {
            std::error_code ec;
            if (!fs::create_directories(parentDir, ec)) {
                log("Failed to create directory: " + parentDir.string());
                failed++;
                continue;
            }
        }

        std::vector<uint8_t> data = extractFileByIndex(static_cast<int>(i));

        if (data.empty() && entry.uncompressedSize > 0) {
            log("Failed to extract " + entry.name + ": " + m_lastError);
            failed++;
            continue;
        }

        std::ofstream outFile(outputPath, std::ios::binary);
        if (!outFile.is_open()) {
            log("Failed to write " + outputPath.string());
            failed++;
            continue;
        }

        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        outFile.close();

        extracted++;

        if (extracted % 50 == 0) {
            log("Progress: " + std::to_string(extracted) + " files extracted...");
        }
    }

    log("Extraction complete: " + std::to_string(extracted) + " succeeded, " +
        std::to_string(failed) + " failed");

    return (failed == 0);
}
