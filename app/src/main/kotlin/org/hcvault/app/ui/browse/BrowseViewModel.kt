package org.hcvault.app.ui.browse

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import org.hcvault.app.session.SessionManager
import org.hcvault.core.common.VaultResult
import org.hcvault.core.crypto.fat.FatRootEntry
import org.hcvault.core.storage.VolumeRepository

data class BrowseUiState(
    val loading: Boolean = true,
    val entries: List<FatRootEntry> = emptyList(),
    val errorMessage: String? = null,
    val dataSizeBytes: Long = 0,
)

@HiltViewModel
class BrowseViewModel @Inject constructor(
    private val sessions: SessionManager,
    private val repository: VolumeRepository,
) : ViewModel() {

    private val _state = MutableStateFlow(BrowseUiState())
    val state: StateFlow<BrowseUiState> = _state.asStateFlow()

    init {
        viewModelScope.launch { loadRoot() }
    }

    private suspend fun loadRoot() {
        val session = sessions.active
        if (session == null) {
            _state.update { it.copy(loading = false, errorMessage = "ERR_NO_SESSION") }
            return
        }
        _state.update { it.copy(loading = true, dataSizeBytes = session.dataSizeBytes) }
        when (val listed = repository.listFatRoot(session)) {
            is VaultResult.Ok -> _state.update {
                it.copy(loading = false, entries = listed.value, errorMessage = null)
            }
            is VaultResult.Err -> _state.update {
                it.copy(loading = false, errorMessage = listed.error.code)
            }
        }
    }

    fun closeSession() {
        sessions.clear()
    }
}
