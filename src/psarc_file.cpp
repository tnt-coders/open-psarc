#include "open-psarc/psarc_file.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "sng_parser.h"
#include "sng_xml_writer.h"

#include <lzma.h>
#include <nlohmann/json.hpp>
#include <openssl/evp.h>
#include <wwtools/wwtools.h>
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

const nlohmann::json* FindJsonKey(const nlohmann::json& obj,
                                  std::initializer_list<std::string_view> keys)
{
    if (!obj.is_object())
    {
        return nullptr;
    }

    for (const auto key : keys)
    {
        const std::string key_str(key);
        if (obj.contains(key_str))
        {
            return &obj.at(key_str);
        }
    }
    return nullptr;
}

const nlohmann::json* ResolveManifestSource(const nlohmann::json& root)
{
    if (!root.is_object())
    {
        return nullptr;
    }

    const auto* entries = FindJsonKey(root, {"Entries", "entries"});
    if (!entries || !entries->is_object() || entries->empty())
    {
        return nullptr;
    }

    const auto first = entries->begin();
    if (!first.value().is_object())
    {
        return nullptr;
    }

    const auto* attributes = FindJsonKey(first.value(), {"Attributes", "attributes"});
    if (!attributes || !attributes->is_object())
    {
        return nullptr;
    }

    return attributes;
}

template <typename T>
std::optional<T> ReadJsonValue(const nlohmann::json& obj,
                               std::initializer_list<std::string_view> keys)
{
    const auto* value = FindJsonKey(obj, keys);
    if (!value || value->is_null())
    {
        return std::nullopt;
    }

    if constexpr (std::is_same_v<T, std::string>)
    {
        if (value->is_string())
        {
            return value->get<std::string>();
        }
        return std::nullopt;
    }
    if constexpr (std::is_same_v<T, float>)
    {
        if (value->is_number())
        {
            return static_cast<float>(value->get<double>());
        }
        return std::nullopt;
    }
    if constexpr (std::is_same_v<T, int>)
    {
        if (value->is_number_integer() || value->is_number_unsigned())
        {
            return value->get<int>();
        }
        if (value->is_number_float())
        {
            return static_cast<int>(value->get<double>());
        }
        return std::nullopt;
    }

    return std::nullopt;
}

