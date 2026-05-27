#include "pch.h"
#include "UPKReader.h"
extern "C" {
#include "miniz.h"
}

namespace UPKReader {

static constexpr uint32_t UPK_MAGIC = 0x9E2A83C1;

static std::vector<uint8_t> ReadFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto sz = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> buf((size_t)sz);
    f.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

static std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    std::string h = hex;
    if (h.size() >= 2 && h[0] == '0' && (h[1] == 'x' || h[1] == 'X')) h = h.substr(2);
    h.erase(std::remove_if(h.begin(), h.end(), [](char c){ return c == ' ' || c == '-'; }), h.end());
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        uint8_t b = (uint8_t)std::stoi(h.substr(i, 2), nullptr, 16);
        out.push_back(b);
    }
    return out;
}

static bool AesEcbDecrypt(std::vector<uint8_t>& data, const std::vector<uint8_t>& key) {
    if (key.size() != 16 && key.size() != 24 && key.size() != 32) return false;
    size_t paddedSize = (data.size() + 15) & ~(size_t)15;
    data.resize(paddedSize, 0);

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0)))
        return false;
    BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_ECB, (ULONG)sizeof(BCRYPT_CHAIN_MODE_ECB), 0);

    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (!BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0,
            (PUCHAR)key.data(), (ULONG)key.size(), 0))) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return false;
    }

    std::vector<uint8_t> out(paddedSize);
    ULONG cbResult = 0;
    bool ok = BCRYPT_SUCCESS(BCryptDecrypt(hKey,
        (PUCHAR)data.data(), (ULONG)data.size(),
        nullptr, nullptr, 0,
        (PUCHAR)out.data(), (ULONG)out.size(), &cbResult, 0));

    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (ok) data = std::move(out);
    return ok;
}

static bool CheckMagic(const std::vector<uint8_t>& d) {
    if (d.size() < 4) return false;
    uint32_t sig = d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
    return sig == UPK_MAGIC;
}

static bool TryDecrypt(std::vector<uint8_t>& data, const std::vector<std::vector<uint8_t>>& keys) {
    if (CheckMagic(data)) return true;
    for (const auto& key : keys) {
        std::vector<uint8_t> attempt = data;
        if (AesEcbDecrypt(attempt, key) && CheckMagic(attempt)) {
            data = std::move(attempt);
            return true;
        }
    }
    return false;
}

static bool DecompressChunks(std::vector<uint8_t>& data) {
    size_t pos = 0;
    auto r32 = [&]() -> int32_t {
        if (pos + 4 > data.size()) return 0;
        int32_t v = data[pos] | (data[pos+1]<<8) | (data[pos+2]<<16) | (data[pos+3]<<24);
        pos += 4; return v;
    };
    auto r16 = [&]() -> uint16_t {
        if (pos + 2 > data.size()) return 0;
        uint16_t v = data[pos] | (data[pos+1]<<8); pos += 2; return v;
    };
    auto skipFString = [&]() {
        int32_t len = r32();
        if (len > 0) pos += (size_t)len;
        else if (len < 0) pos += (size_t)(-len) * 2;
    };

    r32();           // magic
    r16(); r16();    // version, licensee
    r32();           // totalHeaderSize
    skipFString();   // folderName
    r32();           // packageFlags
    r32(); r32();    // nameCount, nameOffset
    r32(); r32();    // exportCount, exportOffset
    r32(); r32();    // importCount, importOffset
    r32();           // dependsOffset
    pos += 16;       // GUID
    int32_t genCount = r32();
    pos += (size_t)(genCount * 8);
    r32(); r32();    // engine version, cooker version

    uint32_t compressionFlags = (uint32_t)r32();
    int32_t chunkCount = r32();
    if (chunkCount == 0 || compressionFlags == 0) return true;

    struct ChunkEntry { int32_t uncompOff, uncompSize, compOff, compSize; };
    std::vector<ChunkEntry> chunks((size_t)chunkCount);
    for (auto& c : chunks) {
        c.uncompOff  = r32();
        c.uncompSize = r32();
        c.compOff    = r32();
        c.compSize   = r32();
    }

    int32_t totalUncomp = 0;
    for (auto& c : chunks)
        totalUncomp = std::max(totalUncomp, c.uncompOff + c.uncompSize);
    std::vector<uint8_t> out((size_t)totalUncomp);

    for (auto& c : chunks) {
        if (c.compOff < 0 || (size_t)c.compOff >= data.size()) continue;
        const uint8_t* cp = data.data() + c.compOff;

        // Inner chunk header: Tag(4) BlockSize(4) CompressedSize(4) UncompressedSize(4)
        int32_t blockSize = cp[4]|(cp[5]<<8)|(cp[6]<<16)|(cp[7]<<24);
        int32_t uSize     = cp[12]|(cp[13]<<8)|(cp[14]<<16)|(cp[15]<<24);

        int32_t blockCount = (uSize + blockSize - 1) / blockSize;
        const uint8_t* hp = cp + 16;
        std::vector<std::pair<int32_t,int32_t>> blocks((size_t)blockCount);
        for (auto& b : blocks) {
            b.first  = hp[0]|(hp[1]<<8)|(hp[2]<<16)|(hp[3]<<24);
            b.second = hp[4]|(hp[5]<<8)|(hp[6]<<16)|(hp[7]<<24);
            hp += 8;
        }

        uint8_t* outPtr = out.data() + c.uncompOff;
        const uint8_t* srcPtr = hp;
        for (auto& b : blocks) {
            mz_ulong destLen = (mz_ulong)b.second;
            if (mz_uncompress(outPtr, &destLen, srcPtr, (mz_ulong)b.first) != MZ_OK)
                return false;
            outPtr += b.second;
            srcPtr += b.first;
        }
    }

    int32_t headerEnd = chunks[0].uncompOff;
    if (headerEnd > 0 && (size_t)headerEnd < data.size()) {
        std::vector<uint8_t> final_(data.begin(), data.begin() + headerEnd);
        final_.insert(final_.end(), out.begin() + headerEnd, out.end());
        data = std::move(final_);
    } else {
        data = std::move(out);
    }
    return true;
}

