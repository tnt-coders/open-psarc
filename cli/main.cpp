#include <open-psarc/psarc_file.h>

#include <chrono>
#include <cstring>
#include <format>
#include <iostream>

void printUsage(const char *programName)
{
    std::cout << std::format("Usage: {} [options] <psarc_file> [output_directory]\n"
                             "\n"
                             "A tool for reading and extracting PSARC archives.\n"
                             "\n"
                             "Arguments:\n"
                             "  psarc_file        Path to the .psarc file to open\n"
                             "  output_directory  Directory to extract files to (optional)\n"
                             "\n"
                             "Options:\n"
                             "  -h, --help        Show this help message\n"
                             "  -l, --list        List files only (don't extract)\n"
                             "  -q, --quiet       Suppress file listing during extraction\n"
                             "  -v, --version     Show version information\n"
                             "\n"
                             "Examples:\n"
                             "  {} archive.psarc              List archive contents\n"
                             "  {} archive.psarc ./output     Extract all files to ./output\n"
                             "  {} -q archive.psarc ./output  Extract quietly\n",
                             programName, programName, programName, programName);
}

void printVersion()
{
    std::cout << "open-psarc version 1.0.0\n";
}

int main(int argc, char *argv[])
{
    bool listOnly = false;
    bool quiet = false;
    const char *psarcPath = nullptr;
    const char *outputDir = nullptr;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--version") == 0)
        {
            printVersion();
            return 0;
        }
        if (std::strcmp(argv[i], "-l") == 0 || std::strcmp(argv[i], "--list") == 0)
        {
            listOnly = true;
            continue;
        }
        if (std::strcmp(argv[i], "-q") == 0 || std::strcmp(argv[i], "--quiet") == 0)
        {
            quiet = true;
            continue;
        }
        if (argv[i][0] == '-')
        {
            std::cerr << std::format("Unknown option: {}\n", argv[i]);
            return 1;
        }

        // Positional arguments
        if (!psarcPath)
        {
            psarcPath = argv[i];
        }
        else if (!outputDir)
        {
            outputDir = argv[i];
        }
        else
        {
            std::cerr << "Too many arguments\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    if (!psarcPath)
    {
        printUsage(argv[0]);
        return 1;
    }

    try
    {
        PsarcFile psarc(psarcPath);
        psarc.open();

        std::cout << std::format("Archive: {}\n", psarcPath);
        std::cout << std::format("Files: {}\n", psarc.getFileCount());

        const bool shouldList = listOnly || !outputDir || !quiet;

        if (shouldList)
        {
            std::cout << "\n";
            for (const auto &name : psarc.getFileList())
            {
                if (const auto *entry = psarc.getEntry(name))
                {
                    std::cout << std::format("  {} ({} bytes)\n", name, entry->uncompressedSize);
                }
            }
        }

        if (outputDir && !listOnly)
        {
            std::cout << std::format("\nExtracting to: {}\n", outputDir);

            const auto start = std::chrono::steady_clock::now();
            psarc.extractAll(outputDir);
            const auto end = std::chrono::steady_clock::now();

            const auto duration = std::chrono::duration<double, std::milli>(end - start);
            std::cout << std::format("Successfully extracted {} files in {:.2f} ms\n",
                                     psarc.getFileCount(), duration.count());
        }
    }
    catch (const PsarcException &e)
    {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }

    return 0;
}
