#include "psarcfile.h"
#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <psarc_file> [output_directory]" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputDir = (argc > 2) ? argv[2] : "./extracted";

    PsarcFile psarc(inputFile);

    // Set up logging (optional)
    psarc.setLogCallback([](const std::string& msg) {
        std::cout << "[LOG] " << msg << std::endl;
    });

    std::cout << "Opening: " << inputFile << std::endl;

    if (!psarc.open()) {
        std::cerr << "Error: " << psarc.getLastError() << std::endl;
        return 1;
    }

    std::cout << "File count: " << psarc.getFileCount() << std::endl;
    std::cout << std::endl;

    // List files
    std::cout << "Files in archive:" << std::endl;
    auto files = psarc.getFileList();
    for (size_t i = 0; i < files.size() && i < 20; ++i) {
        const auto* entry = psarc.getEntry(files[i]);
        if (entry) {
            std::cout << "  " << files[i] << " (" << entry->uncompressedSize << " bytes)" << std::endl;
        }
    }

    if (files.size() > 20) {
        std::cout << "  ... and " << (files.size() - 20) << " more files" << std::endl;
    }

    std::cout << std::endl;

    // Extract all files
    std::cout << "Extracting to: " << outputDir << std::endl;
    if (psarc.extractAll(outputDir)) {
        std::cout << "Extraction successful!" << std::endl;
    } else {
        std::cerr << "Extraction had errors: " << psarc.getLastError() << std::endl;
        return 1;
    }

    return 0;
}
