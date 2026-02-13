#include "open-psarc/psarc_file.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <sstream>
#include <utility>

#include <lzma.h>
#include <openssl/evp.h>
#include <zlib.h>

namespace fs = std::filesystem;

static constexpr std::array<uint8_t, 32> g_psarc_key = {
    0xC5, 0x3D, 0xB2, 0x38, 0x70, 0xA1, 0xA2, 0xF7, 0x1C, 0xAE, 0x64, 0x06, 0x1F, 0xDD, 0x0E, 0x11,
    0x57, 0x30, 0x9D, 0xC8, 0x52, 0x04, 0xD4, 0xC5, 0xBF, 0xDF, 0x25, 0x09, 0x0D, 0xF2, 0x57, 0x2C};

static constexpr std::array<uint8_t, 16> g_psarc_iv = {
    0xE9, 0x15, 0xAA, 0x01, 0x8F, 0xEF, 0x71, 0xFC, 0x50, 0x81, 0x32, 0xE4, 0xBB, 0x4C, 0xEB, 0x42};

static constexpr std::array<uint8_t, 32> g_sng_key = {
    0xCB, 0x64, 0x8D, 0xF3, 0xD1, 0x2A, 0x16, 0xBF, 0x71, 0x70, 0x14, 0x14, 0xE6, 0x96, 0x19, 0xEC,
    0x17, 0x1C, 0xCA, 0x5D, 0x2A, 0x14, 0x2E, 0x3E, 0x59, 0xDE, 0x7A, 0xDD, 0xA1, 0x8A, 0x3A, 0x30};

static constexpr uint32_t g_psarc_magic = 0x50534152;
static constexpr uint32_t g_sng_magic = 0x4A;
static constexpr uint32_t g_toc_encrypted_flag = 0x04;
static constexpr uint32_t g_sng_compressed_flag = 0x01;

[[nodiscard]] static constexpr uint16_t ReadLE16(const uint8_t* data) noexcept
{
    return static_cast<uint16_t>(data[0] | (data[1] << 8));
}

