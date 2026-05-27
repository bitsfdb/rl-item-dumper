#include "pch.h"
#include "TextureExtractor.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace TextureExtractor {

struct ByteReader {
    const uint8_t* data;
    size_t size, pos;
    bool ok = true;

    template<typename T> T Read() {
        if (pos + sizeof(T) > size) { ok = false; return T{}; }
        T v{}; memcpy(&v, data + pos, sizeof(T)); pos += sizeof(T); return v;
    }
    void Skip(size_t n) { if (pos + n > size) ok = false; else pos += n; }
    bool AtEnd() const { return pos >= size; }
};

static void SkipProperties(ByteReader& r, const std::vector<std::string>& nameTable) {
    while (r.ok && !r.AtEnd()) {
        int32_t nameIdx = r.Read<int32_t>();
        r.Read<int32_t>(); // nameNumber
        if (nameIdx < 0 || nameIdx >= (int)nameTable.size()) break;
        if (nameTable[(size_t)nameIdx] == "None") break;
        r.Skip(8); // type FName
        int32_t propSize = r.Read<int32_t>();
        r.Read<int32_t>(); // array index
        if (propSize < 0 || propSize > 65536) break;
        r.Skip((size_t)propSize);
    }
}

struct BulkData {
    uint32_t flags{};
    int32_t  elementCount{};
    int32_t  sizeOnDisk{};
    int32_t  fileOffset{};
    std::vector<uint8_t> inlineBytes;
};

static BulkData ReadBulkData(ByteReader& r) {
    BulkData b{};
    b.flags        = r.Read<uint32_t>();
    b.elementCount = r.Read<int32_t>();
    b.sizeOnDisk   = r.Read<int32_t>();
    b.fileOffset   = r.Read<int32_t>();
    bool external  = (b.flags & 0x20) != 0;
    bool empty     = (b.sizeOnDisk <= 0 || b.elementCount <= 0);
    if (!external && !empty && r.ok && (r.pos + (size_t)b.sizeOnDisk <= r.size)) {
        b.inlineBytes.assign(r.data + r.pos, r.data + r.pos + (size_t)b.sizeOnDisk);
        r.pos += (size_t)b.sizeOnDisk;
    }
    return b;
}

static void DecodeDXT1Block(const uint8_t* blk, std::vector<uint8_t>& rgba, int w, int bx, int by) {
    uint16_t c0 = blk[0] | (blk[1] << 8);
    uint16_t c1 = blk[2] | (blk[3] << 8);
    uint32_t idx = blk[4] | (blk[5]<<8) | (blk[6]<<16) | ((uint32_t)blk[7]<<24);

    uint8_t R[4], G[4], B[4], A[4];
    R[0]=((c0>>11)&31)*255/31; G[0]=((c0>>5)&63)*255/63; B[0]=(c0&31)*255/31; A[0]=255;
    R[1]=((c1>>11)&31)*255/31; G[1]=((c1>>5)&63)*255/63; B[1]=(c1&31)*255/31; A[1]=255;
    if (c0 > c1) {
        R[2]=(2*R[0]+R[1])/3; G[2]=(2*G[0]+G[1])/3; B[2]=(2*B[0]+B[1])/3; A[2]=255;
        R[3]=(R[0]+2*R[1])/3; G[3]=(G[0]+2*G[1])/3; B[3]=(B[0]+2*B[1])/3; A[3]=255;
    } else {
        R[2]=(R[0]+R[1])/2; G[2]=(G[0]+G[1])/2; B[2]=(B[0]+B[1])/2; A[2]=255;
        R[3]=0; G[3]=0; B[3]=0; A[3]=0;
    }
    for (int py = 0; py < 4; ++py)
        for (int px = 0; px < 4; ++px) {
            int x = bx*4+px, y = by*4+py;
            if (x >= w) continue;
            uint32_t i = (idx >> (2*(py*4+px))) & 3;
            size_t p = ((size_t)y*w+x)*4;
            rgba[p]=R[i]; rgba[p+1]=G[i]; rgba[p+2]=B[i]; rgba[p+3]=A[i];
        }
}

