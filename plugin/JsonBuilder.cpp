#include "pch.h"
#include "JsonBuilder.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>

namespace JsonBuilder {

static std::string Base64Encode(const std::vector<uint8_t>& data) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i+1 < data.size()) n |= (uint32_t)data[i+1] << 8;
        if (i+2 < data.size()) n |= (uint32_t)data[i+2];
        out += b64[(n>>18)&63]; out += b64[(n>>12)&63];
        out += (i+1 < data.size()) ? b64[(n>>6)&63]  : '=';
        out += (i+2 < data.size()) ? b64[n&63]        : '=';
    }
    return out;
}

static std::string UtcNow() {
    auto now = std::chrono::system_clock::now();
    auto t   = std::chrono::system_clock::to_time_t(now);
    std::ostringstream ss;
    ss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

bool Write(const std::vector<ItemRecord>& items,
           const std::filesystem::path& outputDir,
           std::string& err) {
    std::filesystem::create_directories(outputDir / "thumbnails");

    nlohmann::json root;
    root["generated_at"] = UtcNow();
    root["item_count"]   = (int)items.size();
    root["items"]        = nlohmann::json::array();

    for (const auto& item : items) {
        nlohmann::json j;
        j["id"]                = item.id;
        j["label"]             = item.label;
        j["long_label"]        = item.longLabel;
        j["asset_package"]     = item.assetPackage;
        j["asset_path"]        = item.assetPath;
        j["slot"]              = item.slot;
        j["slot_index"]        = item.slotIndex;
        j["quality"]           = item.quality;
        j["quality_label"]     = item.qualityLabel;
        j["is_paintable"]      = item.isPaintable;
        j["thumbnail_package"] = item.thumbnailPackage;
        j["thumbnail_asset"]   = item.thumbnailAsset;
        if (!item.thumbnailPath.empty()) {
            j["thumbnail_path"]   = item.thumbnailPath;
            j["thumbnail_base64"] = item.thumbnailBase64.empty()
                ? nlohmann::json(nullptr)
                : nlohmann::json("data:image/png;base64," + item.thumbnailBase64);
        } else {
            j["thumbnail_path"]   = nullptr;
            j["thumbnail_base64"] = nullptr;
        }
        root["items"].push_back(j);
    }

    auto jsonPath = outputDir / "items.json";
    std::ofstream f(jsonPath);
    if (!f) { err = "Cannot write: " + jsonPath.string(); return false; }
    f << root.dump(2);
    return true;
}

std::string WriteThumbnail(const std::filesystem::path& outputDir,
                            const std::string& assetName,
                            const std::vector<uint8_t>& pngBytes,
                            std::string& outBase64) {
    if (pngBytes.empty()) return "";
    auto thumbDir = outputDir / "thumbnails";
    std::filesystem::create_directories(thumbDir);
    auto filePath = thumbDir / (assetName + ".png");
    std::ofstream f(filePath, std::ios::binary);
    if (!f) return "";
    f.write(reinterpret_cast<const char*>(pngBytes.data()), (std::streamsize)pngBytes.size());
    outBase64 = Base64Encode(pngBytes);
    return "thumbnails/" + assetName + ".png";
}

} // namespace JsonBuilder
