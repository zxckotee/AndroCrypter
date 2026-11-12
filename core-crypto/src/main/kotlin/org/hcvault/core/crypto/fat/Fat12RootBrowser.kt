@file:Suppress("MagicNumber")

package org.hcvault.core.crypto.fat

import org.hcvault.core.common.VaultError
import org.hcvault.core.common.VaultResult
import org.hcvault.core.crypto.VolumeSession

/**
 * Lists the root directory of a FAT12 image using [VolumeSession.read] only (no native VFS).
 */
class Fat12RootBrowser(
    private val session: VolumeSession,
) {

    @Suppress("CyclomaticComplexMethod", "ReturnCount", "LoopWithTooManyJumpStatements", "MagicNumber")
    suspend fun listRoot(): VaultResult<List<FatRootEntry>> {
        val boot = when (val sector = session.read(0, BOOT_SECTOR_SIZE)) {
            is VaultResult.Ok -> sector.value
            is VaultResult.Err -> return sector
        }
        if (boot.size < BOOT_SECTOR_SIZE || boot[OFF_BOOT_SIG_0].toInt() and 0xFF != BOOT_SIG_0 ||
            boot[OFF_BOOT_SIG_1].toInt() and 0xFF != BOOT_SIG_1
        ) {
            return VaultResult.Err(VaultError.UnsupportedFormat)
        }

        val bytesPerSector = u16le(boot, OFF_BYTES_PER_SECTOR)
        val reserved = u16le(boot, OFF_RESERVED_SECTORS)
        val fatCount = boot[OFF_NUM_FATS].toInt() and 0xFF
        val rootEntryCount = u16le(boot, OFF_ROOT_ENTRY_COUNT)
        val sectorsPerFat = u16le(boot, OFF_SECTORS_PER_FAT)
        if (bytesPerSector < MIN_SECTOR_SIZE || fatCount == 0 || rootEntryCount == 0) {
            return VaultResult.Err(VaultError.UnsupportedFormat)
        }

        val rootOffset = reserved + fatCount * sectorsPerFat
        val rootByteOffset = rootOffset.toLong() * bytesPerSector
        val rootSize = rootEntryCount * DIR_ENTRY_SIZE

        val root = when (val dir = session.read(rootByteOffset, rootSize)) {
            is VaultResult.Ok -> dir.value
            is VaultResult.Err -> return dir
        }

        val entries = mutableListOf<FatRootEntry>()
        var i = 0
        while (i + DIR_ENTRY_SIZE <= root.size) {
            val slot = root.copyOfRange(i, i + DIR_ENTRY_SIZE)
            i += DIR_ENTRY_SIZE
            if (slot[0].toInt() and 0xFF == SLOT_END) break
            if (slot[0].toInt() and 0xFF == SLOT_DELETED) continue
            val attr = slot[OFF_DIR_ATTR].toInt() and 0xFF
            if (attr == ATTR_LONG_NAME) continue
            if (attr and ATTR_VOLUME_ID != 0) continue

            val name = formatDosName(slot)
            if (name.isEmpty()) continue
            val isDir = attr and ATTR_DIRECTORY != 0
            val size = u32le(slot, OFF_DIR_FILE_SIZE)
            entries += FatRootEntry(name, isDir, size.toLong())
        }
        return VaultResult.Ok(entries)
    }

    private fun formatDosName(entry: ByteArray): String {
        val base = entry.copyOfRange(0, DOS_NAME_LEN).decodeFatChars().trim()
        val ext = entry.copyOfRange(DOS_NAME_LEN, DOS_NAME_LEN + DOS_EXT_LEN).decodeFatChars().trim()
        return if (ext.isEmpty()) base else "$base.$ext"
    }

    private fun ByteArray.decodeFatChars(): String {
        val chars = CharArray(size) { idx -> this[idx].toInt().toChar() }
        return String(chars).trim { it <= ' ' || it == '\u0000' }
    }

    private fun u16le(buf: ByteArray, off: Int): Int =
        (buf[off].toInt() and 0xFF) or ((buf[off + 1].toInt() and 0xFF) shl 8)

    private fun u32le(buf: ByteArray, off: Int): Long {
        return (buf[off].toLong() and 0xFF) or
            ((buf[off + 1].toLong() and 0xFF) shl 8) or
            ((buf[off + 2].toLong() and 0xFF) shl 16) or
            ((buf[off + 3].toLong() and 0xFF) shl 24)
    }

    private companion object {
        const val BOOT_SECTOR_SIZE = 512
        const val DIR_ENTRY_SIZE = 32
        const val MIN_SECTOR_SIZE = 512
        const val ATTR_VOLUME_ID = 0x08
        const val ATTR_DIRECTORY = 0x10
        const val ATTR_LONG_NAME = 0x0F
        const val SLOT_END = 0x00
        const val SLOT_DELETED = 0xE5
        const val BOOT_SIG_0 = 0x55
        const val BOOT_SIG_1 = 0xAA
        const val OFF_BYTES_PER_SECTOR = 11
        const val OFF_RESERVED_SECTORS = 14
        const val OFF_NUM_FATS = 16
        const val OFF_ROOT_ENTRY_COUNT = 17
        const val OFF_SECTORS_PER_FAT = 22
        const val OFF_BOOT_SIG_0 = 510
        const val OFF_BOOT_SIG_1 = 511
        const val OFF_DIR_ATTR = 11
        const val OFF_DIR_FILE_SIZE = 28
        const val DOS_NAME_LEN = 8
        const val DOS_EXT_LEN = 3
    }
}