static void DecodeDXT5Block(const uint8_t* blk, std::vector<uint8_t>& rgba, int w, int bx, int by) {
    uint8_t a0=blk[0], a1=blk[1];
    uint8_t AV[8]; AV[0]=a0; AV[1]=a1;
    if (a0>a1) {
        AV[2]=(6*a0+a1)/7; AV[3]=(5*a0+2*a1)/7; AV[4]=(4*a0+3*a1)/7;
        AV[5]=(3*a0+4*a1)/7; AV[6]=(2*a0+5*a1)/7; AV[7]=(a0+6*a1)/7;
    } else {
        AV[2]=(4*a0+a1)/5; AV[3]=(3*a0+2*a1)/5; AV[4]=(2*a0+3*a1)/5;
        AV[5]=(a0+4*a1)/5; AV[6]=0; AV[7]=255;
    }
    uint64_t ai = 0;
    for (int i = 2; i < 8; ++i) ai |= ((uint64_t)blk[i] << (8*(i-2)));

    uint16_t c0=blk[8]|(blk[9]<<8), c1=blk[10]|(blk[11]<<8);
    uint32_t ci=blk[12]|(blk[13]<<8)|(blk[14]<<16)|((uint32_t)blk[15]<<24);
    uint8_t R[4], G[4], B[4];
    R[0]=((c0>>11)&31)*255/31; G[0]=((c0>>5)&63)*255/63; B[0]=(c0&31)*255/31;
    R[1]=((c1>>11)&31)*255/31; G[1]=((c1>>5)&63)*255/63; B[1]=(c1&31)*255/31;
    R[2]=(2*R[0]+R[1])/3; G[2]=(2*G[0]+G[1])/3; B[2]=(2*B[0]+B[1])/3;
    R[3]=(R[0]+2*R[1])/3; G[3]=(G[0]+2*G[1])/3; B[3]=(B[0]+2*B[1])/3;

    for (int py = 0; py < 4; ++py)
        for (int px = 0; px < 4; ++px) {
            int x = bx*4+px, y = by*4+py;
            if (x >= w) continue;
            int pix = py*4+px;
            uint32_t ai3 = (uint32_t)((ai >> (3*pix)) & 7);
            uint32_t ci2 = (ci >> (2*pix)) & 3;
            size_t p = ((size_t)y*w+x)*4;
            rgba[p]=R[ci2]; rgba[p+1]=G[ci2]; rgba[p+2]=B[ci2]; rgba[p+3]=AV[ai3];
        }
}

static void PngWriteCallback(void* ctx, void* data, int size) {
    auto* buf = static_cast<std::vector<uint8_t>*>(ctx);
    const uint8_t* p = static_cast<const uint8_t*>(data);
    buf->insert(buf->end(), p, p + size);
}

std::vector<uint8_t> ExtractPNG(const std::vector<uint8_t>& objData,
                                 const std::vector<std::string>& nameTable,
                                 std::string& err) {
    ByteReader r{ objData.data(), objData.size(), 0 };

    SkipProperties(r, nameTable);
    if (!r.ok) { err = "Property parse failed"; return {}; }

    ReadBulkData(r); // source art (empty in cooked)
    if (!r.ok) { err = "Source art parse failed"; return {}; }

    int32_t mipCount = r.Read<int32_t>();
    if (!r.ok || mipCount <= 0 || mipCount > 128) { err = "Invalid mip count"; return {}; }

    for (int m = 0; m < mipCount; ++m) {
        BulkData bd = ReadBulkData(r);
        int32_t sizeX = r.Read<int32_t>();
        int32_t sizeY = r.Read<int32_t>();
        if (!r.ok) break;
        if (bd.inlineBytes.empty() || sizeX <= 0 || sizeY <= 0) continue;

        int64_t actualSize    = (int64_t)bd.sizeOnDisk;
        int64_t expectedDXT5  = (int64_t)sizeX * sizeY;
        int64_t expectedDXT1  = expectedDXT5 / 2;
        bool isDXT5 = (actualSize == expectedDXT5);
        bool isDXT1 = !isDXT5 && (actualSize == expectedDXT1);
        if (!isDXT5 && !isDXT1) isDXT5 = true; // fallback for thumbnails

        std::vector<uint8_t> rgba((size_t)(sizeX * sizeY * 4), 0);
        int blocksX = (sizeX + 3) / 4, blocksY = (sizeY + 3) / 4;
        int blockBytes = isDXT5 ? 16 : 8;
        const uint8_t* src = bd.inlineBytes.data();

        for (int by2 = 0; by2 < blocksY; ++by2)
            for (int bx2 = 0; bx2 < blocksX; ++bx2) {
                if (isDXT5) DecodeDXT5Block(src, rgba, sizeX, bx2, by2);
                else        DecodeDXT1Block(src, rgba, sizeX, bx2, by2);
                src += blockBytes;
            }

        std::vector<uint8_t> png;
        stbi_write_png_to_func(PngWriteCallback, &png, sizeX, sizeY, 4, rgba.data(), sizeX * 4);
        if (png.empty()) { err = "PNG encode failed"; return {}; }
        return png;
    }

    err = "No inline mip data found";
    return {};
}

} // namespace TextureExtractor
