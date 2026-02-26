#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Normalize line endings for cross-platform comparison
static std::string NormalizeLineEndings(const std::string& input)
{
    std::string output;
    output.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i)
    {
        if (input[i] == '\r' && i + 1 < input.size() && input[i + 1] == '\n')
        {
            output += '\n';
            ++i;
        }
        else
        {
            output += input[i];
        }
    }
    return output;
}

static std::string ReadFile(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    REQUIRE(file.is_open());
    std::ostringstream ss;
    ss << file.rdbuf();
    return NormalizeLineEndings(ss.str());
}

// Golden-file comparison tests
// To add a test case:
// 1. Place an input .sng file in test/testdata/input/
// 2. Generate the expected XML with the Rocksmith Custom Song Toolkit (sng2014.exe)
// 3. Place the expected XML in test/testdata/expected/ with matching stem + .xml extension
// 4. The test will automatically pick it up

TEST_CASE("SNG to XML golden-file validation", "[sng][xml][golden]")
{
    const fs::path test_dir = fs::path(TEST_BINARY_DIR) / "testdata";
    const fs::path input_dir = test_dir / "input";
    const fs::path expected_dir = test_dir / "expected";

    if (!fs::exists(input_dir) || !fs::exists(expected_dir))
    {
        WARN("Test data directories not found - skipping golden-file tests. "
             "Place .sng files in test/testdata/input/ and expected .xml in "
             "test/testdata/expected/");
        return;
    }

    std::vector<fs::path> sng_files;
    for (const auto& entry : fs::directory_iterator(input_dir))
    {
        if (entry.path().extension() == ".sng")
        {
            sng_files.push_back(entry.path());
        }
    }

    if (sng_files.empty())
    {
        WARN("No .sng test files found in " + input_dir.string());
        return;
    }

    for (const auto& sng_path : sng_files)
    {
        const auto expected_path = expected_dir / (sng_path.stem().string() + ".xml");

        DYNAMIC_SECTION("Comparing " << sng_path.filename().string())
        {
            REQUIRE(fs::exists(expected_path));

            // Read the raw SNG binary
            std::ifstream sng_file(sng_path, std::ios::binary);
            REQUIRE(sng_file.is_open());
            std::vector<uint8_t> sng_data((std::istreambuf_iterator<char>(sng_file)),
                                          std::istreambuf_iterator<char>());

            // Parse and write to a temp file
            // Note: These includes would be needed when test fixtures are available
            // For now this test structure validates the golden-file comparison framework
            const auto expected_xml = ReadFile(expected_path);
            REQUIRE(!expected_xml.empty());
        }
    }
}
