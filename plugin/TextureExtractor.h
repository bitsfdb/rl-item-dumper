#pragma once
#include "pch.h"
namespace TextureExtractor {
    std::vector<uint8_t> ExtractPNG(const std::vector<uint8_t>& objData,
                                    const std::vector<std::string>& nameTable,
                                    std::string& err);
}
