#include "psarcfile.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <format>
#include <sstream>

#include <openssl/evp.h>
#include <zlib.h>

namespace fs = std::filesystem;

static constexpr std::array<uint8_t, 32> PSARC_KEY = {
    0xC5, 0x3D, 0xB2, 0x38, 0x70, 0xA1, 0xA2, 0xF7,
    0x1C, 0xAE, 0x64, 0x06, 0x1F, 0xDD, 0x0E, 0x11,
    0x57, 0x30, 0x9D, 0xC8, 0x52, 0x04, 0xD4, 0xC5,
    0xBF, 0xDF, 0x25, 0x09, 0x0D, 0xF2, 0x57, 0x2C
};

static constexpr std::array<uint8_t, 16> PSARC_IV = {
    0xE9, 0x15, 0xAA, 0x01, 0x8F, 0xEF, 0x71, 0xFC,
    0x50, 0x81, 0x32, 0xE4, 0xBB, 0x4C, 0xEB, 0x42
};

static constexpr std::array<uint8_t, 32> SNG_KEY = {
    0xCB, 0x64, 0x8D, 0xF3, 0xD1, 0x2A, 0x16, 0xBF,
    0x71, 0x70, 0x14, 0x14, 0xE6, 0x96, 0x19, 0xEC,
    0x17, 0x1C, 0xCA, 0x5D, 0x2A, 0x14, 0x2E, 0x3E,
    0x59, 0xDE, 0x7A, 0xDD, 0xA1, 0x8A, 0x3A, 0x30
};

static constexpr uint32_t PSARC_MAGIC = 0x50534152;
static constexpr uint32_t SNG_MAGIC = 0x4A;
static constexpr uint32_t TOC_ENCRYPTED_FLAG = 0x04;
static constexpr uint32_t SNG_COMPRESSED_FLAG = 0x01;

[[nodiscard]] static constexpr uint32_t readLE32(const uint8_t* data) noexcept
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

PsarcFile::PsarcFile(const std::string& filePath)
    : m_filePath(filePath)
{
}

PsarcFile::~PsarcFile()
{
    close();
}

uint16_t PsarcFile::readBigEndian16()
{
    std::array<uint8_t, 2> bytes{};
    m_file->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    return static_cast<uint16_t>((bytes[0] << 8) | bytes[1]);
}

uint32_t PsarcFile::readBigEndian32()
{
    std::array<uint8_t, 4> bytes{};
    m_file->read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    return (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
}

void PsarcFile::open()
{
    if (m_isOpen)
    {
        return;
    }

    m_file = std::make_unique<std::ifstream>(m_filePath, std::ios::binary);
    if (!m_file->is_open())
    {
        throw PsarcException(std::format("Failed to open file: {}", m_filePath));
    }

    readHeader();
    readTOC();
    readManifest();
    m_isOpen = true;
}

void PsarcFile::close()
{
    if (m_file)
    {
        m_file->close();
        m_file.reset();
    }
    m_entries.clear();
    m_fileMap.clear();
    m_zLengths.clear();
    m_isOpen = false;
}

const PsarcFile::FileEntry* PsarcFile::getEntry(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        return nullptr;
    }
    return &m_entries[index];
}

const PsarcFile::FileEntry* PsarcFile::getEntry(const std::string& fileName) const
{
    const auto it = m_fileMap.find(fileName);
    return (it != m_fileMap.end()) ? &m_entries[it->second] : nullptr;
}

void PsarcFile::readHeader()
{
    m_file->seekg(0);
    m_header.magic = readBigEndian32();

    if (m_header.magic != PSARC_MAGIC)
    {
        throw PsarcException("Invalid PSARC file: wrong magic number");
    }

    m_header.versionMajor = readBigEndian16();
    m_header.versionMinor = readBigEndian16();
    m_file->read(m_header.compressionMethod, sizeof(m_header.compressionMethod));
    m_header.tocLength = readBigEndian32();
    m_header.tocEntrySize = readBigEndian32();
    m_header.numFiles = readBigEndian32();
    m_header.blockSize = readBigEndian32();
    m_header.archiveFlags = readBigEndian32();

    if (m_header.versionMajor != 1 || m_header.versionMinor != 4)
    {
        throw PsarcException(std::format(
            "Unsupported PSARC version: {}.{}", m_header.versionMajor, m_header.versionMinor));
    }
}

