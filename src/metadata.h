#pragma once
#include "common.h"
inline wstring utf8(const std::string& value) {
    int size=static_cast<int>(value.size());
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),size,nullptr,0);
    wstring output(count,L'\0');
    if (count) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),size,&output[0],count);
    return output;
}
inline wstring titleFromJson(const wstring& value) {
    if (value.size()<2 || value.front()!=L'"' || value.back()!=L'"') return {};
    wstring title;
    for (size_t i=1;i+1<value.size();++i) {
        wchar_t c=value[i];
        if (c==L'\\' && i+2<value.size()) {
            c=value[++i];
            if (c==L'u' && i+5<value.size()) {
                wchar_t hex[5]={value[i+1],value[i+2],value[i+3],value[i+4],0}, *end=nullptr;
                c=static_cast<wchar_t>(wcstoul(hex,&end,16));
                if (*end) return {};
                i+=4;
            } else if (c==L'n' || c==L'r' || c==L't' || c==L'b' || c==L'f') c=L' ';
        }
        title+=c<32 ? L' ' : c;
    }
    return title;
}
