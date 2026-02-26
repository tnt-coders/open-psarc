#pragma once

#include "sng_types.h"

#include <filesystem>

class SngXmlWriter
{
  public:
    static void Write(const sng::SngData& sng, const std::filesystem::path& output_path);
};
