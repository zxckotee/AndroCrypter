#include "EncryptionThreadPool.h"
#include "Crypto.h"

size_t GetEncryptionThreadCount(void) {
    return 0;
}

size_t GetMaxEncryptionThreadCount(void) {
    return 0;
}

BOOL IsEncryptionThreadPoolRunning(void) {
    return FALSE;
}

BOOL AcquireThreadPoolReference(void) {
    return FALSE;
}

void ReleaseThreadPoolReference(void) {}

void EncryptionThreadPoolBeginKeyDerivation(
    TC_EVENT* completionEvent,
    TC_EVENT* noOutstandingWorkItemEvent,
    LONG* completionFlag,
    LONG* outstandingWorkItemCount,
    int pkcs5Prf,
    unsigned char* password,
    int passwordLength,
    unsigned char* salt,
    int iterationCount,
    int memoryCost,
    unsigned char* derivedKey,
    LONG* derivationResult,
    LONG volatile* pAbortKeyDerivation) {
    (void)completionEvent;
    (void)noOutstandingWorkItemEvent;
    (void)completionFlag;
    (void)outstandingWorkItemCount;
    (void)pkcs5Prf;
    (void)password;
    (void)passwordLength;
    (void)salt;
    (void)iterationCount;
    (void)memoryCost;
    (void)derivedKey;
    (void)derivationResult;
    (void)pAbortKeyDerivation;
}

void EncryptionThreadPoolDoWork(
    EncryptionThreadPoolWorkType type,
    uint8* data,
    const UINT64_STRUCT* startUnitNo,
    uint32 unitCount,
    PCRYPTO_INFO cryptoInfo) {
    if (unitCount == 0) {
        return;
    }
    switch (type) {
    case DecryptDataUnitsWork:
        DecryptDataUnitsCurrentThreadEx(data, startUnitNo, unitCount, cryptoInfo);
        break;
    case EncryptDataUnitsWork:
        EncryptDataUnitsCurrentThreadEx(data, startUnitNo, unitCount, cryptoInfo);
        break;
    default:
        break;
    }
}

void EncryptDataUnitsCurrentThreadEx(
    unsigned __int8* buf,
    const UINT64_STRUCT* structUnitNo,
    TC_LARGEST_COMPILER_UINT nbrUnits,
    PCRYPTO_INFO ci) {
    EncryptDataUnitsCurrentThread(buf, structUnitNo, nbrUnits, ci);
}

void DecryptDataUnitsCurrentThreadEx(
    unsigned __int8* buf,
    const UINT64_STRUCT* structUnitNo,
    TC_LARGEST_COMPILER_UINT nbrUnits,
    PCRYPTO_INFO ci) {
    DecryptDataUnitsCurrentThread(buf, structUnitNo, nbrUnits, ci);
}
