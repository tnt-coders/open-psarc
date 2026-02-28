# open-psarc

[![Build](https://github.com/tnt-coders/open-psarc/actions/workflows/build.yml/badge.svg)](https://github.com/tnt-coders/open-psarc/actions/workflows/build.yml)

A C++23 library and command-line tool for reading and extracting PSARC (PlayStation Archive) files, with specialized support for **Rocksmith 2014** — including TOC/SNG decryption, WEM-to-OGG audio conversion, and SNG-to-XML arrangement conversion.

Generic PSARC extraction works for any PSARC archive; the conversion tools (audio and SNG) are Rocksmith 2014-specific.

## Features

- Read and extract PSARC archives (zlib and LZMA compression)
- Automatic decryption of Rocksmith 2014 TOC and SNG files
- WEM/BNK to OGG audio conversion
- SNG binary to XML arrangement conversion
- Available as both a C++ library and CLI tool

## Building

### Prerequisites

- CMake 3.24+
- C++23 compatible compiler
- Conan 2.x (for dependency management)

### Build Steps

```bash
# Install dependencies
conan install . --output-folder=build/debug --build=missing

# Configure
cmake --preset conan-debug      # or conan-release

# Build
cmake --build build/debug       # or build/release
```

## Usage

### Command Line

```bash
# List archive contents
open-psarc archive.psarc

# Extract all files
open-psarc archive.psarc ./output

# Extract with audio conversion (WEM/BNK -> OGG)
open-psarc -a archive.psarc ./output

# Extract with SNG arrangement conversion (SNG -> XML)
open-psarc -s archive.psarc ./output

# Extract with both conversions
open-psarc -a -s archive.psarc ./output

# Extract quietly (no file listing)
open-psarc -q archive.psarc ./output

# List only (don't extract)
open-psarc -l archive.psarc
```

### Library

```cpp
#include <open-psarc/psarc_file.h>
#include <print>

int main()
{
    try
    {
        PsarcFile psarc("archive.psarc");
        psarc.Open();

        // List files
        for (const auto& name : psarc.GetFileList())
        {
            std::println("{}", name);
        }

        // Extract a single file to memory
        auto data = psarc.ExtractFile("path/to/file.txt");

        // Extract all files to disk
        psarc.ExtractAll("./output");

        // Convert audio (Rocksmith 2014)
        psarc.ConvertAudio("./output");

        // Convert SNG arrangements to XML (Rocksmith 2014)
        psarc.ConvertSng("./output");
    }
    catch (const PsarcException& e)
    {
        std::println(stderr, "Error: {}", e.what());
        return 1;
    }

    return 0;
}
```

### CMake Integration

```cmake
find_package(OpenPSARC REQUIRED)
target_link_libraries(your_target PRIVATE OpenPSARC::OpenPSARC)
```

## API Reference

### `PsarcFile`

| Method | Description |
|--------|-------------|
| `PsarcFile(std::string path)` | Construct with path to .psarc file |
| `void Open()` | Open and parse the archive |
| `void Close()` | Close the archive |
| `bool IsOpen() const` | Check if archive is open |
| `std::vector<std::string> GetFileList() const` | Get list of all file names |
| `bool FileExists(const std::string& name) const` | Check if file exists in archive |
| `std::vector<uint8_t> ExtractFile(const std::string& name)` | Extract file to memory |
| `void ExtractFileTo(const std::string& name, const std::string& path)` | Extract file to disk |
| `void ExtractAll(const std::string& directory)` | Extract all files to directory |
| `void ConvertAudio(const std::string& directory)` | Convert WEM/BNK audio to OGG |
| `void ConvertSng(const std::string& directory)` | Convert SNG arrangements to XML |
| `int GetFileCount() const` | Get number of files in archive |
| `const FileEntry* GetEntry(int index) const` | Get entry by index |
| `const FileEntry* GetEntry(const std::string& name) const` | Get entry by name |

### `PsarcException`

Thrown on any error. Inherits from `std::runtime_error`.
