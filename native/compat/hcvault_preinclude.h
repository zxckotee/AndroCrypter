#pragma once

#define VC_DCS_DISABLE_ARGON2 1
#define HCVAULT_BUILD 1

#include <stdlib.h>
#include <sys/mman.h>
#include <wchar.h>

typedef long LONG;
typedef unsigned long ULONG;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef void* HANDLE;
typedef void* HWND;

#include <strings.h>
#define _wcsicmp wcscasecmp
#ifndef TC_EVENT
typedef void* TC_EVENT;
#endif

static inline HANDLE CreateEvent(void* a, int b, int c, const void* d) {
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    static int dummy_event;
    return (HANDLE)&dummy_event;
}

#ifndef VirtualLock
#define VirtualLock(ptr, size) (void)mlock((void*)(ptr), (size_t)(size))
#endif
#ifndef VirtualUnlock
#define VirtualUnlock(ptr, size) (void)munlock((void*)(ptr), (size_t)(size))
#endif
#define CloseHandle(h) ((void)(h))
#ifndef TC_WAIT_EVENT
#define TC_WAIT_EVENT(event) ((void)(event))
#endif

#include <stdatomic.h>
typedef atomic_long hcvault_atomic_long;

static inline long InterlockedExchangeAdd(long* target, long value) {
    return atomic_fetch_add((atomic_long*)target, value) + value;
}

static inline long InterlockedExchange(long volatile* target, long value) {
    return atomic_exchange((atomic_long*)target, value);
}

static inline long InterlockedDecrement(long* target) {
    return atomic_fetch_sub((atomic_long*)target, 1) - 1;
}

#ifndef HasRDRAND
#define HasRDRAND() 0
#endif
#ifndef HasRDSEED
#define HasRDSEED() 0
#endif