[[nodiscard]] static constexpr uint32_t ReadLE32(const uint8_t* data) noexcept
{
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

[[nodiscard]] static constexpr uint16_t ReadBE16(const uint8_t* data) noexcept
{
    return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

[[nodiscard]] static constexpr uint32_t ReadBE32(const uint8_t* data) noexcept
{
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

PsarcFile::PsarcFile(std::string file_path) : m_file_path(std::move(file_path))
{
}

PsarcFile::~PsarcFile()
{
    try
    {
        Close();
    }
    catch (...)
    {
        // Prevent exceptions from escaping the destructor (undefined behavior)
    }
}

void PsarcFile::ReadBytes(void* dest, size_t count)
{
    m_file->read(reinterpret_cast<char*>(dest), static_cast<std::streamsize>(count));
    if (!m_file->good() && !m_file->eof())
    {
        throw PsarcException("Failed to read from file");
    }
    if (std::cmp_not_equal(m_file->gcount(), count))
    {
        throw PsarcException(std::format("Unexpected end of file: expected {} bytes, got {}", count,
                                         m_file->gcount()));
    }
}

uint16_t PsarcFile::ReadBigEndian16()
{
    std::array<uint8_t, 2> bytes{};
    ReadBytes(bytes.data(), bytes.size());
    return ReadBE16(bytes.data());
}

uint32_t PsarcFile::ReadBigEndian32()
{
    std::array<uint8_t, 4> bytes{};
    ReadBytes(bytes.data(), bytes.size());
    return ReadBE32(bytes.data());
}

void PsarcFile::Open()
{
    if (m_is_open)
    {
        return;
    }

    m_file = std::make_unique<std::ifstream>(m_file_path, std::ios::binary);
    if (!m_file->is_open())
    {
        throw PsarcException(std::format("Failed to open file: {}", m_file_path));
    }

    ReadHeader();
    ReadToc();
    ReadManifest();
    m_is_open = true;
}

void PsarcFile::Close()
{
    if (m_file)
    {
        m_file->close();
        m_file.reset();
    }
    m_entries.clear();
    m_file_map.clear();
    m_z_lengths.clear();
    m_is_open = false;
}

const PsarcFile::FileEntry* PsarcFile::GetEntry(int index) const
{
    if (index < 0 || std::cmp_greater_equal(index, m_entries.size()))
    {
        return nullptr;
    }
    return &m_entries[index];
}

const PsarcFile::FileEntry* PsarcFile::GetEntry(const std::string& file_name) const
{
    const auto it = m_file_map.find(file_name);
    return (it != m_file_map.end()) ? &m_entries[it->second] : nullptr;
}

void PsarcFile::ReadHeader()
{
    m_file->seekg(0);
    m_header.m_magic = ReadBigEndian32();

    if (m_header.m_magic != g_psarc_magic)
    {
        throw PsarcException("Invalid PSARC file: wrong magic number");
    }

    m_header.m_version_major = ReadBigEndian16();
    m_header.m_version_minor = ReadBigEndian16();
    ReadBytes(m_header.m_compression_method, sizeof(m_header.m_compression_method));
    m_header.m_toc_length = ReadBigEndian32();
    m_header.m_toc_entry_size = ReadBigEndian32();
    m_header.m_num_files = ReadBigEndian32();
    m_header.m_block_size = ReadBigEndian32();
    m_header.m_archive_flags = ReadBigEndian32();

    if (m_header.m_version_major != 1 || m_header.m_version_minor != 4)
    {
        throw PsarcException(std::format("Unsupported PSARC version: {}.{}",
                                         m_header.m_version_major, m_header.m_version_minor));
    }
}

std::vector<uint8_t> PsarcFile::DecryptToc(const std::vector<uint8_t>& data)
{
    if (data.empty())
    {
        return {};
    }

    const size_t padded_size = ((data.size() + 15) / 16) * 16;
    std::vector<uint8_t> input = data;
    input.resize(padded_size, 0);

    std::vector<uint8_t> output(padded_size);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        throw PsarcException("Failed to create cipher context");
    }

    int len = 0;
    bool success = EVP_DecryptInit_ex(ctx, EVP_aes_256_cfb128(), nullptr, g_psarc_key.data(),
                                      g_psarc_iv.data()) == 1;
    if (success)
    {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        success = EVP_DecryptUpdate(ctx, output.data(), &len, input.data(),
                                    static_cast<int>(input.size())) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!success)
    {
        throw PsarcException("Failed to decrypt TOC");
    }

    output.resize(data.size());
    return output;
}

std::vector<uint8_t> PsarcFile::DecryptSng(const std::vector<uint8_t>& data)
{
    if (data.size() < 24)
    {
        throw PsarcException("SNG data too short");
    }
    if (ReadLE32(data.data()) != g_sng_magic)
    {
        throw PsarcException("Invalid SNG magic");
    }

    const uint32_t flags = ReadLE32(data.data() + 4);
    const uint8_t* iv = data.data() + 8;
    std::vector<uint8_t> payload(data.begin() + 24, data.end());
    std::vector<uint8_t> decrypted(payload.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        throw PsarcException("Failed to create cipher context");
    }

    int len = 0;
    bool success = EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr, g_sng_key.data(), iv) == 1;
    if (success)
    {
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        success = EVP_DecryptUpdate(ctx, decrypted.data(), &len, payload.data(),
                                    static_cast<int>(payload.size())) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!success)
    {
        throw PsarcException("Failed to decrypt SNG");
    }

    decrypted.resize(static_cast<size_t>(len));

    if (flags & g_sng_compressed_flag)
    {
        const uint32_t uncomp_size = ReadLE32(decrypted.data());
        std::vector<uint8_t> compressed(decrypted.begin() + 4, decrypted.end());
        return DecompressZlib(compressed, uncomp_size);
    }

    return decrypted;
}

void PsarcFile::ReadToc()
{
    const bool encrypted = (m_header.m_archive_flags & g_toc_encrypted_flag) != 0;

    m_file->seekg(32);
    std::vector<uint8_t> toc_data(m_header.m_toc_length - 32);
    ReadBytes(toc_data.data(), toc_data.size());

    if (encrypted)
    {
        toc_data = DecryptToc(toc_data);
    }

    const int b_num = (static_cast<int>(m_header.m_toc_entry_size) - 20) / 2;
    if (b_num < 1 || b_num > 8)
    {
        throw PsarcException("Invalid TOC entry size");
    }

    size_t pos = 0;

    m_entries.resize(m_header.m_num_files);

    for (uint32_t i = 0; i < m_header.m_num_files; ++i)
    {
        pos += 16; // Skip MD5

        if (pos + 4 > toc_data.size())
        {
            throw PsarcException("TOC data truncated while reading entry");
        }

        m_entries[i].m_start_chunk_index = ReadBE32(toc_data.data() + pos);
        pos += 4;

        if (pos + static_cast<size_t>(b_num * 2) > toc_data.size())
        {
            throw PsarcException("TOC data truncated while reading length/offset");
        }

        uint64_t length = 0;
        uint64_t offset = 0;
        for (int j = 0; j < b_num; ++j)
        {
            length = (length << 8) | toc_data[pos++];
        }
        for (int j = 0; j < b_num; ++j)
        {
            offset = (offset << 8) | toc_data[pos++];
        }

        m_entries[i].m_uncompressed_size = length;
        m_entries[i].m_offset = offset;
    }

    while (pos + 1 < toc_data.size())
    {
        m_z_lengths.push_back(ReadBE16(toc_data.data() + pos));
        pos += 2;
    }
}

void PsarcFile::ReadManifest()
{
    if (m_entries.empty())
    {
        throw PsarcException("No entries in PSARC");
    }

    const auto manifest_data = ExtractFileByIndex(0);
    const std::string manifest(manifest_data.begin(), manifest_data.end());

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

    m_entries[0].m_name = "NamesBlock.bin";
    m_file_map["NamesBlock.bin"] = 0;

    for (size_t i = 1; i < m_entries.size() && i - 1 < names.size(); ++i)
    {
        m_entries[i].m_name = names[i - 1];
        m_file_map[names[i - 1]] = static_cast<int>(i);
    }
}

std::vector<uint8_t> PsarcFile::DecompressZlib(const std::vector<uint8_t>& data,
                                               uint64_t uncompressed_size)
{
    if (data.empty())
    {
        return {};
    }

    std::vector<uint8_t> result(uncompressed_size);

    constexpr std::array window_bits = {MAX_WBITS, -MAX_WBITS, MAX_WBITS + 32};

    for (const int wb : window_bits)
    {
        z_stream strm{};
        strm.avail_in = static_cast<uInt>(data.size());
        strm.next_in = const_cast<Bytef*>(data.data());
        strm.avail_out = static_cast<uInt>(uncompressed_size);
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

std::vector<uint8_t> PsarcFile::DecompressLzma(const std::vector<uint8_t>& data,
                                               uint64_t uncompressed_size)
{
    if (data.empty())
    {
        return {};
    }

    std::vector<uint8_t> result(uncompressed_size);

    lzma_stream strm = LZMA_STREAM_INIT;

    if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK)
    {
        return {};
    }

    strm.next_in = data.data();
    strm.avail_in = data.size();
    strm.next_out = result.data();
    strm.avail_out = uncompressed_size;

    const lzma_ret ret = lzma_code(&strm, LZMA_FINISH);
    lzma_end(&strm);

    if (ret == LZMA_STREAM_END || ret == LZMA_OK)
    {
        result.resize(uncompressed_size - strm.avail_out);
        return result;
    }

    return {};
}

std::vector<uint8_t> PsarcFile::ExtractFileByIndex(int index)
{
    if (index < 0 || std::cmp_greater_equal(index, m_entries.size()))
    {
        throw PsarcException(std::format("Invalid entry index: {}", index));
    }

    const auto& entry = m_entries[index];
    if (entry.m_uncompressed_size == 0)
    {
        return {};
    }

    std::vector<uint8_t> result;
    result.reserve(static_cast<size_t>(entry.m_uncompressed_size));
    m_file->seekg(static_cast<std::streamoff>(entry.m_offset));

    uint32_t z_index = entry.m_start_chunk_index;

    while (result.size() < entry.m_uncompressed_size)
    {
        if (z_index >= m_z_lengths.size())
        {
            throw PsarcException("Chunk index out of range");
        }

        const uint16_t z_len = m_z_lengths[z_index++];

        if (z_len == 0)
        {
            std::vector<uint8_t> block(m_header.m_block_size);
            m_file->read(reinterpret_cast<char*>(block.data()),
                         static_cast<std::streamsize>(m_header.m_block_size));
            const auto bytes_read = static_cast<size_t>(m_file->gcount());
            if (bytes_read == 0 && !m_file->eof())
            {
                throw PsarcException("Failed to read uncompressed block");
            }
            block.resize(bytes_read);
            result.insert(result.end(), block.begin(), block.end());
        }
        else
        {
            std::vector<uint8_t> chunk(z_len);
            m_file->read(reinterpret_cast<char*>(chunk.data()), z_len);
            if (std::cmp_not_equal(m_file->gcount(), z_len))
            {
                throw PsarcException("Failed to read compressed chunk");
            }

            const uint64_t remaining = entry.m_uncompressed_size - result.size();
            const uint64_t expected_size =
                std::min(remaining, static_cast<uint64_t>(m_header.m_block_size));

            std::vector<uint8_t> decompressed;
            const std::string_view compression(m_header.m_compression_method, 4);

            if (compression == "zlib")
            {
                decompressed = DecompressZlib(chunk, expected_size);
            }
            else if (compression == "lzma")
            {
                decompressed = DecompressLzma(chunk, expected_size);
            }
            else
            {
                // Try zlib first, then lzma as fallback
                decompressed = DecompressZlib(chunk, expected_size);
                if (decompressed.empty())
                {
                    decompressed = DecompressLzma(chunk, expected_size);
                }
            }

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

    result.resize(std::min(result.size(), static_cast<size_t>(entry.m_uncompressed_size)));

    if (entry.m_name.find("songs/bin/generic/") != std::string::npos &&
        entry.m_name.ends_with(".sng"))
    {
        result = DecryptSng(result);
    }

    return result;
}

std::vector<std::string> PsarcFile::GetFileList() const
{
    std::vector<std::string> files;
    files.reserve(m_entries.size());

    for (const auto& entry : m_entries)
    {
        if (!entry.m_name.empty())
        {
            files.push_back(entry.m_name);
        }
    }

    return files;
}

bool PsarcFile::FileExists(const std::string& file_name) const
{
    return m_file_map.contains(file_name);
}

std::vector<uint8_t> PsarcFile::ExtractFile(const std::string& file_name)
{
    const auto it = m_file_map.find(file_name);
    if (it == m_file_map.end())
    {
        throw PsarcException(std::format("File not found: {}", file_name));
    }
    return ExtractFileByIndex(it->second);
}

void PsarcFile::ExtractFileTo(const std::string& file_name, const std::string& output_path)
{
    const auto data = ExtractFile(file_name);

    std::ofstream out(output_path, std::ios::binary);
    if (!out)
    {
        throw PsarcException(std::format("Failed to create file: {}", output_path));
    }

    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));

    if (!out.good())
    {
        throw PsarcException(std::format("Failed to write file: {}", output_path));
    }
}

void PsarcFile::ExtractAll(const std::string& output_directory)
{
    fs::create_directories(output_directory);

    std::vector<std::string> failed_files;

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const auto& entry = m_entries[i];
        if (entry.m_name.empty())
        {
            continue;
        }

        const fs::path output_path = fs::path(output_directory) / entry.m_name;

        try
        {
            fs::create_directories(output_path.parent_path());

            const auto data = ExtractFileByIndex(static_cast<int>(i));

            std::ofstream out(output_path, std::ios::binary);
            if (!out)
            {
                failed_files.push_back(std::format("{}: failed to create file", entry.m_name));
                continue;
            }

            out.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));

            if (!out.good())
            {
                failed_files.push_back(std::format("{}: failed to write data", entry.m_name));
            }
        }
        catch (const std::exception& e)
        {
            failed_files.push_back(std::format("{}: {}", entry.m_name, e.what()));
        }
    }

    if (!failed_files.empty())
    {
        std::string error_msg = std::format("Failed to extract {} file(s):\n", failed_files.size());
        for (const auto& msg : failed_files)
        {
            error_msg += "  " + msg + "\n";
        }
        throw PsarcException(error_msg);
    }
}