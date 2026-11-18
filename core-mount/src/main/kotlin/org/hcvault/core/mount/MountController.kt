package org.hcvault.core.mount

import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import org.hcvault.core.common.HcLog

typealias RootProbe = suspend () -> RootStatus

@Singleton
class MountController @Inject constructor(
    private val rootShell: RootShell,
    private val rootProbe: RootProbe = { RootEnvironmentProbe.probe() },
) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    private val _state = MutableStateFlow(MountControllerState())
    val state: StateFlow<MountControllerState> = _state.asStateFlow()

    init {
        scope.launch { refreshRootStatus() }
    }

    suspend fun refreshRootStatus() {
        val status = rootProbe()
        _state.update { current ->
            current.copy(
                rootStatus = status,
                mountPhase = when {
                    status !is RootStatus.Magisk -> MountPhase.Idle
                    current.mountPhase == MountPhase.Mounted -> current.mountPhase
                    current.mountPhase == MountPhase.Mounting -> MountPhase.Idle
                    else -> current.mountPhase
                },
                activeMountPoint = if (status is RootStatus.Magisk) current.activeMountPoint else null,
                errorMessage = if (status is RootStatus.Magisk) current.errorMessage else null,
            )
        }
    }

    fun onMountActionRequested() {
        val snapshot = _state.value
        when {
            snapshot.mountPhase == MountPhase.Mounting -> return
            snapshot.rootStatus !is RootStatus.Magisk -> {
                _state.update {
                    it.copy(
                        mountPhase = MountPhase.Error,
                        errorMessage = mountBlockedMessage(snapshot.rootStatus),
                    )
                }
                return
            }
            !MountedModeSupport.IMPLEMENTED -> {
                HcLog.d(TAG, "Mount action blocked: mounted mode not implemented yet")
                _state.update {
                    it.copy(
                        mountPhase = MountPhase.Error,
                        errorMessage = NOT_IMPLEMENTED_MESSAGE,
                    )
                }
                return
            }
        }
        _state.update { it.copy(mountPhase = MountPhase.Mounting, errorMessage = null) }
        scope.launch {
            if (!rootShell.isAppGrantedRoot()) {
                _state.update {
                    it.copy(
                        rootStatus = RootStatus.SuDenied,
                        mountPhase = MountPhase.Error,
                        errorMessage = mountBlockedMessage(RootStatus.SuDenied),
                    )
                }
                return@launch
            }
            // Placeholder until RootService + hcvault-mountd are wired (E6 follow-up).
            _state.update {
                it.copy(
                    mountPhase = MountPhase.Error,
                    errorMessage = NOT_IMPLEMENTED_MESSAGE,
                )
            }
        }
    }

    fun onUnmountActionRequested() {
        _state.update {
            it.copy(
                mountPhase = MountPhase.Idle,
                activeMountPoint = null,
                errorMessage = null,
            )
        }
    }

    fun clearMountError() {
        _state.update {
            if (it.mountPhase == MountPhase.Error) {
                it.copy(mountPhase = MountPhase.Idle, errorMessage = null)
            } else {
                it
            }
        }
    }

    private fun mountBlockedMessage(status: RootStatus): String = when (status) {
        RootStatus.NotChecked -> "Still checking root access."
        RootStatus.NoSu -> "Magisk (or compatible su) is required for kernel mount."
        RootStatus.SuDenied -> "Grant superuser access to HCVault, then try again."
        RootStatus.SuOtherImpl -> "Mounted mode is tested with Magisk."
        is RootStatus.Magisk -> NOT_IMPLEMENTED_MESSAGE
    }

    companion object {
        private const val TAG = "MountController"
        const val NOT_IMPLEMENTED_MESSAGE =
            "Kernel mount is still in development; use «Open volume» for userspace browsing."
    }
}