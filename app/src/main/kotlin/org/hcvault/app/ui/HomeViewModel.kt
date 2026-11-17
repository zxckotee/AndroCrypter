package org.hcvault.app.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import org.hcvault.core.mount.RootEnvironmentProbe
import org.hcvault.core.mount.RootStatus
import org.hcvault.core.mount.summaryForHome

data class HomeUiState(
    val rootStatus: RootStatus = RootStatus.NotChecked,
    val rootTitle: String? = null,
    val rootDetail: String? = null,
)

@HiltViewModel
class HomeViewModel @Inject constructor() : ViewModel() {

    private val _state = MutableStateFlow(HomeUiState())
    val state: StateFlow<HomeUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            val status = RootEnvironmentProbe.probe()
            val (title, detail) = status.summaryForHome()
            _state.value = HomeUiState(
                rootStatus = status,
                rootTitle = title.takeIf { it.isNotEmpty() },
                rootDetail = detail.takeIf { it.isNotEmpty() },
            )
        }
    }
}