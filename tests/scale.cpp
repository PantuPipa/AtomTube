#include <windows.h>
#include <cstdio>
#include <vector>

static int render(HDC dc,const BITMAPINFO& source,const std::vector<unsigned>& pixels,
                  unsigned* output,int mode) {
    ZeroMemory(output,1024u*576u*4u);
    SetStretchBltMode(dc,mode);
    if (mode==HALFTONE) SetBrushOrgEx(dc,0,0,nullptr);
    if (StretchDIBits(dc,0,0,1024,576,0,0,1280,720,pixels.data(),&source,DIB_RGB_COLORS,SRCCOPY)==GDI_ERROR)
        return -1;
    int intermediate=0;
    for (size_t i=0;i<1024u*576u;++i)
        if (output[i]!=0x00000000 && output[i]!=0x0000E0C0) ++intermediate;
    return intermediate;
}

static double benchmark(HDC dc,const BITMAPINFO& source,const std::vector<unsigned>& pixels,int mode) {
    SetStretchBltMode(dc,mode);
    if (mode==HALFTONE) SetBrushOrgEx(dc,0,0,nullptr);
    LARGE_INTEGER frequency,start,end;
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&start);
    for (int i=0;i<120;++i)
        StretchDIBits(dc,0,0,1024,576,0,0,1280,720,pixels.data(),&source,DIB_RGB_COLORS,SRCCOPY);
    QueryPerformanceCounter(&end);
    return 1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart/120.0;
}

int wmain() {
    std::vector<unsigned> pixels(1280u*720u);
    for (unsigned y=0;y<720;++y)
        for (unsigned x=0;x<1280;++x)
            pixels[y*1280+x]=x<160+(y*4)/3 ? 0x0000E0C0 : 0x00000000;
    BITMAPINFO source={}; source.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    source.bmiHeader.biWidth=1280; source.bmiHeader.biHeight=-720;
    source.bmiHeader.biPlanes=1; source.bmiHeader.biBitCount=32; source.bmiHeader.biCompression=BI_RGB;
    BITMAPINFO target=source; target.bmiHeader.biWidth=1024; target.bmiHeader.biHeight=-576;
    void* bits=nullptr;
    HDC dc=CreateCompatibleDC(nullptr);
    HBITMAP bitmap=CreateDIBSection(dc,&target,DIB_RGB_COLORS,&bits,nullptr,0);
    HGDIOBJ old=bitmap ? SelectObject(dc,bitmap) : nullptr;
    int nearest=bitmap ? render(dc,source,pixels,static_cast<unsigned*>(bits),COLORONCOLOR) : -1;
    int smooth=bitmap ? render(dc,source,pixels,static_cast<unsigned*>(bits),HALFTONE) : -1;
    wprintf(L"Pixels intermediários | COLORONCOLOR %d | HALFTONE %d\n",nearest,smooth);
    if (bitmap) wprintf(L"Tempo por escala no PC | COLORONCOLOR %.3f ms | HALFTONE %.3f ms\n",
        benchmark(dc,source,pixels,COLORONCOLOR),benchmark(dc,source,pixels,HALFTONE));
    if (old) SelectObject(dc,old);
    if (bitmap) DeleteObject(bitmap);
    if (dc) DeleteDC(dc);
    return nearest==0 && smooth>0 ? 0 : 1;
}
