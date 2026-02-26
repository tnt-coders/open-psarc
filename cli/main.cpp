#include <open-psarc/psarc_file.h>

#include <chrono>
#include <cstring>
#include <print>

void PrintUsage(const char* program_name)
{
    std::print("Usage: {} [options] <psarc_file> [output_directory]\n"
               "\n"
               "A tool for reading and extracting Rocksmith 2014 PSARC archives.\n"
               "\n"
               "Arguments:\n"
               "  psarc_file        Path to the .psarc file to open\n"
               "  output_directory  Directory to extract files to (optional)\n"
               "\n"
               "Options:\n"
               "  -a, --convert-audio  Convert .wem/.bnk audio to .ogg after extraction\n"
               "  -h, --help           Show this help message\n"
               "  -l, --list           List files only (don't extract)\n"
               "  -q, --quiet          Suppress file listing during extraction\n"
               "  -s, --convert-sng    Convert .sng arrangements to .xml after extraction\n"
               "  -v, --version        Show version information\n"
               "\n"
               "Examples:\n"
               "  {} archive.psarc              List archive contents\n"
               "  {} archive.psarc ./output     Extract all files to ./output\n"
               "  {} -a -s archive.psarc ./out  Extract with audio and SNG conversion\n",
               program_name, program_name, program_name, program_name);
}

void PrintVersion()
{
    std::println("open-psarc version 1.0.0");
}

int main(int argc, char* argv[]) // NOLINT(bugprone-exception-escape)
{
    try
    {
        bool convert_audio = false;
        bool convert_sng = false;
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
            if (std::strcmp(argv[i], "-a") == 0 || std::strcmp(argv[i], "--convert-audio") == 0)
            {
                convert_audio = true;
                continue;
            }
            if (std::strcmp(argv[i], "-s") == 0 || std::strcmp(argv[i], "--convert-sng") == 0)
            {
                convert_sng = true;
                continue;
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
                std::println(stderr, "Unknown option: {}", argv[i]);
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
                std::println(stderr, "Too many arguments");
                PrintUsage(argv[0]);
                return 1;
            }
        }

        if (!psarc_path)
        {
            PrintUsage(argv[0]);
            return 1;
        }

        PsarcFile psarc(psarc_path);
        psarc.Open();

        std::println("Archive: {}", psarc_path);
        std::println("Files: {}", psarc.GetFileCount());

        const bool should_list = list_only || !output_dir || !quiet;

        if (should_list)
        {
            std::println("");
            for (const auto& name : psarc.GetFileList())
            {
                if (const auto* entry = psarc.GetEntry(name))
                {
                    std::println("  {} ({} bytes)", name, entry->m_uncompressed_size);
                }
            }
        }

        if (output_dir && !list_only)
        {
            std::println("\nExtracting to: {}", output_dir);

            const auto start = std::chrono::steady_clock::now();
            psarc.ExtractAll(output_dir);
            const auto end = std::chrono::steady_clock::now();

            const auto duration = std::chrono::duration<double, std::milli>(end - start);
            std::println("Successfully extracted {} files in {:.2f} ms", psarc.GetFileCount(),
                         duration.count());

            if (convert_audio)
            {
                std::println("\nConverting audio files...");

                const auto audio_start = std::chrono::steady_clock::now();
                psarc.ConvertAudio(output_dir);
                const auto audio_end = std::chrono::steady_clock::now();

                const auto audio_duration =
                    std::chrono::duration<double, std::milli>(audio_end - audio_start);
                std::println("Audio conversion completed in {:.2f} ms", audio_duration.count());
            }

            if (convert_sng)
            {
                std::println("\nConverting SNG arrangements to XML...");

                const auto sng_start = std::chrono::steady_clock::now();
                psarc.ConvertSng(output_dir);
                const auto sng_end = std::chrono::steady_clock::now();

                const auto sng_duration =
                    std::chrono::duration<double, std::milli>(sng_end - sng_start);
                std::println("SNG conversion completed in {:.2f} ms", sng_duration.count());
            }
        }
    }
    catch (const PsarcException& e)
    {
        std::println(stderr, "Error: {}", e.what());
        return 1;
    }
    catch (const std::exception& e)
    {
        std::println(stderr, "Unexpected error: {}", e.what());
        return 1;
    }

    return 0;
}
