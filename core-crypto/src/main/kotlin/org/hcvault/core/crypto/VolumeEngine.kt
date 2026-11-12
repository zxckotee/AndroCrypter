package org.hcvault.core.crypto

import org.hcvault.core.common.SecretChars
import org.hcvault.core.common.VaultResult

/** Credentials for opening a volume. Wiped by the engine after key derivation. */
class VolumeCredentials(
    val password: SecretChars,
    val pim: Int = 0,
    val keyfiles: List<ByteArray> = emptyList(),
)

/** Narrow contract between Kotlin and the native volume layer; implemented over JNI in a later PR. */
interface VolumeEngine {
    suspend fun open(fd: Int, credentials: VolumeCredentials): VaultResult<VolumeSession>
}

interface VolumeSession : AutoCloseable {
    val dataSizeBytes: Long
}