std::vector<uint8_t> PsarcFile::decryptTOC(const std::vector<uint8_t>& data)
{
    if (data.empty())
    {
        return {};
    }

    const size_t paddedSize = ((data.size() + 15) / 16) * 16;
    std::vector<uint8_t> input = data;
    input.resize(paddedSize, 0);

    std::vector<uint8_t> output(paddedSize);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        throw PsarcException("Failed to create cipher context");
    }

    int len = 0;
    bool success = EVP_DecryptInit_ex(ctx, EVP_aes_256_cfb128(), nullptr,
                                      PSARC_KEY.data(), PSARC_IV.data()) == 1;
    if (success)
    {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        success = EVP_DecryptUpdate(ctx, output.data(), &len,
                                    input.data(), static_cast<int>(input.size())) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!success)
    {
        throw PsarcException("Failed to decrypt TOC");
    }

    output.resize(data.size());
    return output;
}

std::vector<uint8_t> PsarcFile::decryptSNG(const std::vector<uint8_t>& data)
{
    if (data.size() < 24)
    {
        throw PsarcException("SNG data too short");
    }
    if (readLE32(data.data()) != SNG_MAGIC)
    {
        throw PsarcException("Invalid SNG magic");
    }

    const uint32_t flags = readLE32(data.data() + 4);
    const uint8_t* iv = data.data() + 8;
    std::vector<uint8_t> payload(data.begin() + 24, data.end());
    std::vector<uint8_t> decrypted(payload.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        throw PsarcException("Failed to create cipher context");
    }

    int len = 0;
    bool success = EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, SNG_KEY.data(), iv) == 1;
    if (success)
    {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        success = EVP_DecryptUpdate(ctx, decrypted.data(), &len,
                                    payload.data(), static_cast<int>(payload.size())) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!success)
    {
        throw PsarcException("Failed to decrypt SNG");
    }

    decrypted.resize(static_cast<size_t>(len));

    if (flags & SNG_COMPRESSED_FLAG)
    {
        const uint32_t uncompSize = readLE32(decrypted.data());
        std::vector<uint8_t> compressed(decrypted.begin() + 4, decrypted.end());
        return decompressZlib(compressed, uncompSize);
    }

    return decrypted;
}

void PsarcFile::readTOC()
{
    const bool encrypted = (m_header.archiveFlags & TOC_ENCRYPTED_FLAG) != 0;

    m_file->seekg(32);
    std::vector<uint8_t> tocData(m_header.tocLength - 32);
    m_file->read(reinterpret_cast<char*>(tocData.data()), static_cast<std::streamsize>(tocData.size()));

    if (encrypted)
    {
        tocData = decryptTOC(tocData);
    }

    const int bNum = (static_cast<int>(m_header.tocEntrySize) - 20) / 2;
    if (bNum < 1 || bNum > 8)
    {
        throw PsarcException("Invalid TOC entry size");
    }

    size_t pos = 0;

    auto readBE16 = [&]() -> uint16_t
    {
        uint16_t val = static_cast<uint16_t>((tocData[pos] << 8) | tocData[pos + 1]);
        pos += 2;
        return val;
    };

    auto readBE32 = [&]() -> uint32_t
    {
        uint32_t val = (tocData[pos] << 24) | (tocData[pos + 1] << 16) |
                       (tocData[pos + 2] << 8) | tocData[pos + 3];
        pos += 4;
        return val;
    };

    m_entries.resize(m_header.numFiles);

    for (uint32_t i = 0; i < m_header.numFiles; ++i)
    {
        pos += 16; // Skip MD5

        m_entries[i].startChunkIndex = readBE32();

        uint64_t length = 0;
        uint64_t offset = 0;
        for (int j = 0; j < bNum; ++j)
        {
            length = (length << 8) | tocData[pos++];
        }
        for (int j = 0; j < bNum; ++j)
        {
            offset = (offset << 8) | tocData[pos++];
        }

        m_entries[i].uncompressedSize = length;
        m_entries[i].offset = offset;
    }

    while (pos + 1 < tocData.size())
    {
        m_zLengths.push_back(readBE16());
    }
}

void PsarcFile::readManifest()
{
    if (m_entries.empty())
    {
        throw PsarcException("No entries in PSARC");
    }

    const auto manifestData = extractFileByIndex(0);
    const std::string manifest(manifestData.begin(), manifestData.end());

    std::vector<std::string> names;
    std::istringstream iss(manifest);
    std::string line;

    while (std::getline(iss, line))
    {
        const size_t start = line.find_first_not_of(" \t\r\n");
        const size_t end = line.find_last_not_of(" \t\r\n");
        if (start != std::string::npos)
        {
            names.push_back(line.substr(start, end - start + 1));
        }
    }

    m_entries[0].name = "NamesBlock.bin";
    m_fileMap["NamesBlock.bin"] = 0;

    for (size_t i = 1; i < m_entries.size() && i - 1 < names.size(); ++i)
    {
        m_entries[i].name = names[i - 1];
        m_fileMap[names[i - 1]] = static_cast<int>(i);
    }
}

