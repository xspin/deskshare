#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <iostream>

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


// 截取屏幕到 HBITMAP
static HBITMAP captureScreenToBitmap(size_t& width, size_t& height) {
    // 获取屏幕 DC
    // HDC hScreenDC = CreateDC("DISPLAY", NULL, NULL, NULL);
    HDC hScreenDC = GetDC(NULL);
    if (!hScreenDC) return NULL;

    // 获取屏幕尺寸
    width = GetDeviceCaps(hScreenDC, HORZRES);
    height = GetDeviceCaps(hScreenDC, VERTRES);

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

// 将 HBITMAP 转为 JPEG 字节流（输出到 vector）
static std::vector<unsigned char> bitmapToJpeg(HBITMAP hBitmap, int quality = 80) {
    std::vector<unsigned char> jpegData;
    if (!hBitmap) return jpegData;

    // 创建 GDI+ 位图
    Bitmap bitmap(hBitmap, NULL);
    if (bitmap.GetLastStatus() != Ok) return jpegData;

    // 创建内存流（存储 JPEG 数据）
    IStream* pStream = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &pStream) != S_OK) return jpegData;

    // 配置 JPEG 编码器参数（设置质量）
    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;

    // 获取 JPEG 编码器 CLSID
    CLSID clsidJpegEncoder;
    UINT numEncoders = 0;
    UINT sizeEncoders = 0;

    // 先获取编码器信息大小
    GetImageEncodersSize(&numEncoders, &sizeEncoders);
    if (sizeEncoders == 0) return jpegData;

    // 分配内存存储编码器信息
    std::vector<BYTE> encoderInfo(sizeEncoders);
    ImageCodecInfo* pEncoderInfo = (ImageCodecInfo*)encoderInfo.data();

    // 获取所有编码器信息并查找 JPEG 编码器
    GetImageEncoders(numEncoders, sizeEncoders, pEncoderInfo);
    for (UINT i = 0; i < numEncoders; ++i) {
        if (wcscmp(pEncoderInfo[i].MimeType, L"image/jpeg") == 0) {
            clsidJpegEncoder = pEncoderInfo[i].Clsid;
            break;
        }
    }

    // 将位图编码为 JPEG 并写入内存流
    if (bitmap.Save(pStream, &clsidJpegEncoder, &encoderParams) != Ok) {
        pStream->Release();
        return jpegData;
    }

    // 从内存流中读取 JPEG 数据到 vector
    HGLOBAL hGlobal = NULL;
    if (GetHGlobalFromStream(pStream, &hGlobal) != S_OK) {
        pStream->Release();
        return jpegData;
    }

    BYTE* pData = (BYTE*)GlobalLock(hGlobal);
    DWORD dataSize = GlobalSize(hGlobal);
    if (pData && dataSize > 0) {
        jpegData.assign(pData, pData + dataSize);
    }
    GlobalUnlock(hGlobal);
    pStream->Release();

    return jpegData;
}


std::vector<unsigned char> captureScreen(size_t& width, size_t& height, float quality) {
    static GdiplusInitializer gdiplusInitializer;

    HBITMAP hBitmap = captureScreenToBitmap(width, height);
    if (!hBitmap) {
        return {};
    }

    std::vector<unsigned char> jpegData = bitmapToJpeg(hBitmap, quality * 100);
    DeleteObject(hBitmap);  // 释放位图资源

    return jpegData;
}


std::pair<int,int> getScreenResolution() {
        // 方法 1：忽略系统缩放（获取物理像素）
    int width = GetSystemMetrics(SM_CXSCREEN);  // 屏幕宽度（像素）
    int height = GetSystemMetrics(SM_CYSCREEN); // 屏幕高度（像素）

        // 方法 2：适配系统缩放（获取逻辑分辨率，如 1920x1080 缩放 150% 后为 1280x720）
        // log_width = GetSystemMetrics(SM_CXFULLSCREEN);
        // log_height = GetSystemMetrics(SM_CYFULLSCREEN);
    return {width, height};
}

std::pair<int,int> getCursorLoc() {
    POINT pos;
    if (GetCursorPos(&pos)) {
        return {pos.x, pos.y};
    }
    std::cerr << "GetCursorPos failed: " << GetLastError() << std::endl;
    return {0, 0};
}