SngManifestMetadata ParseManifestMetadata(const std::string& json_text)
{
    SngManifestMetadata metadata;

    nlohmann::json root;
    try
    {
        constexpr std::string_view utf8_bom("\xEF\xBB\xBF");
        std::string_view payload(json_text);
        if (payload.starts_with(utf8_bom))
        {
            payload.remove_prefix(utf8_bom.size());
        }
        root = nlohmann::json::parse(payload);
    }
    catch (const std::exception&)
    {
        return metadata;
    }

    const nlohmann::json* source = ResolveManifestSource(root);
    if (!source)
    {
        return metadata;
    }

    metadata.title = ReadJsonValue<std::string>(*source, {"SongName", "songName"});
    metadata.arrangement =
        ReadJsonValue<std::string>(*source, {"ArrangementName", "arrangementName"});
    metadata.cent_offset = ReadJsonValue<float>(*source, {"CentOffset", "centOffset"});
    metadata.song_name_sort = ReadJsonValue<std::string>(*source, {"SongNameSort", "songNameSort"});
    metadata.average_tempo =
        ReadJsonValue<float>(*source, {"SongAverageTempo", "songAverageTempo"});
    metadata.artist_name = ReadJsonValue<std::string>(*source, {"ArtistName", "artistName"});
    metadata.artist_name_sort =
        ReadJsonValue<std::string>(*source, {"ArtistNameSort", "artistNameSort"});
    metadata.album_name = ReadJsonValue<std::string>(*source, {"AlbumName", "albumName"});
    metadata.album_name_sort =
        ReadJsonValue<std::string>(*source, {"AlbumNameSort", "albumNameSort"});
    metadata.album_year = ReadJsonValue<int>(*source, {"SongYear", "songYear"});
    metadata.tone_base = ReadJsonValue<std::string>(*source, {"Tone_Base", "toneBase"});
    metadata.tone_names[0] = ReadJsonValue<std::string>(*source, {"Tone_A", "toneA"});
    metadata.tone_names[1] = ReadJsonValue<std::string>(*source, {"Tone_B", "toneB"});
    metadata.tone_names[2] = ReadJsonValue<std::string>(*source, {"Tone_C", "toneC"});
    metadata.tone_names[3] = ReadJsonValue<std::string>(*source, {"Tone_D", "toneD"});

    const auto* props = FindJsonKey(*source, {"ArrangementProperties", "arrangementProperties"});
    if (props && props->is_object())
    {
        SngManifestArrangementProperties parsed;
        parsed.represent = ReadJsonValue<int>(*props, {"represent"}).value_or(0);
        parsed.bonus_arr = ReadJsonValue<int>(*props, {"bonusArr"}).value_or(0);
        parsed.standard_tuning = ReadJsonValue<int>(*props, {"standardTuning"}).value_or(0);
        parsed.non_standard_chords = ReadJsonValue<int>(*props, {"nonStandardChords"}).value_or(0);
        parsed.barre_chords = ReadJsonValue<int>(*props, {"barreChords"}).value_or(0);
        parsed.power_chords = ReadJsonValue<int>(*props, {"powerChords"}).value_or(0);
        parsed.drop_d_power = ReadJsonValue<int>(*props, {"dropDPower"}).value_or(0);
        parsed.open_chords = ReadJsonValue<int>(*props, {"openChords"}).value_or(0);
        parsed.finger_picking = ReadJsonValue<int>(*props, {"fingerPicking"}).value_or(0);
        parsed.pick_direction = ReadJsonValue<int>(*props, {"pickDirection"}).value_or(0);
        parsed.double_stops = ReadJsonValue<int>(*props, {"doubleStops"}).value_or(0);
        parsed.palm_mutes = ReadJsonValue<int>(*props, {"palmMutes"}).value_or(0);
        parsed.harmonics = ReadJsonValue<int>(*props, {"harmonics"}).value_or(0);
        parsed.pinch_harmonics = ReadJsonValue<int>(*props, {"pinchHarmonics"}).value_or(0);
        parsed.hopo = ReadJsonValue<int>(*props, {"hopo"}).value_or(0);
        parsed.tremolo = ReadJsonValue<int>(*props, {"tremolo"}).value_or(0);
        parsed.slides = ReadJsonValue<int>(*props, {"slides"}).value_or(0);
        parsed.unpitched_slides = ReadJsonValue<int>(*props, {"unpitchedSlides"}).value_or(0);
        parsed.bends = ReadJsonValue<int>(*props, {"bends"}).value_or(0);
        parsed.tapping = ReadJsonValue<int>(*props, {"tapping"}).value_or(0);
        parsed.vibrato = ReadJsonValue<int>(*props, {"vibrato"}).value_or(0);
        parsed.fret_hand_mutes = ReadJsonValue<int>(*props, {"fretHandMutes"}).value_or(0);
        parsed.slap_pop = ReadJsonValue<int>(*props, {"slapPop"}).value_or(0);
        parsed.two_finger_picking = ReadJsonValue<int>(*props, {"twoFingerPicking"}).value_or(0);
        parsed.fifths_and_octaves = ReadJsonValue<int>(*props, {"fifthsAndOctaves"}).value_or(0);
        parsed.syncopation = ReadJsonValue<int>(*props, {"syncopation"}).value_or(0);
        parsed.bass_pick = ReadJsonValue<int>(*props, {"bassPick"}).value_or(0);
        parsed.sustain = ReadJsonValue<int>(*props, {"sustain"}).value_or(0);
        parsed.path_lead = ReadJsonValue<int>(*props, {"pathLead"}).value_or(0);
        parsed.path_rhythm = ReadJsonValue<int>(*props, {"pathRhythm"}).value_or(0);
        parsed.path_bass = ReadJsonValue<int>(*props, {"pathBass"}).value_or(0);
        metadata.arrangement_properties = parsed;
    }

    return metadata;
}

bool IsLikelyManifestFile(std::string_view path)
{
    return path.ends_with(".json") && path.find("songs_dlc_") != std::string_view::npos;
}

