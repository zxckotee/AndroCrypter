/* LibFuzzer target for ReadVolumeHeader — malformed headers must not crash. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Crypto.h"
#include "Password.h"
#include "Volumes.h"
#include "hcvault/volume.h"

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 512) {
        return 0;
    }
    unsigned char header[TC_VOLUME_HEADER_EFFECTIVE_SIZE];
    memset(header, 0, sizeof(header));
    memcpy(header, data, size < sizeof(header) ? size : sizeof(header));

    Password pw;
    memset(&pw, 0, sizeof(pw));
    const char* pass = "fuzz";
    memcpy(pw.Text, pass, strlen(pass));
    pw.Length = (unsigned __int32)strlen(pass);

    PCRYPTO_INFO crypto = NULL;
    int st = ReadVolumeHeader(FALSE, header, &pw, 0, 0, &crypto, NULL);
    (void)st;
    if (crypto != NULL) {
        crypto_close(crypto);
    }
    burn(header, sizeof(header));
    burn(&pw, sizeof(pw));
    return 0;
}