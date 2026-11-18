package org.hcvault.app.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import org.hcvault.core.mount.MountController
import org.hcvault.core.mount.mountActionEnabled
import org.hcvault.core.mount.mountActionLabel
import org.hcvault.core.mount.mountStatusLine
import org.hcvault.core.mount.summaryForHome

data class HomeUiState(
    val rootTitle: String? = null,
    val rootDetail: String? = null,
    val mountActionLabel: String = "Checking root…",
    val mountActionEnabled: Boolean = false,
    val mountStatusLine: String? = null,
)

@HiltViewModel
class HomeViewModel @Inject constructor(
    private val mountController: MountController,
) : ViewModel() {

    val state: StateFlow<HomeUiState> = mountController.state
        .map { mount ->
            val (title, detail) = mount.rootStatus.summaryForHome()
            HomeUiState(
                rootTitle = title.takeIf { it.isNotEmpty() },
                rootDetail = detail.takeIf { it.isNotEmpty() },
                mountActionLabel = mount.mountActionLabel(),
                mountActionEnabled = mount.mountActionEnabled(),
                mountStatusLine = mount.mountStatusLine(),
            )
        }
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5_000), HomeUiState())

    fun onMountAction() {
        mountController.onMountActionRequested()
    }
}