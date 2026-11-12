package org.hcvault.core.storage

import android.net.Uri
import android.os.ParcelFileDescriptor
import javax.inject.Inject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.hcvault.core.common.SecretChars
import org.hcvault.core.common.VaultError
import org.hcvault.core.common.VaultResult
import org.hcvault.core.crypto.VolumeCredentials
import org.hcvault.core.crypto.VolumeEngine
import org.hcvault.core.crypto.VolumeSession
import org.hcvault.core.crypto.fat.Fat12RootBrowser
import org.hcvault.core.crypto.fat.FatRootEntry

class VolumeRepository @Inject constructor(
    private val engine: VolumeEngine,
    private val safVolumeIo: SafVolumeIo,
) {

    suspend fun openVolume(
        uri: Uri,
        password: SecretChars,
        pim: Int = 0,
    ): VaultResult<VolumeSession> = withContext(Dispatchers.IO) {
        when (val pfdResult = safVolumeIo.openReadOnly(uri)) {
            is VaultResult.Err -> return@withContext pfdResult
            is VaultResult.Ok -> openWithDescriptor(pfdResult.value, password, pim)
        }
    }

    suspend fun openVolumePath(
        path: String,
        password: SecretChars,
        pim: Int = 0,
    ): VaultResult<VolumeSession> = withContext(Dispatchers.IO) {
        when (val pfdResult = safVolumeIo.openReadOnlyPath(path)) {
            is VaultResult.Err -> return@withContext pfdResult
            is VaultResult.Ok -> openWithDescriptor(pfdResult.value, password, pim)
        }
    }

    suspend fun listFatRoot(session: VolumeSession): VaultResult<List<FatRootEntry>> =
        Fat12RootBrowser(session).listRoot()

    private suspend fun openWithDescriptor(
        descriptor: ParcelFileDescriptor,
        password: SecretChars,
        pim: Int,
    ): VaultResult<VolumeSession> {
        return try {
            val dup = descriptor.dup()
            descriptor.close()
            val credentials = VolumeCredentials(password, pim)
            when (val opened = engine.open(dup.fd, credentials)) {
                is VaultResult.Err -> {
                    dup.close()
                    opened
                }
                is VaultResult.Ok -> VaultResult.Ok(ManagedVolumeSession(opened.value, dup))
            }
        } catch (_: IllegalStateException) {
            VaultResult.Err(VaultError.Io("descriptor"))
        }
    }
}

private class ManagedVolumeSession(
    private val inner: VolumeSession,
    private val descriptor: ParcelFileDescriptor,
) : VolumeSession {
    override val dataSizeBytes: Long
        get() = inner.dataSizeBytes

    override suspend fun read(offset: Long, length: Int): VaultResult<ByteArray> = inner.read(offset, length)

    override fun close() {
        inner.close()
        descriptor.close()
    }
}
