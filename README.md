# rl-item-dumper

A [BakkesMod](https://bakkesmod.com/) plugin for Rocket League that extracts every in-game item — labels, slot types, quality, and thumbnails — and writes them to `items.json` alongside PNG thumbnail files.

## Output

```
ItemDumper/
├── items.json          # full catalog with base64-embedded thumbnails
└── thumbnails/
    ├── T_Body_Octane.png
    ├── T_Decal_Shisa.png
    └── ...
```

Each entry in `items.json`:
```json
{
  "id": 23,
  "label": "Octane",
  "long_label": "Octane",
  "asset_package": "Body_Octane",
  "asset_path": "...",
  "slot": "Body",
  "slot_index": 0,
  "quality": 0,
  "quality_label": "Common",
  "is_paintable": false,
  "thumbnail_package": "Thumbnails_Body",
  "thumbnail_asset": "T_Body_Octane",
  "thumbnail_path": "thumbnails/T_Body_Octane.png",
  "thumbnail_base64": "data:image/png;base64,..."
}
```

## Setup

1. **AES keys** — copy `keys.txt` from [RLUPKTools](https://github.com/CrunchyRL/RLUPKTools) into the output folder (default `%APPDATA%/bakkesmod/bakkesmod/data/ItemDumper/`). Without keys, item metadata is still dumped but thumbnails are skipped.

2. **Install** — copy `ItemDumper.dll` to `%APPDATA%/bakkesmod/bakkesmod/plugins/` (the build does this automatically on post-build).

3. **Load** — open BakkesMod → Plugins → Plugin Manager → enable **Item Dumper**.

## Usage

Open the BakkesMod console (`F6`) and run:

```
dump_items
```

To use a custom output path:

```
itemdumper_output_path "C:/my/output/dir"
dump_items
```

## Building

Prerequisites: Visual Studio 2022, CMake ≥ 3.20, BakkesMod SDK installed (`%APPDATA%/bakkesmod/bakkesmod/bakkesmodsdk`).

```powershell
cd plugin
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The DLL is automatically copied to the BakkesMod plugins folder on a successful build.

## How it works

1. `GetItemsWrapper().GetAllProducts()` — live item catalog from the running game
2. Windows registry — auto-detects the Rocket League install path (Steam or Epic)
3. `keys.txt` — AES-256-ECB keys for UPK decryption (game-specific, not included)
4. UPK pipeline (pure C++, no Python):
   - AES-ECB decrypt via Windows BCrypt
   - zlib chunk decompress via miniz
   - UE3 name/export table parse
   - `UTexture2D` bulk mip data extract
   - DXT1/DXT5 block decode → RGBA
   - PNG encode via `stb_image_write`
5. `nlohmann/json` — serialize to `items.json`

## Third-party

| Library | License |
|---------|---------|
| [miniz](https://github.com/richgel999/miniz) | MIT |
| [stb_image_write](https://github.com/nothings/stb) | Public domain |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT |
