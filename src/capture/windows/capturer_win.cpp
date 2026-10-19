#include <sys/param.h>
#include <windows.h>

#include <gdiplus.h> // must after windows.h
#include <iostream>
#include <vector>

namespace impl {
using namespace Gdiplus;

class GdiplusInitializer {
  public:
    GdiplusInitializer() {
        GdiplusStartup(&gdiplusToken, &gdiplusInput, NULL);
    }

    ~GdiplusInitializer() {
        GdiplusShutdown(gdiplusToken);
    }

  private:
    GdiplusStartupInput gdiplusInput;
    ULONG_PTR gdiplusToken;
};

std::pair<size_t, size_t> getScreenResolution() {
    // 逻辑像素
    size_t width = GetSystemMetrics(SM_CXSCREEN);  // 屏幕宽度（像素）
    size_t height = GetSystemMetrics(SM_CYSCREEN); // 屏幕高度（像素）

    // width = GetSystemMetrics(SM_CXFULLSCREEN);
    // height = GetSystemMetrics(SM_CYFULLSCREEN);

    // ensure even size
    width += (width & 1);
    height += (height & 1);
    return {width, height};
}

std::pair<size_t, size_t> getCursorLoc() {
    POINT pos;
    if (GetCursorPos(&pos)) {
        return {pos.x, pos.y};
    }
    std::cerr << "GetCursorPos failed: " << GetLastError() << std::endl;
    return {0, 0};
}

// 截取屏幕到 HBITMAP
static HBITMAP captureScreenToBitmap(size_t &width, size_t &height) {
    // 获取屏幕 DC
    // HDC hScreenDC = CreateDC("DISPLAY", NULL, NULL, NULL);
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC)
        return NULL;

    // 获取屏幕尺寸
    // width = GetDeviceCaps(hScreenDC, HORZRES);
    // height = GetDeviceCaps(hScreenDC, VERTRES);
    auto t = getScreenResolution();
    width = t.first;
    height = t.second;

    // 创建内存 DC 和位图
    HDC hMemDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    SelectObject(hMemDC, hBitmap);

    // 复制屏幕图像到内存位图
    BitBlt(hMemDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);

    // 释放临时资源
    DeleteDC(hMemDC);
    DeleteDC(hScreenDC);

    return hBitmap;
}

static bool HBITMAPToBGRA(std::vector<unsigned char> &bgra, HBITMAP hBitmap) {
    if (!hBitmap)
        return false;

    // 1. 获取位图尺寸
    BITMAP bm;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bm))
        return false;

    size_t width = bm.bmWidth;
    size_t height = bm.bmHeight;

    // 2. 创建兼容 DC 并选入位图
    HDC hdc = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // 3. 设置 BITMAPINFO（负高度 = 自上而下）
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // 负值：自上而下存储
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // 32 位 BGRA
    bmi.bmiHeader.biCompression = BI_RGB;

    // 4. 分配缓冲区并获取像素
    bgra.resize(width * height * 4);
    int lines = GetDIBits(hdcMem, hBitmap, 0, height, bgra.data(), &bmi, DIB_RGB_COLORS);

    // 清理 DC
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdc);

    if (lines == 0)
        return false;

    return true;
}

static CLSID *getJpegEncoder() {
    static bool cached = false;
    static CLSID clsidJpegEncoder{};

    if (cached) {
        return &clsidJpegEncoder;
    }
    // 获取 JPEG 编码器 CLSID
    UINT numEncoders = 0;
    UINT sizeEncoders = 0;

    // 先获取编码器信息大小
    GetImageEncodersSize(&numEncoders, &sizeEncoders);
    if (sizeEncoders == 0)
        return nullptr;

    // 分配内存存储编码器信息
    std::vector<BYTE> encoderInfo(sizeEncoders);
    ImageCodecInfo *pEncoderInfo = (ImageCodecInfo *)encoderInfo.data();

    // 获取所有编码器信息并查找 JPEG 编码器
    GetImageEncoders(numEncoders, sizeEncoders, pEncoderInfo);
    for (UINT i = 0; i < numEncoders; ++i) {
        if (wcscmp(pEncoderInfo[i].MimeType, L"image/jpeg") == 0) {
            clsidJpegEncoder = pEncoderInfo[i].Clsid;
            break;
        }
    }
    cached = true;

    return &clsidJpegEncoder;
}