std::string ToLower(std::string value)
{
    std::ranges::transform(value, value.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

PsarcFile::PsarcFile(std::string file_path) : m_file_path(std::move(file_path))
{
}

PsarcFile::~PsarcFile() = default;

void PsarcFile::ReadBytes(void* dest, size_t count)
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
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
    m_header.magic = ReadBigEndian32();

    if (m_header.magic != g_psarc_magic)
    {
        throw PsarcException("Invalid PSARC file: wrong magic number");
    }

    m_header.version_major = ReadBigEndian16();
    m_header.version_minor = ReadBigEndian16();
    ReadBytes(m_header.compression_method.data(), m_header.compression_method.size());
    m_header.toc_length = ReadBigEndian32();
    m_header.toc_entry_size = ReadBigEndian32();
    m_header.num_files = ReadBigEndian32();
    m_header.block_size = ReadBigEndian32();
    m_header.archive_flags = ReadBigEndian32();

    if (m_header.version_major != 1 || m_header.version_minor != 4)
    {
        throw PsarcException(std::format("Unsupported PSARC version: {}.{}", m_header.version_major,
                                         m_header.version_minor));
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
    const bool encrypted = (m_header.archive_flags & g_toc_encrypted_flag) != 0;

    m_file->seekg(32);
    std::vector<uint8_t> toc_data(m_header.toc_length - 32);
    ReadBytes(toc_data.data(), toc_data.size());

    if (encrypted)
    {
        toc_data = DecryptToc(toc_data);
    }

    const int b_num = (static_cast<int>(m_header.toc_entry_size) - 20) / 2;
    if (b_num < 1 || b_num > 8)
    {
        throw PsarcException("Invalid TOC entry size");
    }

    size_t pos = 0;

    m_entries.resize(m_header.num_files);

    for (uint32_t i = 0; i < m_header.num_files; ++i)
    {
        pos += 16; // Skip MD5

        if (pos + 4 > toc_data.size())
        {
            throw PsarcException("TOC data truncated while reading entry");
        }

        m_entries[i].start_chunk_index = ReadBE32(toc_data.data() + pos);
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

        m_entries[i].uncompressed_size = length;
        m_entries[i].offset = offset;
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

    m_entries[0].name = "NamesBlock.bin";
    m_file_map["NamesBlock.bin"] = 0;

    for (size_t i = 1; i < m_entries.size() && i - 1 < names.size(); ++i)
    {
        m_entries[i].name = names[i - 1];
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
        strm.next_in =
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
            const_cast<Bytef*>(data.data());
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
    if (entry.uncompressed_size == 0)
    {
        return {};
    }

    std::vector<uint8_t> result;
    result.reserve(static_cast<size_t>(entry.uncompressed_size));
    m_file->seekg(static_cast<std::streamoff>(entry.offset));

    uint32_t z_index = entry.start_chunk_index;

    while (result.size() < entry.uncompressed_size)
    {
        if (z_index >= m_z_lengths.size())
        {
            throw PsarcException("Chunk index out of range");
        }

        const uint16_t z_len = m_z_lengths[z_index++];

        if (z_len == 0)
        {
            std::vector<uint8_t> block(m_header.block_size);
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            m_file->read(reinterpret_cast<char*>(block.data()),
                         static_cast<std::streamsize>(m_header.block_size));
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
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            m_file->read(reinterpret_cast<char*>(chunk.data()), z_len);
            if (std::cmp_not_equal(m_file->gcount(), z_len))
            {
                throw PsarcException("Failed to read compressed chunk");
            }

            const uint64_t remaining = entry.uncompressed_size - result.size();
            const uint64_t expected_size =
                std::min(remaining, static_cast<uint64_t>(m_header.block_size));

            std::vector<uint8_t> decompressed;
            const std::string_view compression(m_header.compression_method.data(),
                                               m_header.compression_method.size());

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

    result.resize(std::min(result.size(), static_cast<size_t>(entry.uncompressed_size)));

    if (entry.name.find("songs/bin/generic/") != std::string::npos && entry.name.ends_with(".sng"))
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
        if (!entry.name.empty())
        {
            files.push_back(entry.name);
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

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
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
        if (entry.name.empty())
        {
            continue;
        }

        const fs::path output_path = fs::path(output_directory) / entry.name;

        try
        {
            fs::create_directories(output_path.parent_path());

            const auto data = ExtractFileByIndex(static_cast<int>(i));

            std::ofstream out(output_path, std::ios::binary);
            if (!out)
            {
                failed_files.push_back(std::format("{}: failed to create file", entry.name));
                continue;
            }

            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            out.write(reinterpret_cast<const char*>(data.data()),
                      static_cast<std::streamsize>(data.size()));

            if (!out.good())
            {
                failed_files.push_back(std::format("{}: failed to write data", entry.name));
            }
        }
        catch (const std::exception& e)
        {
            failed_files.push_back(std::format("{}: {}", entry.name, e.what()));
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

void PsarcFile::ConvertAudio(const std::string& output_directory)
{
    fs::create_directories(output_directory);

    // Track which WEM IDs are referenced by BNK files so we don't convert them twice
    std::unordered_map<std::string, bool> referenced_wems;

    // Collect BNK and WEM files from the archive
    std::vector<std::string> bnk_files;
    std::vector<std::string> wem_files;

    for (const auto& entry : m_entries)
    {
        if (entry.name.empty())
        {
            continue;
        }

        if (entry.name.ends_with(".bnk"))
        {
            bnk_files.push_back(entry.name);
        }
        else if (entry.name.ends_with(".wem"))
        {
            wem_files.push_back(entry.name);
        }
    }

    std::vector<std::string> failed_files;

    // Process BNK files
    for (const auto& bnk_name : bnk_files)
    {
        try
        {
            const auto bnk_data = ExtractFile(bnk_name);
            const std::string_view bnk_view(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<const char*>(bnk_data.data()), bnk_data.size());
            const auto entries = wwtools::BnkExtract(bnk_view);

            // Use the BNK stem as the song name
            const fs::path bnk_path(bnk_name);
            const std::string song_name = bnk_path.stem().string();

            for (size_t i = 0; i < entries.size(); ++i)
            {
                const auto& bnk_entry = entries[i];

                try
                {
                    std::string wem_data;

                    if (bnk_entry.streamed)
                    {
                        // Find the corresponding WEM file in the archive by ID
                        const std::string wem_id = std::to_string(bnk_entry.id);
                        std::string found_wem;

                        for (const auto& wem_name : wem_files)
                        {
                            if (fs::path(wem_name).stem().string() == wem_id)
                            {
                                found_wem = wem_name;
                                break;
                            }
                        }

                        if (found_wem.empty())
                        {
                            failed_files.push_back(
                                std::format("{}: streamed WEM {} not found in archive", bnk_name,
                                            bnk_entry.id));
                            continue;
                        }

                        referenced_wems[found_wem] = true;
                        const auto raw = ExtractFile(found_wem);
                        wem_data.assign(
                            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                            reinterpret_cast<const char*>(raw.data()), raw.size());
                    }
                    else
                    {
                        wem_data = bnk_entry.data;
                    }

                    if (wem_data.empty())
                    {
                        continue;
                    }

                    const auto ogg_data = wwtools::Wem2Ogg(wem_data);

                    // Name the OGG after the song (BNK stem), with a suffix if multiple entries
                    std::string ogg_name = song_name;
                    if (entries.size() > 1)
                    {
                        ogg_name += std::format("_{}", i);
                    }
                    ogg_name += ".ogg";

                    const fs::path ogg_path =
                        fs::path(output_directory) / bnk_path.parent_path() / ogg_name;
                    fs::create_directories(ogg_path.parent_path());

                    std::ofstream out(ogg_path, std::ios::binary);
                    if (!out)
                    {
                        failed_files.push_back(std::format("{}: failed to create file", ogg_name));
                        continue;
                    }

                    out.write(ogg_data.data(), static_cast<std::streamsize>(ogg_data.size()));
                    if (!out.good())
                    {
                        failed_files.push_back(std::format("{}: failed to write data", ogg_name));
                    }
                }
                catch (const std::exception& e)
                {
                    failed_files.push_back(
                        std::format("{} (WEM {}): {}", bnk_name, bnk_entry.id, e.what()));
                }
            }
        }
        catch (const std::exception& e)
        {
            failed_files.push_back(std::format("{}: {}", bnk_name, e.what()));
        }
    }

    // Convert standalone WEM files not referenced by any BNK
    for (const auto& wem_name : wem_files)
    {
        if (referenced_wems.contains(wem_name))
        {
            continue;
        }

        try
        {
            const auto raw = ExtractFile(wem_name);
            const std::string_view wem_view(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                reinterpret_cast<const char*>(raw.data()), raw.size());
            const auto ogg_data = wwtools::Wem2Ogg(wem_view);

            const fs::path wem_path(wem_name);
            const std::string ogg_name = wem_path.stem().string() + ".ogg";
            const fs::path ogg_path =
                fs::path(output_directory) / wem_path.parent_path() / ogg_name;
            fs::create_directories(ogg_path.parent_path());

            std::ofstream out(ogg_path, std::ios::binary);
            if (!out)
            {
                failed_files.push_back(std::format("{}: failed to create file", ogg_name));
                continue;
            }

            out.write(ogg_data.data(), static_cast<std::streamsize>(ogg_data.size()));
            if (!out.good())
            {
                failed_files.push_back(std::format("{}: failed to write data", ogg_name));
            }
        }
        catch (const std::exception& e)
        {
            failed_files.push_back(std::format("{}: {}", wem_name, e.what()));
        }
    }

    if (!failed_files.empty())
    {
        std::string error_msg =
            std::format("Failed to convert {} audio file(s):\n", failed_files.size());
        for (const auto& msg : failed_files)
        {
            error_msg += "  " + msg + "\n";
        }
        throw PsarcException(error_msg);
    }
}

void PsarcFile::ConvertSng(const std::string& output_directory)
{
    fs::create_directories(output_directory);

    // Collect SNG files from songs/bin/generic/
    std::vector<std::string> sng_files;
    for (const auto& entry : m_entries)
    {
        if (entry.name.find("songs/bin/generic/") != std::string::npos &&
            entry.name.ends_with(".sng"))
        {
            sng_files.push_back(entry.name);
        }
    }

    std::vector<std::string> failed_files;
    std::vector<int> manifest_indices;
    manifest_indices.reserve(m_entries.size());
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        if (IsLikelyManifestFile(m_entries[i].name))
        {
            manifest_indices.push_back(static_cast<int>(i));
        }
    }

    for (const auto& sng_name : sng_files)
    {
        try
        {
            const auto data = ExtractFile(sng_name);

            const auto sng_data = SngParser::Parse(data);

            const std::string sng_stem = ToLower(fs::path(sng_name).stem().string());
            std::optional<SngManifestMetadata> manifest;

            int matched_manifest = -1;
            for (const int idx : manifest_indices)
            {
                const std::string json_stem =
                    ToLower(fs::path(m_entries[idx].name).stem().string());
                if (json_stem == sng_stem)
                {
                    matched_manifest = idx;
                    break;
                }
            }

            if (matched_manifest < 0)
            {
                for (const int idx : manifest_indices)
                {
                    const std::string json_name = ToLower(m_entries[idx].name);
                    if (json_name.find(sng_stem) != std::string::npos)
                    {
                        matched_manifest = idx;
                        break;
                    }
                }
            }

            if (matched_manifest >= 0)
            {
                const auto json_data = ExtractFileByIndex(matched_manifest);
                std::string json_text(json_data.begin(), json_data.end());
                manifest = ParseManifestMetadata(json_text);
            }

            // Output path: songs/bin/generic/foo.sng -> {output_dir}/songs/arr/foo.xml
            const fs::path sng_path(sng_name);
            const std::string xml_name = sng_path.stem().string() + ".xml";
            const fs::path xml_path = fs::path(output_directory) / "songs" / "arr" / xml_name;
            fs::create_directories(xml_path.parent_path());

            SngXmlWriter::Write(sng_data, xml_path, manifest ? &(*manifest) : nullptr);
        }
        catch (const std::exception& e)
        {
            failed_files.push_back(std::format("{}: {}", sng_name, e.what()));
        }
    }

    if (!failed_files.empty())
    {
        std::string error_msg =
            std::format("Failed to convert {} SNG file(s):\n", failed_files.size());
        for (const auto& msg : failed_files)
        {
            error_msg += "  " + msg + "\n";
        }
        throw PsarcException(error_msg);
    }
}
