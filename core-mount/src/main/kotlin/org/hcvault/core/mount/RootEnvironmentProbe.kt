package org.hcvault.core.mount

import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Detects whether libsu can run commands in the mount-master namespace (Magisk or other `su`).
 * Does not mount volumes; see [MountedModeSupport] for product readiness.
 */
object RootEnvironmentProbe {

    suspend fun probe(): RootStatus = withContext(Dispatchers.IO) {
        if (Shell.isAppGrantedRoot() != true) {
            return@withContext RootStatus.NoSu
        }
        val version = Shell.cmd("magisk -v").exec()
        if (version.isSuccess) {
            val label = version.out.joinToString(" ").trim()
            if (label.isNotEmpty()) {
                return@withContext RootStatus.Magisk(label)
            }
        }
        val which = Shell.cmd("command -v magisk").exec()
        if (which.isSuccess && which.out.any { it.contains("magisk") }) {
            return@withContext RootStatus.Magisk("present")
        }
        RootStatus.SuOtherImpl
    }
}

object MountedModeSupport {
    /** Kernel mount path (FUSE → loop → mount) is not implemented yet. */
    const val IMPLEMENTED: Boolean = false
}