// 将 HBITMAP 转为 JPEG 字节流（输出到 vector）
static bool bitmapToJpeg(std::vector<unsigned char> &jpegData, HBITMAP hBitmap, int quality = 80) {
    if (!hBitmap)
        return false;

    // 创建 GDI+ 位图
    Bitmap bitmap(hBitmap, NULL);
    if (bitmap.GetLastStatus() != Ok)
        return false;

    // 创建内存流（存储 JPEG 数据）
    IStream *pStream = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) != S_OK)
        return false;

    // 配置 JPEG 编码器参数（设置质量）
    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;

    CLSID *clsidJpegEncoder = getJpegEncoder();

    // 将位图编码为 JPEG 并写入内存流
    if (bitmap.Save(pStream, clsidJpegEncoder, &encoderParams) != Ok) {
        pStream->Release();
        return false;
    }

    // 从内存流中读取 JPEG 数据到 vector
    HGLOBAL hGlobal = NULL;
    if (GetHGlobalFromStream(pStream, &hGlobal) != S_OK) {
        pStream->Release();
        return false;
    }

    BYTE *pData = (BYTE *)GlobalLock(hGlobal);
    DWORD dataSize = GlobalSize(hGlobal);
    if (pData && dataSize > 0) {
        jpegData.assign(pData, pData + dataSize);
    }
    GlobalUnlock(hGlobal);
    pStream->Release();

    return true;
}

bool captureJpeg(std::vector<unsigned char> &jpeg, size_t &width, size_t &height, float quality) {
    static GdiplusInitializer gdiplusInitializer;

    HBITMAP hBitmap = captureScreenToBitmap(width, height);
    if (!hBitmap) {
        return false;
    }

    bool ret = bitmapToJpeg(jpeg, hBitmap, quality * 100);
    DeleteObject(hBitmap); // 释放位图资源

    return ret;
}

static void scaleRGBA_Nearest(const unsigned char *src, int srcW, int srcH, unsigned char *dst,
                              int dstW, int dstH) {
    for (int y = 0; y < dstH; y++) {
        int srcY = y * srcH / dstH;
        for (int x = 0; x < dstW; x++) {
            int srcX = x * srcW / dstW;
            int si = (srcY * srcW + srcX) * 4;
            int di = (y * dstW + x) * 4;
            dst[di] = src[si];
            dst[di + 1] = src[si + 1];
            dst[di + 2] = src[si + 2];
            dst[di + 3] = src[si + 3]; // Alpha is ignored
        }
    }
}

/*
static void scaleRGBA_Bilinear(const unsigned char *src, int srcW, int srcH, unsigned char *dst,
                               int dstW, int dstH) {
    for (int y = 0; y < dstH; y++) {
        float srcYf = (float)y * srcH / dstH;
        int y0 = (int)srcYf;
        int y1 = std::min(y0 + 1, srcH - 1);
        float fy = srcYf - y0;

        for (int x = 0; x < dstW; x++) {
            float srcXf = (float)x * srcW / dstW;
            int x0 = (int)srcXf;
            int x1 = std::min(x0 + 1, srcW - 1);
            float fx = srcXf - x0;

            int i00 = (y0 * srcW + x0) * 4;
            int i01 = (y0 * srcW + x1) * 4;
            int i10 = (y1 * srcW + x0) * 4;
            int i11 = (y1 * srcW + x1) * 4;
            int di = (y * dstW + x) * 4;

            for (int c = 0; c < 4; c++) { // 4 个通道都插值
                float top = src[i00 + c] * (1 - fx) + src[i01 + c] * fx;
                float bot = src[i10 + c] * (1 - fx) + src[i11 + c] * fx;
                dst[di + c] = (unsigned char)(top * (1 - fy) + bot * fy);
            }
        }
    }
}
*/

bool captureBgra(std::vector<unsigned char> &bgra, size_t &width, size_t &height) {
    HBITMAP hBitmap = captureScreenToBitmap(width, height);
    if (!hBitmap) {
        return false;
    }

    bool ret = HBITMAPToBGRA(bgra, hBitmap);
    DeleteObject(hBitmap);

    return ret;
}

bool captureRgba(std::vector<unsigned char> &rgba, size_t &width, size_t &height) {
    bool ret = captureBgra(rgba, width, height);

    // BGRA → RGBA：交换 R 和 B
    for (size_t i = 0; i < width * height; ++i) {
        std::swap(rgba[i * 4], rgba[i * 4 + 2]); // B <-> R
    }
    return ret;
}

bool captureRgbaScaled(std::vector<unsigned char> &rgba, size_t width, size_t height) {
    static std::vector<unsigned char> buf;

    size_t w, h;
    if (!captureRgba(buf, w, h)) {
        return false;
    }

    rgba.resize(width * height * 4);

    // scaleRGBA_Bilinear(buf.data(), w, h, rgba.data(), width, height);
    scaleRGBA_Nearest(buf.data(), w, h, rgba.data(), width, height);
    return true;
}
} // namespace impl
