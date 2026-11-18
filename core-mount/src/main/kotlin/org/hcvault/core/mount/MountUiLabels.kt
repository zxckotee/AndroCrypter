package org.hcvault.core.mount

/** Copy for the home-screen mount action button (non-Compose so :core-mount stays UI-free). */
fun MountControllerState.mountActionLabel(): String = when (mountPhase) {
    MountPhase.Mounting -> "Mounting…"
    MountPhase.Mounted -> "Unmount"
    MountPhase.Error -> "Mount unavailable"
    MountPhase.Idle -> when (rootStatus) {
        RootStatus.NotChecked -> "Checking root…"
        is RootStatus.Magisk ->
            if (MountedModeSupport.IMPLEMENTED) "Mount volume" else "Mount (in development)"
        else -> "Mount unavailable"
    }
}

fun MountControllerState.mountActionEnabled(): Boolean =
    when (mountPhase) {
        MountPhase.Mounting -> false
        MountPhase.Mounted -> true
        MountPhase.Error -> rootStatus is RootStatus.Magisk
        MountPhase.Idle ->
            rootStatus is RootStatus.Magisk || rootStatus == RootStatus.NotChecked
    }

fun MountControllerState.mountStatusLine(): String? = errorMessage
    ?: when (mountPhase) {
        MountPhase.Mounted -> activeMountPoint?.let { "Mounted at $it" }
        else -> null
    }