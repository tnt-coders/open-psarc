#include "psarcfile.h"
#include <iostream>
#include <format>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << std::format("Usage: {} <psarc_file> [output_directory]\n", argv[0]);
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputDir = (argc > 2) ? argv[2] : "./extracted";

    try {
        PsarcFile psarc(inputFile);

        // Set up logging (optional)
        psarc.setLogCallback([](const std::string& msg) {
            std::cout << std::format("[LOG] {}\n", msg);
        });

        std::cout << std::format("Opening: {}\n", inputFile);

        psarc.open();

        std::cout << std::format("File count: {}\n\n", psarc.getFileCount());

        // List files
        std::cout << "Files in archive:\n";
        auto files = psarc.getFileList();
        for (size_t i = 0; i < files.size() && i < 20; ++i) {
            const auto* entry = psarc.getEntry(files[i]);
            if (entry) {
                std::cout << std::format("  {} ({} bytes)\n", files[i], entry->uncompressedSize);
            }
        }

        if (files.size() > 20) {
            std::cout << std::format("  ... and {} more files\n", files.size() - 20);
        }

        std::cout << '\n';

        // Extract all files
        std::cout << std::format("Extracting to: {}\n", outputDir);
        psarc.extractAll(outputDir);
        std::cout << "Extraction successful!\n";

    } catch (const PsarcException& e) {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }

    return 0;
}
