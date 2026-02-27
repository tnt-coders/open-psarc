#pragma once

#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class PsarcException : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

class PsarcFile
{
  public:
    struct FileEntry
    {
        std::string name;
        uint64_t offset = 0;
        uint64_t uncompressed_size = 0;
        uint32_t start_chunk_index = 0;
    };

    explicit PsarcFile(std::string file_path);
    ~PsarcFile();

    PsarcFile(const PsarcFile&) = delete;
    PsarcFile& operator=(const PsarcFile&) = delete;
    PsarcFile(PsarcFile&&) noexcept = default;
    PsarcFile& operator=(PsarcFile&&) noexcept = default;

    void Open();
    void Close();
    [[nodiscard]] bool IsOpen() const
    {
        return m_is_open;
    }

    [[nodiscard]] std::vector<std::string> GetFileList() const;
    [[nodiscard]] bool FileExists(const std::string& file_name) const;
    [[nodiscard]] std::vector<uint8_t> ExtractFile(const std::string& file_name);
    void ExtractFileTo(const std::string& file_name, const std::string& output_path);
    void ExtractAll(const std::string& output_directory);
    void ConvertAudio(const std::string& output_directory);
    void ConvertSng(const std::string& output_directory);

    [[nodiscard]] int GetFileCount() const
    {
        return static_cast<int>(m_entries.size());
    }
    [[nodiscard]] const FileEntry* GetEntry(int index) const;
    [[nodiscard]] const FileEntry* GetEntry(const std::string& file_name) const;

  private:
    struct Header
    {
        uint32_t magic = 0;
        uint16_t version_major = 0;
        uint16_t version_minor = 0;
        std::array<char, 4> compression_method = {};
        uint32_t toc_length = 0;
        uint32_t toc_entry_size = 0;
        uint32_t num_files = 0;
        uint32_t block_size = 0;
        uint32_t archive_flags = 0;
    };

    void ReadHeader();
    void ReadToc();
    void ReadManifest();

    void ReadBytes(void* dest, size_t count);
    [[nodiscard]] uint16_t ReadBigEndian16();
    [[nodiscard]] uint32_t ReadBigEndian32();

    [[nodiscard]] std::vector<uint8_t> DecryptToc(const std::vector<uint8_t>& data);
    [[nodiscard]] std::vector<uint8_t> DecryptSng(const std::vector<uint8_t>& data);
    [[nodiscard]] static std::vector<uint8_t> DecompressZlib(const std::vector<uint8_t>& data,
                                                             uint64_t uncompressed_size);
    [[nodiscard]] static std::vector<uint8_t> DecompressLzma(const std::vector<uint8_t>& data,
                                                             uint64_t uncompressed_size);
    [[nodiscard]] std::vector<uint8_t> ExtractFileByIndex(int index);

    std::string m_file_path;
    std::unique_ptr<std::ifstream> m_file;
    Header m_header{};
    std::vector<FileEntry> m_entries;
    std::vector<uint16_t> m_z_lengths;
    std::unordered_map<std::string, int> m_file_map;
    bool m_is_open = false;
};
