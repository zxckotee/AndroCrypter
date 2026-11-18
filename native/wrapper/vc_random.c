#include "Random.h"

#include "hcvault/volume.h"

BOOL RandgetBytes(void* hwndDlg, unsigned char* buf, int len, BOOL forceSlowPoll) {
    (void)hwndDlg;
    (void)forceSlowPoll;
    if (buf == NULL || len <= 0) {
        return FALSE;
    }
    return vc_random_bytes(buf, (size_t)len) == VC_OK ? TRUE : FALSE;
}

BOOL RandgetBytesFull(void* hwndDlg, unsigned char* buf, int len, BOOL forceSlowPoll, BOOL allowAnyLength) {
    (void)allowAnyLength;
    return RandgetBytes(hwndDlg, buf, len, forceSlowPoll);
}