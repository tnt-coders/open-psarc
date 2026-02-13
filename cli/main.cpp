#include <open-psarc/psarc_file.h>

#include <chrono>
#include <cstring>
#include <format>
#include <iostream>

void PrintUsage(const char* program_name)
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
                             program_name, program_name, program_name, program_name);
}

void PrintVersion()
{
    std::cout << "open-psarc version 1.0.0\n";
}

int main(int argc, char* argv[])
{
    bool list_only = false;
    bool quiet = false;
    const char* psarc_path = nullptr;
    const char* output_dir = nullptr;

    // Parse arguments
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0)
        {
            PrintUsage(argv[0]);
            return 0;
        }
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--version") == 0)
        {
            PrintVersion();
            return 0;
        }
        if (std::strcmp(argv[i], "-l") == 0 || std::strcmp(argv[i], "--list") == 0)
        {
            list_only = true;
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
        if (!psarc_path)
        {
            psarc_path = argv[i];
        }
        else if (!output_dir)
        {
            output_dir = argv[i];
        }
        else
        {
            std::cerr << "Too many arguments\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    if (!psarc_path)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    try
    {
        PsarcFile psarc(psarc_path);
        psarc.Open();

        std::cout << std::format("Archive: {}\n", psarc_path);
        std::cout << std::format("Files: {}\n", psarc.GetFileCount());

        const bool should_list = list_only || !output_dir || !quiet;

        if (should_list)
        {
            std::cout << "\n";
            for (const auto& name : psarc.GetFileList())
            {
                if (const auto* entry = psarc.GetEntry(name))
                {
                    std::cout << std::format("  {} ({} bytes)\n", name, entry->m_uncompressed_size);
                }
            }
        }

        if (output_dir && !list_only)
        {
            std::cout << std::format("\nExtracting to: {}\n", output_dir);

            const auto start = std::chrono::steady_clock::now();
            psarc.ExtractAll(output_dir);
            const auto end = std::chrono::steady_clock::now();

            const auto duration = std::chrono::duration<double, std::milli>(end - start);
            std::cout << std::format("Successfully extracted {} files in {:.2f} ms\n",
                                     psarc.GetFileCount(), duration.count());
        }
    }
    catch (const PsarcException& e)
    {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << std::format("Unexpected error: {}\n", e.what());
        return 1;
    }

    return 0;
}