std::vector<uint8_t> PsarcFile::decompressZlib(const std::vector<uint8_t>& data, uint64_t uncompressedSize)
{
    if (data.empty())
    {
        return {};
    }

    std::vector<uint8_t> result(uncompressedSize);

    constexpr std::array windowBits = {MAX_WBITS, -MAX_WBITS, MAX_WBITS + 32};

    for (const int wb : windowBits)
    {
        z_stream strm{};
        strm.avail_in = static_cast<uInt>(data.size());
        strm.next_in = const_cast<Bytef*>(data.data());
        strm.avail_out = static_cast<uInt>(uncompressedSize);
        strm.next_out = result.data();

        if (inflateInit2(&strm, wb) == Z_OK)
        {
            if (inflate(&strm, Z_FINISH) == Z_STREAM_END)
            {
                inflateEnd(&strm);
                result.resize(strm.total_out);
                return result;
            }
            inflateEnd(&strm);
        }
    }

    return {};
}

std::vector<uint8_t> PsarcFile::extractFileByIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_entries.size()))
    {
        throw PsarcException(std::format("Invalid entry index: {}", index));
    }

    const auto& entry = m_entries[index];
    if (entry.uncompressedSize == 0)
    {
        return {};
    }

    std::vector<uint8_t> result;
    result.reserve(static_cast<size_t>(entry.uncompressedSize));
    m_file->seekg(static_cast<std::streamoff>(entry.offset));

    uint32_t zIndex = entry.startChunkIndex;

    while (result.size() < entry.uncompressedSize)
    {
        if (zIndex >= m_zLengths.size())
        {
            throw PsarcException("Chunk index out of range");
        }

        const uint16_t zLen = m_zLengths[zIndex++];

        if (zLen == 0)
        {
            std::vector<uint8_t> block(m_header.blockSize);
            m_file->read(reinterpret_cast<char*>(block.data()), static_cast<std::streamsize>(m_header.blockSize));
            block.resize(static_cast<size_t>(m_file->gcount()));
            result.insert(result.end(), block.begin(), block.end());
        }
        else
        {
            std::vector<uint8_t> chunk(zLen);
            m_file->read(reinterpret_cast<char*>(chunk.data()), zLen);

            const uint64_t remaining = entry.uncompressedSize - result.size();
            const uint64_t expectedSize = std::min(remaining, static_cast<uint64_t>(m_header.blockSize));

            auto decompressed = decompressZlib(chunk, expectedSize);
            if (!decompressed.empty())
            {
                result.insert(result.end(), decompressed.begin(), decompressed.end());
            }
            else
            {
                result.insert(result.end(), chunk.begin(), chunk.end());
            }
        }
    }

    result.resize(std::min(result.size(), static_cast<size_t>(entry.uncompressedSize)));

    if (entry.name.find("songs/bin/generic/") != std::string::npos &&
        entry.name.ends_with(".sng"))
    {
        result = decryptSNG(result);
    }

    return result;
}

std::vector<std::string> PsarcFile::getFileList() const
{
    std::vector<std::string> files;
    files.reserve(m_entries.size());

    for (const auto& entry : m_entries)
    {
        if (!entry.name.empty())
        {
            files.push_back(entry.name);
        }
    }

    return files;
}

bool PsarcFile::fileExists(const std::string& fileName) const
{
    return m_fileMap.contains(fileName);
}

std::vector<uint8_t> PsarcFile::extractFile(const std::string& fileName)
{
    const auto it = m_fileMap.find(fileName);
    if (it == m_fileMap.end())
    {
        throw PsarcException(std::format("File not found: {}", fileName));
    }
    return extractFileByIndex(it->second);
}

void PsarcFile::extractFileTo(const std::string& fileName, const std::string& outputPath)
{
    const auto data = extractFile(fileName);

    std::ofstream out(outputPath, std::ios::binary);
    if (!out)
    {
        throw PsarcException(std::format("Failed to create file: {}", outputPath));
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

void PsarcFile::extractAll(const std::string& outputDirectory)
{
    fs::create_directories(outputDirectory);

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const auto& entry = m_entries[i];
        if (entry.name.empty())
        {
            continue;
        }

        const fs::path outputPath = fs::path(outputDirectory) / entry.name;
        fs::create_directories(outputPath.parent_path());

        const auto data = extractFileByIndex(static_cast<int>(i));

        if (std::ofstream out(outputPath, std::ios::binary); out)
        {
            out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
    }
}
