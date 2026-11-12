package org.hcvault.core.fixtures

/** Describes one generated `.hc` fixture. The password is never part of the manifest (env `TEST_VOLUME_PASSWORD`). */
data class FixtureSpec(
    val name: String,
    val file: String,
    val sizeBytes: Long,
    val encryption: String,
    val hash: String,
    val filesystem: String,
    val pim: Int,
)
