package org.hcvault.core.crypto

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.hcvault.core.common.VaultError
import org.hcvault.core.common.VaultResult
import org.hcvault.core.crypto.internal.NativeVolume

internal class VolumeSessionImpl(
    private val handle: Long,
) : VolumeSession {

    private var closed = false

    override val dataSizeBytes: Long
        get() = NativeVolume.nativeDataSize(handle)

    override suspend fun read(offset: Long, length: Int): VaultResult<ByteArray> = withContext(Dispatchers.IO) {
        checkOpen()
        if (length <= 0) {
            return@withContext VaultResult.Ok(ByteArray(0))
        }
        val buf = ByteArray(length)
        val status = NativeVolume.nativeRead(handle, offset, buf, length)
        if (status != VC_OK) {
            VaultResult.Err(mapStatus(status))
        } else {
            VaultResult.Ok(buf)
        }
    }

    override fun close() {
        if (closed) return
        NativeVolume.nativeClose(handle)
        closed = true
    }

    private fun checkOpen() {
        check(!closed) { "session closed" }
    }

    private companion object {
        const val VC_OK = 0
    }
}

internal fun mapNativeOpenResult(handle: Long): VaultResult<VolumeSession> {
    if (handle > 0) {
        return VaultResult.Ok(VolumeSessionImpl(handle))
    }
    return VaultResult.Err(mapStatus(-handle.toInt()))
}

internal fun mapStatus(code: Int): VaultError = when (code) {
    VC_ERR_AUTH -> VaultError.Auth
    VC_ERR_CANCELLED -> VaultError.Cancelled
    VC_ERR_UNSUPPORTED -> VaultError.UnsupportedFormat
    VC_ERR_IO -> VaultError.Io("native I/O")
    else -> VaultError.Internal("native status $code")
}

private const val VC_ERR_AUTH = 1
private const val VC_ERR_CANCELLED = 2
private const val VC_ERR_UNSUPPORTED = 3
private const val VC_ERR_IO = 4
