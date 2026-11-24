#include "theme.h"
#include <algorithm>

static HBITMAP artwork[10]={};
void Theme::bitmap(HDC dc,HBITMAP image,RECT rect,bool round) {
    if (!image || rect.right<=rect.left || rect.bottom<=rect.top) return;
    BITMAP bm; if (!GetObjectW(image,sizeof(bm),&bm)) return;
    HDC source=CreateCompatibleDC(dc); if (!source) return;
    auto old=SelectObject(source,image);
    int saved=SaveDC(dc);
    if (round) {
        HRGN clip=CreateEllipticRgnIndirect(&rect);
        ExtSelectClipRgn(dc,clip,RGN_AND); DeleteObject(clip);
    }
    SetStretchBltMode(dc,HALFTONE); SetBrushOrgEx(dc,0,0,nullptr);
    StretchBlt(dc,rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top,
        source,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);
    RestoreDC(dc,saved); SelectObject(source,old); DeleteDC(source);
}
void Theme::art(HDC dc,Art id,int x,int y,int width,int height) {
    auto& image=artwork[static_cast<int>(id)-App];
    if (!image) image=LoadBitmapW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(id));
    bitmap(dc,image,RECT{x,y,x+width,y+height});
}
void Theme::releaseArt() { for (auto& image:artwork) if (image) { DeleteObject(image); image=nullptr; } }

void Theme::fill(HDC dc,const RECT& rect,COLORREF color) {
    SetDCBrushColor(dc,color);
    FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
void Theme::rounded(HDC dc,RECT rect,COLORREF surface,COLORREF border,int radius) {
    auto brush=SelectObject(dc,GetStockObject(DC_BRUSH));
    auto pen=SelectObject(dc,GetStockObject(DC_PEN));
    SetDCBrushColor(dc,surface); SetDCPenColor(dc,border);
    RoundRect(dc,rect.left,rect.top,rect.right,rect.bottom,radius,radius);
    SelectObject(dc,pen); SelectObject(dc,brush);
}
void Theme::label(HDC dc,const wstring& value,RECT rect,COLORREF color,UINT flags) {
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
    DrawTextW(dc,value.c_str(),-1,&rect,flags|DT_NOPREFIX);
}
void Theme::mark(HDC dc,int x,int y,int size) {
    fill(dc,RECT{x,y,x+size,y+size},accent);
    POINT arrow[]={{x+size*3/8,y+size/4},{x+size*3/8,y+size*3/4},{x+size*3/4,y+size/2}};
    SetDCBrushColor(dc,ink);
    auto brush=SelectObject(dc,GetStockObject(DC_BRUSH));
    auto pen=SelectObject(dc,GetStockObject(NULL_PEN));
    Polygon(dc,arrow,3);
    SelectObject(dc,pen); SelectObject(dc,brush);
}
void Theme::button(const DRAWITEMSTRUCT& item,bool primary,bool magnifier) {
    HDC dc=item.hDC;
    RECT r=item.rcItem;
    bool disabled=(item.itemState&ODS_DISABLED)!=0;
    COLORREF surface=primary && !disabled ? accent : raised;
    if (item.itemState&ODS_SELECTED) surface=primary ? RGB(116,156,225) : line;
    fill(dc,r,background);
    rounded(dc,r,surface,(item.itemState&ODS_FOCUS) ? text : primary ? accent : line,9);
    wchar_t caption[96]; GetWindowTextW(item.hwndItem,caption,_countof(caption));
    auto font=SelectObject(dc,reinterpret_cast<HFONT>(SendMessageW(item.hwndItem,WM_GETFONT,0,0)));
    COLORREF foreground=disabled ? muted : primary ? ink : text;
    if (magnifier) {
        SIZE size={}; GetTextExtentPoint32W(dc,caption,lstrlenW(caption),&size);
        int x=(r.right-size.cx-19)/2, y=r.bottom/2-6;
        auto pen=CreatePen(PS_SOLID,2,foreground); auto oldPen=SelectObject(dc,pen);
        auto oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
        Ellipse(dc,x,y,x+10,y+10); MoveToEx(dc,x+8,y+8,nullptr); LineTo(dc,x+14,y+14);
        SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(pen);
        r.left+=19;
    }
    label(dc,caption,r,foreground,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(dc,font);
}

struct Slider { int value=0; bool dragging=false; };
LRESULT CALLBACK sliderProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto value=reinterpret_cast<Slider*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(new Slider)); return 0;
    case WM_NCDESTROY: delete value; break;
    case SliderGet: return value->value;
    case SliderTracking: return value->dragging;
    case SliderSet:
        if (value->value!=static_cast<int>(wp)) {
            value->value=static_cast<int>(wp); InvalidateRect(hwnd,nullptr,FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd); SetCapture(hwnd); value->dragging=true;
        // Fall through to preview the clicked position.
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
        if (value->dragging) {
            RECT r; GetClientRect(hwnd,&r);
            int trackWidth=std::max(1,static_cast<int>(r.right)-14);
            value->value=std::max(0,std::min(1000,(static_cast<short>(LOWORD(lp))-7)*1000/trackWidth));
            bool commit=msg==WM_LBUTTONUP;
            if (commit) { value->dragging=false; ReleaseCapture(); }
            InvalidateRect(hwnd,nullptr,FALSE);
            SendMessageW(GetParent(hwnd),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(hwnd),commit ? SliderCommit : SliderPreview),
                reinterpret_cast<LPARAM>(hwnd));
        }
        return 0;
    case WM_CAPTURECHANGED: value->dragging=false; return 0;
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_ENABLE: InvalidateRect(hwnd,nullptr,FALSE); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT paint; auto dc=BeginPaint(hwnd,&paint);
        RECT r; GetClientRect(hwnd,&r);
        Theme::fill(dc,r,Theme::panel);
        int center=r.bottom/2, x=7+(r.right-14)*value->value/1000;
        Theme::fill(dc,RECT{7,center-2,r.right-7,center+2},Theme::line);
        if (IsWindowEnabled(hwnd)) {
            Theme::fill(dc,RECT{7,center-2,x,center+2},Theme::accent);
            Theme::fill(dc,RECT{x-4,center-5,x+5,center+5},GetFocus()==hwnd ? Theme::text : Theme::accent);
        }
        EndPaint(hwnd,&paint); return 0;
    }
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
