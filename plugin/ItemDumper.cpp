#include "pch.h"
#include "ItemDumper.h"
#include "RLPathFinder.h"
#include "UPKReader.h"
#include "TextureExtractor.h"
#include "JsonBuilder.h"

BAKKESMOD_PLUGIN(ItemDumper, "Item Dumper", "1.0", PLUGINTYPE_FREEPLAY)

static std::string QualityLabel(int q) {
    static const char* labels[] = {
        "Common","Uncommon","Rare","VeryRare","Import","Exotic","BlackMarket","Premium","Limited","Legacy"
    };
    if (q >= 0 && q < 10) return labels[q];
    return std::to_string(q);
}

void ItemDumper::onLoad() {
    cvarManager->registerCvar("itemdumper_output_path", "",
        "Output directory for items.json and thumbnails");

    cvarManager->registerNotifier("dump_items", [this](std::vector<std::string>) {
        gameWrapper->SetTimeout([this](GameWrapper*) {
            auto pathCvar = cvarManager->getCvar("itemdumper_output_path");
            std::string pathStr = pathCvar ? pathCvar.getStringValue() : "";
            if (pathStr.empty()) {
                pathStr = std::string(getenv("APPDATA"))
                          + "/bakkesmod/bakkesmod/data/ItemDumper";
            }
            std::filesystem::create_directories(pathStr);
            RunDump(pathStr);
        }, 0.0f);
    }, "Dump all RL items with thumbnails to items.json", PERMISSION_ALL);
}

void ItemDumper::onUnload() {}

void ItemDumper::RunDump(const std::filesystem::path& outputDir) {
    auto log = [&](const std::string& msg) { cvarManager->log("ItemDumper: " + msg); };

    // 1. Live item catalog
    auto itemsWrapper = gameWrapper->GetItemsWrapper();
    auto products     = itemsWrapper.GetAllProducts();
    int  count        = products.Count();
    log("Found " + std::to_string(count) + " products");

    // 2. RL install path
    auto rlPath = RLPathFinder::FindRocketLeaguePath();
    if (rlPath.empty())
        log("WARNING: RL install path not found — thumbnails will be skipped");

    // 3. AES keys
    auto keys = UPKReader::LoadKeys(outputDir / "keys.txt");
    if (keys.empty())
        log("WARNING: keys.txt not found — thumbnails skipped");

    auto upkDir = rlPath / "TAGame" / "CookedPCConsole";

    // 4. Package cache
    std::unordered_map<std::string, UPKReader::Package> pkgCache;
    auto getPackage = [&](const std::string& pkgName) -> const UPKReader::Package* {
        if (pkgName.empty() || rlPath.empty() || keys.empty()) return nullptr;
        auto it = pkgCache.find(pkgName);
        if (it != pkgCache.end()) return &it->second;
        auto upkPath = upkDir / (pkgName + ".upk");
        if (!std::filesystem::exists(upkPath)) return nullptr;
        std::string err;
        UPKReader::Package pkg;
        if (!UPKReader::Open(upkPath, keys, pkg, err)) {
            log("UPK failed [" + pkgName + "]: " + err);
            return nullptr;
        }
        pkgCache[pkgName] = std::move(pkg);
        return &pkgCache[pkgName];
    };

    // 5. Build records
    std::vector<JsonBuilder::ItemRecord> records;
    records.reserve((size_t)count);

    for (int i = 0; i < count; ++i) {
        auto prod = products.Get(i);
        if (!prod) continue;

        JsonBuilder::ItemRecord rec;
        rec.id           = prod.GetID();
        rec.label        = prod.GetLabel().ToString();
        rec.longLabel    = prod.GetLongLabel().ToString();
        rec.assetPackage = prod.GetAssetPackageName();
        rec.assetPath    = prod.GetAssetPath().ToString();
        rec.quality      = prod.GetQuality();
        rec.qualityLabel = QualityLabel(rec.quality);
        rec.isPaintable  = prod.IsPaintable();

        auto slot = prod.GetSlot();
        if (slot) {
            rec.slot      = slot.GetLabel().ToString();
            rec.slotIndex = slot.GetSlotIndex();
        }

        rec.thumbnailPackage = prod.GetThumbnailPackageName();
        rec.thumbnailAsset   = prod.GetThumbnailAssetName();

        // 6. Extract thumbnail
        if (!rec.thumbnailPackage.empty() && !rec.thumbnailAsset.empty()) {
            const UPKReader::Package* pkg = getPackage(rec.thumbnailPackage);
            if (pkg) {
                auto objData = UPKReader::GetObjectData(*pkg, rec.thumbnailAsset);
                if (!objData.empty()) {
                    std::string texErr;
                    auto png = TextureExtractor::ExtractPNG(objData, pkg->nameTable, texErr);
                    if (!png.empty()) {
                        std::string b64;
                        rec.thumbnailPath   = JsonBuilder::WriteThumbnail(outputDir, rec.thumbnailAsset, png, b64);
                        rec.thumbnailBase64 = b64;
                    } else {
                        log("Texture failed [" + rec.thumbnailAsset + "]: " + texErr);
                    }
                }
            }
        }

        records.push_back(std::move(rec));
        if (i % 100 == 0) log("Progress: " + std::to_string(i) + "/" + std::to_string(count));
    }

    // 7. Write JSON
    std::string jsonErr;
    if (!JsonBuilder::Write(records, outputDir, jsonErr))
        log("ERROR writing JSON: " + jsonErr);
    else
        log("Done! " + std::to_string(records.size()) + " items → " + (outputDir / "items.json").string());
}
