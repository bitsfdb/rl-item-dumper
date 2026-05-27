#include "pch.h"
#include "JsonBuilder.h"
namespace JsonBuilder {
    bool Write(const std::vector<ItemRecord>&, const std::filesystem::path&, std::string&) { return false; }
    std::string WriteThumbnail(const std::filesystem::path&, const std::string&, const std::vector<uint8_t>&, std::string&) { return {}; }
}
