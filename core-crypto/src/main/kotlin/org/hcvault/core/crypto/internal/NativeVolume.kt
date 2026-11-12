package org.hcvault.core.crypto.internal

internal object NativeVolume {
    init {
        System.loadLibrary("hcvault_jni")
    }

    external fun nativeOpen(fd: Int, passwordUtf8: ByteArray, pim: Int): Long

    external fun nativeClose(handle: Long)

    external fun nativeDataSize(handle: Long): Long

    /** On success returns [vc_status] VC_OK (0); otherwise a positive error code. */
    external fun nativeRead(handle: Long, offset: Long, out: ByteArray, len: Int): Int
}
