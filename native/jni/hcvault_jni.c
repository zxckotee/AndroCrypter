#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hcvault/volume.h"
#include "hcvault/vfs.h"

static vc_volume* ptr(jlong handle) {
    return (vc_volume*)(intptr_t)handle;
}

JNIEXPORT jlong JNICALL
Java_org_hcvault_core_crypto_internal_NativeVolume_nativeOpen(
    JNIEnv* env,
    jclass /*clazz*/,
    jint fd,
    jbyteArray passwordUtf8,
    jint pim) {
    if (passwordUtf8 == NULL) {
        return -(jlong)VC_ERR_AUTH;
    }
    jsize pw_len = (*env)->GetArrayLength(env, passwordUtf8);
    jbyte* pw_bytes = (*env)->GetByteArrayElements(env, passwordUtf8, NULL);
    if (pw_bytes == NULL) {
        return -(jlong)VC_ERR_INTERNAL;
    }

    vc_open_params params = {
        .password = (const char*)pw_bytes,
        .password_len = (size_t)pw_len,
        .pim = pim,
        .keyfiles = NULL,
        .keyfile_lens = NULL,
        .keyfile_count = 0,
        .hidden = 0,
        .abort_flag = NULL,
    };

    vc_volume* vol = NULL;
    vc_status st = vc_open(fd, &params, &vol);
    (*env)->ReleaseByteArrayElements(env, passwordUtf8, pw_bytes, JNI_ABORT);
    if (st != VC_OK) {
        return -(jlong)st;
    }
    return (jlong)(intptr_t)vol;
}

JNIEXPORT void JNICALL
Java_org_hcvault_core_crypto_internal_NativeVolume_nativeClose(JNIEnv* env, jclass /*clazz*/, jlong handle) {
    (void)env;
    if (handle <= 0) {
        return;
    }
    vc_close(ptr(handle));
}

JNIEXPORT jlong JNICALL
Java_org_hcvault_core_crypto_internal_NativeVolume_nativeDataSize(JNIEnv* env, jclass /*clazz*/, jlong handle) {
    (void)env;
    if (handle <= 0) {
        return 0;
    }
    return (jlong)vc_data_size(ptr(handle));
}

JNIEXPORT jint JNICALL
Java_org_hcvault_core_crypto_internal_NativeVolume_nativeRead(
    JNIEnv* env,
    jclass /*clazz*/,
    jlong handle,
    jlong offset,
    jbyteArray out,
    jint len) {
    if (handle <= 0 || out == NULL || len <= 0) {
        return (jint)VC_ERR_IO;
    }
    jbyte* buf = (*env)->GetByteArrayElements(env, out, NULL);
    if (buf == NULL) {
        return (jint)VC_ERR_INTERNAL;
    }
    vc_status st = vc_read(ptr(handle), (uint64_t)offset, buf, (size_t)len);
    (*env)->ReleaseByteArrayElements(env, out, buf, 0);
    return (jint)st;
}

JNIEXPORT jobjectArray JNICALL
Java_org_hcvault_core_crypto_internal_NativeVolume_nativeListRoot(
    JNIEnv* env,
    jclass /*clazz*/,
    jlong handle) {
    if (handle <= 0) {
        return NULL;
    }
    vfs_ctx* vfs = NULL;
    if (vfs_open(ptr(handle), &vfs) != VFS_OK) {
        return NULL;
    }
    vfs_dirent entries[256];
    size_t count = 0;
    vfs_status st = vfs_list_dir(vfs, "/", entries, 256, &count);
    vfs_close(vfs);
    if (st != VFS_OK) {
        return NULL;
    }
    jclass str_cls = (*env)->FindClass(env, "java/lang/String");
    if (str_cls == NULL) {
        return NULL;
    }
    jobjectArray arr = (*env)->NewObjectArray(env, (jsize)count, str_cls, NULL);
    if (arr == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        jstring js = (*env)->NewStringUTF(env, entries[i].name);
        if (js == NULL) {
            return NULL;
        }
        (*env)->SetObjectArrayElement(env, arr, (jsize)i, js);
        (*env)->DeleteLocalRef(env, js);
    }
    return arr;
}
