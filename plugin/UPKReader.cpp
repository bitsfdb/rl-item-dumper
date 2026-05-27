#include "pch.h"
#include "UPKReader.h"
namespace UPKReader {
    bool Open(const std::filesystem::path&, const std::vector<std::vector<uint8_t>>&, Package&, std::string&) { return false; }
    std::vector<uint8_t> GetObjectData(const Package&, const std::string&) { return {}; }
    std::vector<std::vector<uint8_t>> LoadKeys(const std::filesystem::path&) { return {}; }
}
