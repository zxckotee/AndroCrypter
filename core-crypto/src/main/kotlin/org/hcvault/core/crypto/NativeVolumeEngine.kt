package org.hcvault.core.crypto

import javax.inject.Inject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.hcvault.core.common.VaultResult
import org.hcvault.core.crypto.internal.NativeVolume

class NativeVolumeEngine @Inject constructor() : VolumeEngine {

    override suspend fun open(fd: Int, credentials: VolumeCredentials): VaultResult<VolumeSession> =
        withContext(Dispatchers.IO) {
            val passwordBytes = credentials.password.toUtf8Bytes()
            try {
                val handle = NativeVolume.nativeOpen(fd, passwordBytes, credentials.pim)
                mapNativeOpenResult(handle)
            } finally {
                passwordBytes.fill(0)
            }
        }
}
