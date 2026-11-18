package org.hcvault.core.mount

/**
 * Combined root-probe and mount lifecycle state for the UI and foreground service.
 *
 * Root is modeled with [RootStatus]; mount work uses [MountPhase] once root is usable.
 */
data class MountControllerState(
    val rootStatus: RootStatus = RootStatus.NotChecked,
    val mountPhase: MountPhase = MountPhase.Idle,
    val activeMountPoint: String? = null,
    val errorMessage: String? = null,
) {
    val isRootProbeComplete: Boolean
        get() = rootStatus !is RootStatus.NotChecked

    val canOfferMountAction: Boolean
        get() = rootStatus is RootStatus.Magisk &&
            MountedModeSupport.IMPLEMENTED &&
            mountPhase != MountPhase.Mounting

    val isMountActionInProgress: Boolean
        get() = mountPhase == MountPhase.Mounting
}