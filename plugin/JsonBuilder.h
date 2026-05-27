#pragma once
#include "pch.h"
namespace JsonBuilder {
    struct ItemRecord {
        int id{};
        std::string label, longLabel, assetPackage, assetPath;
        std::string slot; int slotIndex{};
        int quality{}; std::string qualityLabel;
        bool isPaintable{};
        std::string thumbnailPackage, thumbnailAsset;
        std::string thumbnailPath, thumbnailBase64;
    };
    bool Write(const std::vector<ItemRecord>& items,
               const std::filesystem::path& outputDir,
               std::string& err);
    std::string WriteThumbnail(const std::filesystem::path& outputDir,
                               const std::string& assetName,
                               const std::vector<uint8_t>& pngBytes,
                               std::string& outBase64);
}
