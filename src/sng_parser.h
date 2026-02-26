#pragma once

#include "sng_types.h"

#include <cstdint>
#include <span>

class SngParser
{
  public:
    [[nodiscard]] static sng::SngData Parse(std::span<const uint8_t> data);
};