static std::string ReadFStringAt(const std::vector<uint8_t>& d, size_t& pos) {
    if (pos + 4 > d.size()) return "";
    int32_t len = d[pos] | (d[pos+1]<<8) | (d[pos+2]<<16) | (d[pos+3]<<24);
    pos += 4;
    if (len == 0) return "";
    if (len > 0) {
        if (pos + (size_t)len > d.size()) return "";
        std::string s(reinterpret_cast<const char*>(d.data() + pos), (size_t)len);
        pos += (size_t)len;
        if (!s.empty() && s.back() == '\0') s.pop_back();
        return s;
    }
    int32_t charCount = -len;
    if (pos + (size_t)(charCount * 2) > d.size()) return "";
    std::wstring ws((size_t)charCount, L'\0');
    memcpy(ws.data(), d.data() + pos, (size_t)(charCount * 2));
    pos += (size_t)(charCount * 2);
    if (!ws.empty() && ws.back() == L'\0') ws.pop_back();
    std::string s; s.reserve(ws.size());
    for (wchar_t wc : ws) s += (char)(wc & 0xFF);
    return s;
}

static bool ParseTables(Package& pkg, std::string& err) {
    const auto& d = pkg.data;
    if (d.size() < 4) { err = "Package too small"; return false; }

    size_t pos = 0;
    auto r32 = [&]() -> int32_t {
        if (pos + 4 > d.size()) return 0;
        int32_t v = d[pos] | (d[pos+1]<<8) | (d[pos+2]<<16) | (d[pos+3]<<24);
        pos += 4; return v;
    };
    auto r16 = [&]() -> uint16_t {
        if (pos + 2 > d.size()) return 0;
        uint16_t v = d[pos] | (d[pos+1]<<8); pos += 2; return v;
    };
    auto skipFS = [&]() { ReadFStringAt(d, pos); };

    r32();          // magic
    r16(); r16();   // version, licensee
    r32();          // totalHeaderSize
    skipFS();       // folderName
    r32();          // packageFlags

    int32_t nameCount   = r32(), nameOffset   = r32();
    int32_t exportCount = r32(), exportOffset = r32();
    r32(); r32();   // importCount, importOffset
    r32();          // dependsOffset
    pos += 16;      // GUID
    int32_t genCount = r32();
    pos += (size_t)(genCount * 8);
    r32(); r32();   // engine version, cooker version

    // Name table
    pkg.nameTable.reserve((size_t)nameCount);
    pos = (size_t)nameOffset;
    for (int i = 0; i < nameCount; ++i) {
        std::string name = ReadFStringAt(d, pos);
        pos += 8; // uint64 flags
        pkg.nameTable.push_back(std::move(name));
    }

    // Export table — each entry is 72 bytes in RL UE3
    pkg.exportTable.reserve((size_t)exportCount);
    pos = (size_t)exportOffset;
    for (int i = 0; i < exportCount; ++i) {
        size_t entryStart = pos;
        r32(); r32(); r32();        // classIndex, superIndex, outerIndex
        int32_t nameIdx = r32();
        r32();                      // nameNumber
        r32();                      // archetypeIndex
        pos += 8;                   // objectFlags (uint64)
        int32_t serialSize   = r32();
        int32_t serialOffset = r32();
        pos = entryStart + 72;      // skip remainder of entry

        Package::Export e;
        e.name   = (nameIdx >= 0 && nameIdx < (int)pkg.nameTable.size())
                   ? pkg.nameTable[(size_t)nameIdx] : "";
        e.offset = serialOffset;
        e.size   = serialSize;
        pkg.exportTable.push_back(std::move(e));
    }
    return true;
}

std::vector<std::vector<uint8_t>> LoadKeys(const std::filesystem::path& keysFile) {
    std::vector<std::vector<uint8_t>> result;
    std::ifstream f(keysFile);
    if (!f) return result;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        auto key = HexToBytes(line);
        if (key.size() == 16 || key.size() == 24 || key.size() == 32)
            result.push_back(std::move(key));
    }
    return result;
}

bool Open(const std::filesystem::path& upkPath,
          const std::vector<std::vector<uint8_t>>& keys,
          Package& out, std::string& err) {
    out = {};
    auto raw = ReadFile(upkPath);
    if (raw.empty()) { err = "Cannot read: " + upkPath.string(); return false; }
    if (!TryDecrypt(raw, keys)) {
        err = "AES decrypt failed (no valid key) for: " + upkPath.filename().string();
        return false;
    }
    if (!DecompressChunks(raw)) {
        err = "Zlib decompress failed for: " + upkPath.filename().string();
        return false;
    }
    out.data = std::move(raw);
    if (!ParseTables(out, err)) return false;
    return true;
}

std::vector<uint8_t> GetObjectData(const Package& pkg, const std::string& exportName) {
    for (const auto& e : pkg.exportTable) {
        if (e.name == exportName && e.offset > 0 && e.size > 0) {
            size_t end = (size_t)(e.offset + e.size);
            if (end <= pkg.data.size())
                return std::vector<uint8_t>(pkg.data.begin() + e.offset, pkg.data.begin() + end);
        }
    }
    return {};
}

} // namespace UPKReader
