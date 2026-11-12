package org.hcvault.core.crypto.fat

data class FatRootEntry(
    val name: String,
    val isDirectory: Boolean,
    val sizeBytes: Long,
)
