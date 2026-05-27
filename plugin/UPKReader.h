#pragma once
#include "pch.h"
namespace UPKReader {
    struct Package {
        std::vector<uint8_t> data;
        std::vector<std::string> nameTable;
        struct Export { std::string name; int32_t offset; int32_t size; };
        std::vector<Export> exportTable;
    };
    bool Open(const std::filesystem::path& upkPath,
              const std::vector<std::vector<uint8_t>>& keys,
              Package& out, std::string& err);
    std::vector<uint8_t> GetObjectData(const Package& pkg, const std::string& exportName);
    std::vector<std::vector<uint8_t>> LoadKeys(const std::filesystem::path& keysFile);
}
