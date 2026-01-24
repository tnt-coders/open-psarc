#ifndef PSARCFILE_H
#define PSARCFILE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <cstdint>
#include <functional>

class PsarcFile
{
public:
    struct FileEntry {
        std::string name;
        uint64_t offset = 0;
        uint64_t uncompressedSize = 0;
        uint32_t startChunkIndex = 0;
    };

    // Optional logging callback
    using LogCallback = std::function<void(const std::string&)>;

    explicit PsarcFile(const std::string& filePath);
    ~PsarcFile();

    // Disable copy
    PsarcFile(const PsarcFile&) = delete;
    PsarcFile& operator=(const PsarcFile&) = delete;

    // Enable move
    PsarcFile(PsarcFile&&) noexcept = default;
    PsarcFile& operator=(PsarcFile&&) noexcept = default;

    bool open();
    void close();
    bool isOpen() const;

    std::vector<std::string> getFileList() const;
    bool fileExists(const std::string& fileName) const;
    std::vector<uint8_t> extractFile(const std::string& fileName);
    bool extractFileTo(const std::string& fileName, const std::string& outputPath);
    bool extractAll(const std::string& outputDirectory);

    std::string getLastError() const { return m_lastError; }
    int getFileCount() const { return static_cast<int>(m_entries.size()); }
    const FileEntry* getEntry(int index) const;
    const FileEntry* getEntry(const std::string& fileName) const;

    // Set optional debug logging callback
    void setLogCallback(LogCallback callback) { m_logCallback = std::move(callback); }

private:
    struct Header {
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

    bool readHeader();
    bool readTOC();
    bool readManifest();

    std::vector<uint8_t> decryptTOC(const std::vector<uint8_t>& encryptedData);
    std::vector<uint8_t> decryptSNG(const std::vector<uint8_t>& encryptedData);

    std::vector<uint8_t> decompressZlib(const std::vector<uint8_t>& data, uint64_t uncompressedSize, bool& success);

    std::vector<uint8_t> extractFileByIndex(int entryIndex);

    // Helper functions for binary reading
    uint16_t readBigEndian16();
    uint32_t readBigEndian32();
    uint32_t readLittleEndian32();

    void log(const std::string& message) const;

    std::string m_filePath;
    std::unique_ptr<std::ifstream> m_file;
    Header m_header;
    std::vector<FileEntry> m_entries;
    std::vector<uint16_t> m_zLengths;
    std::unordered_map<std::string, int> m_fileMap;
    std::string m_lastError;
    bool m_isOpen = false;
    bool m_tocEncrypted = false;
    LogCallback m_logCallback;
};

#endif // PSARCFILE_H
