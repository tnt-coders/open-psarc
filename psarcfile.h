#pragma once

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
        uint64_t uncompressedSize = 0;
        uint32_t startChunkIndex = 0;
    };

    explicit PsarcFile(const std::string& filePath);
    ~PsarcFile();

    PsarcFile(const PsarcFile&) = delete;
    PsarcFile& operator=(const PsarcFile&) = delete;
    PsarcFile(PsarcFile&&) noexcept = default;
    PsarcFile& operator=(PsarcFile&&) noexcept = default;

    void open();
    void close();
    [[nodiscard]] bool isOpen() const { return m_isOpen; }

    [[nodiscard]] std::vector<std::string> getFileList() const;
    [[nodiscard]] bool fileExists(const std::string& fileName) const;
    [[nodiscard]] std::vector<uint8_t> extractFile(const std::string& fileName);
    void extractFileTo(const std::string& fileName, const std::string& outputPath);
    void extractAll(const std::string& outputDirectory);

    [[nodiscard]] int getFileCount() const { return static_cast<int>(m_entries.size()); }
    [[nodiscard]] const FileEntry* getEntry(int index) const;
    [[nodiscard]] const FileEntry* getEntry(const std::string& fileName) const;

private:
    struct Header
    {
        uint32_t magic = 0;
        uint16_t versionMajor = 0;
        uint16_t versionMinor = 0;
        char compressionMethod[4] = {0};
        uint32_t tocLength = 0;
        uint32_t tocEntrySize = 0;
        uint32_t numFiles = 0;
        uint32_t blockSize = 0;
        uint32_t archiveFlags = 0;
    };

    void readHeader();
    void readTOC();
    void readManifest();

    [[nodiscard]] std::vector<uint8_t> decryptTOC(const std::vector<uint8_t>& data);
    [[nodiscard]] std::vector<uint8_t> decryptSNG(const std::vector<uint8_t>& data);
    [[nodiscard]] std::vector<uint8_t> decompressZlib(const std::vector<uint8_t>& data, uint64_t uncompressedSize);
    [[nodiscard]] std::vector<uint8_t> extractFileByIndex(int index);

    [[nodiscard]] uint16_t readBigEndian16();
    [[nodiscard]] uint32_t readBigEndian32();

    std::string m_filePath;
    std::unique_ptr<std::ifstream> m_file;
    Header m_header;
    std::vector<FileEntry> m_entries;
    std::vector<uint16_t> m_zLengths;
    std::unordered_map<std::string, int> m_fileMap;
    bool m_isOpen = false;
};
