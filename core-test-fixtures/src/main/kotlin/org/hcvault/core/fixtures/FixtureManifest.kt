package org.hcvault.core.fixtures

import kotlinx.serialization.Serializable
import kotlinx.serialization.json.Json

/** Describes one generated `.hc` fixture. The password is never part of the manifest (env `TEST_VOLUME_PASSWORD`). */
@Serializable
data class FixtureSpec(
    val name: String,
    val file: String,
    val sizeBytes: Long,
    val encryption: String,
    val hash: String,
    val filesystem: String,
    val pim: Int,
)

@Serializable
data class FixtureManifest(
    val version: Int,
    val fixtures: List<FixtureSpec>,
)

object FixtureManifests {
    private val json = Json { ignoreUnknownKeys = true }

    fun parse(raw: String): FixtureManifest = json.decodeFromString(raw)

    fun primaryFixture(manifest: FixtureManifest): FixtureSpec =
        manifest.fixtures.firstOrNull() ?: error("manifest has no fixtures")
}
