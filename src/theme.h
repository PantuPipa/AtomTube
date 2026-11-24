#pragma once
#include "common.h"

namespace Theme {
constexpr COLORREF background=RGB(7,8,19), panel=RGB(17,19,36), raised=RGB(31,34,57);
constexpr COLORREF line=RGB(57,62,86), text=RGB(247,247,253), muted=RGB(176,180,202);
constexpr COLORREF accent=RGB(149,189,255), ink=RGB(10,22,47);
enum Art { App=201,Logo,Inside,Searching,Tracks,Formats,Downloading,Merging,Opening,Error };
void art(HDC dc,Art id,int x,int y,int width,int height);
void bitmap(HDC dc,HBITMAP image,RECT rect,bool round=false);
void releaseArt();
void fill(HDC dc,const RECT& rect,COLORREF color);
void rounded(HDC dc,RECT rect,COLORREF surface,COLORREF border,int radius);
void label(HDC dc,const wstring& text,RECT rect,COLORREF color,UINT flags);
void mark(HDC dc,int x,int y,int size);
void button(const DRAWITEMSTRUCT& item,bool primary,bool magnifier=false);
}

constexpr UINT SliderSet=WM_USER+1, SliderGet=WM_USER+2, SliderTracking=WM_USER+3;
constexpr int SliderPreview=1, SliderCommit=2;
LRESULT CALLBACK sliderProc(HWND,UINT,WPARAM,LPARAM);
