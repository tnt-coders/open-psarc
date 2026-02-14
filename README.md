# open-psarc

[![Pre-commit](https://github.com/tnt-coders/open-psarc/actions/workflows/pre-commit.yml/badge.svg)](https://github.com/tnt-coders/open-psarc/actions/workflows/pre-commit.yml)
[![Build](https://github.com/tnt-coders/open-psarc/actions/workflows/build.yml/badge.svg)](https://github.com/tnt-coders/open-psarc/actions/workflows/build.yml)
[![Lint](https://github.com/tnt-coders/open-psarc/actions/workflows/lint.yml/badge.svg)](https://github.com/tnt-coders/open-psarc/actions/workflows/lint.yml)
[![Package](https://github.com/tnt-coders/open-psarc/actions/workflows/package.yml/badge.svg)](https://github.com/tnt-coders/open-psarc/actions/workflows/package.yml)

A C++23 library and command-line tool for reading PSARC (PlayStation Archive) files, with support for Rocksmith-specific encryption.

## ⚠️ Active Development Notice

This project is under active development. The information in this README may not reflect the current state of the codebase. Please check the latest commits and issues for the most up-to-date information.

## Features

- Read and extract PSARC archives
- Support for zlib and LZMA compression
- Automatic decryption of Rocksmith TOC and SNG files
- Available as both a library and CLI tool

## Building

### Prerequisites

- CMake 3.16+
- C++23 compatible compiler
- Conan 2.x (for dependency management)

### Build Steps

```bash
# Install dependencies
conan install . --output-folder=build --build=missing

# Configure
cmake --preset conan-release
# or for debug: cmake --preset conan-debug

# Build
cmake --build build --config Release

# Install (optional)
cmake --install build --prefix /usr/local
```

## Usage

### Command Line

```bash
# List archive contents
open-psarc archive.psarc

# Extract all files
open-psarc archive.psarc ./output

# Extract quietly (no file listing)
open-psarc -q archive.psarc ./output

# List only (don't extract even if output dir given)
open-psarc -l archive.psarc ./output
```

### Library

```cpp
#include <open-psarc/psarc_file.h>

int main()
{
    try
    {
        PsarcFile psarc("archive.psarc");
        psarc.open();

        // List files
        for (const auto& name : psarc.getFileList())
        {
            std::cout << name << "\n";
        }

        // Extract a single file
        auto data = psarc.extractFile("path/to/file.txt");

        // Extract all files
        psarc.extractAll("./output");
    }
    catch (const PsarcException& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
```

### CMake Integration

After installing, use in your CMake project:

```cmake
find_package(open-psarc REQUIRED)
target_link_libraries(your_target PRIVATE tnt::open-psarc)
```

## API Reference

### `PsarcFile`

| Method                                                                 | Description                        |
|------------------------------------------------------------------------|------------------------------------|
| `PsarcFile(const std::string& path)`                                   | Construct with path to .psarc file |
| `void open()`                                                          | Open and parse the archive         |
| `void close()`                                                         | Close the archive                  |
| `bool isOpen() const`                                                  | Check if archive is open           |
| `std::vector<std::string> getFileList() const`                         | Get list of all file names         |
| `bool fileExists(const std::string& name) const`                       | Check if file exists in archive    |
| `std::vector<uint8_t> extractFile(const std::string& name)`            | Extract file to memory             |
| `void extractFileTo(const std::string& name, const std::string& path)` | Extract file to disk               |
| `void extractAll(const std::string& directory)`                        | Extract all files to directory     |
| `int getFileCount() const`                                             | Get number of files in archive     |
| `const FileEntry* getEntry(int index) const`                           | Get entry by index                 |
| `const FileEntry* getEntry(const std::string& name) const`             | Get entry by name                  |

### `PsarcException`

Thrown on any error. Inherits from `std::runtime_error`.
