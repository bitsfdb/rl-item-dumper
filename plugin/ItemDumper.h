#pragma once
#include "pch.h"

class ItemDumper : public BakkesMod::Plugin::BakkesModPlugin {
public:
    void onLoad() override;
    void onUnload() override;

private:
    void RunDump(const std::filesystem::path& outputDir);
};
