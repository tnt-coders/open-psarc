#include "psarc_file.h"
#include <chrono>
#include <iostream>
#include <format>

void printUsage(const char* programName)
{
    std::cout << std::format(
        "Usage: {} <psarc_file> [output_directory]\n"
        "\n"
        "Arguments:\n"
        "  psarc_file        Path to the .psarc file to open\n"
        "  output_directory  Directory to extract files to (optional)\n"
        "\n"
        "If no output directory is specified, lists the archive contents.\n"
        "If an output directory is specified, extracts all files to that directory.\n",
        programName);
}

int main(int argc, char* argv[])
{
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")
    {
        printUsage(argv[0]);
        return (argc < 2) ? 1 : 0;
    }

    try
    {
        PsarcFile psarc(argv[1]);
        psarc.open();

        std::cout << std::format("Archive: {}\n", argv[1]);
        std::cout << std::format("Files: {}\n\n", psarc.getFileCount());

        for (const auto& name : psarc.getFileList())
        {
            if (auto* entry = psarc.getEntry(name))
            {
                std::cout << std::format("  {} ({} bytes)\n", name, entry->uncompressedSize);
            }
        }

        if (argc > 2)
        {
            std::cout << std::format("\nExtracting to: {}\n", argv[2]);
            auto start = std::chrono::steady_clock::now();
            psarc.extractAll(argv[2]);
            auto end = std::chrono::steady_clock::now();
            std::cout << std::format("Successfully extracted {} files.\n", psarc.getFileCount());

            // Various ways to express the result
            auto duration = end - start;
            std::cout << "Elapsed time: " << std::chrono::duration<double, std::milli>(duration).count() << " ms\n";
        }
    }
    catch (const PsarcException& e)
    {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }

    return 0;
}
