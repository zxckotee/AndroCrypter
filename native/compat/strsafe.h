#pragma once
#include <wchar.h>

#ifndef S_OK
#define S_OK 0
#endif

static inline int StringCchCopyW(wchar_t* dest, size_t destLen, const wchar_t* src) {
    if (destLen == 0) {
        return -1;
    }
    size_t i = 0;
    while (i + 1 < destLen && src[i] != L'\0') {
        dest[i] = src[i];
        ++i;
    }
    dest[i] = L'\0';
    return S_OK;
}

static inline int StringCchCatW(wchar_t* dest, size_t destLen, const wchar_t* src) {
    size_t used = wcslen(dest);
  if (used >= destLen) {
        return -1;
    }
    return StringCchCopyW(dest + used, destLen - used, src);
}
