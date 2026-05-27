#include "pch.h"
#include "ItemDumper.h"

BAKKESMOD_PLUGIN(ItemDumper, "Item Dumper", "1.0", PLUGINTYPE_FREEPLAY)

void ItemDumper::onLoad() {
    cvarManager->registerCvar("itemdumper_output_path", "", "Output directory for items.json and thumbnails");

    cvarManager->registerNotifier("dump_items", [this](std::vector<std::string>) {
        auto pathCvar = cvarManager->getCvar("itemdumper_output_path");
        std::string pathStr = pathCvar ? pathCvar.getStringValue() : "";
        if (pathStr.empty()) {
            std::string appdata = std::string(getenv("APPDATA"));
            pathStr = appdata + "/bakkesmod/bakkesmod/data/ItemDumper";
        }
        std::filesystem::create_directories(pathStr);
        RunDump(pathStr);
    }, "Dump all RL items to items.json", PERMISSION_ALL);
}

void ItemDumper::onUnload() {}

void ItemDumper::RunDump(const std::filesystem::path& outputDir) {
    cvarManager->log("ItemDumper: starting dump to " + outputDir.string());
    // Pipeline wired in Task 10
}
