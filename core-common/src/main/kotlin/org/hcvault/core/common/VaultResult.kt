package org.hcvault.core.common

/** Result type shared by the engine, storage and mount layers. */
sealed interface VaultResult<out T> {
    data class Ok<T>(val value: T) : VaultResult<T>
    data class Err(val error: VaultError) : VaultResult<Nothing>
}

/** Errors are deliberately coarse so that a failed unlock never reveals why the password was rejected. */
sealed class VaultError(val code: String) {
    data object Auth : VaultError("ERR_AUTH")
    data object Cancelled : VaultError("ERR_CANCELLED")
    data object UnsupportedFormat : VaultError("ERR_UNSUPPORTED")
    data object NotSeekable : VaultError("ERR_NOT_SEEKABLE")
    data class Io(val detail: String) : VaultError("ERR_IO")
    data class Internal(val detail: String) : VaultError("ERR_INTERNAL")
}
