#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class PsarcException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class PsarcFile
{
public:
    explicit PsarcFile(std::string file_path);
    ~PsarcFile();

    PsarcFile(const PsarcFile&) = delete;
    PsarcFile& operator=(const PsarcFile&) = delete;
    PsarcFile(PsarcFile&&) noexcept;
    PsarcFile& operator=(PsarcFile&&) noexcept;

    void Open();
    void Close();
    [[nodiscard]] bool IsOpen() const;
    [[nodiscard]] int GetFileCount() const;

    [[nodiscard]] std::vector<std::string> GetFileList() const;
    [[nodiscard]] bool FileExists(const std::string& file_name) const;
    [[nodiscard]] uint64_t GetFileSize(const std::string& file_name) const;
    [[nodiscard]] std::vector<uint8_t> ExtractFile(const std::string& file_name);
    void ExtractFileTo(const std::string& file_name, const std::string& output_path);
    void ExtractAll(const std::string& output_directory);
    void ConvertAudio(const std::string& output_directory);
    void ConvertSng(const std::string& output_directory);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
