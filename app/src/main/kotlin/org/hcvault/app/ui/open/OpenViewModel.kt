package org.hcvault.app.ui.open

import android.net.Uri
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
import org.hcvault.core.common.SecretChars
import org.hcvault.core.common.VaultResult
import org.hcvault.core.storage.VolumeRepository

data class OpenUiState(
    val volumeUri: Uri? = null,
    val displayName: String? = null,
    val busy: Boolean = false,
    val errorMessage: String? = null,
    val opened: Boolean = false,
)

@HiltViewModel
class OpenViewModel @Inject constructor(
    private val repository: VolumeRepository,
    private val sessions: SessionManager,
) : ViewModel() {

    private val _state = MutableStateFlow(OpenUiState())
    val state: StateFlow<OpenUiState> = _state.asStateFlow()

    fun onVolumePicked(uri: Uri, displayName: String?) {
        _state.update { it.copy(volumeUri = uri, displayName = displayName, errorMessage = null) }
    }

    fun unlock(passwordChars: CharArray) {
        val uri = _state.value.volumeUri ?: return
        viewModelScope.launch {
            _state.update { it.copy(busy = true, errorMessage = null) }
            val secret = SecretChars.takeOwnership(passwordChars)
            try {
                when (val result = repository.openVolume(uri, secret)) {
                    is VaultResult.Ok -> {
                        sessions.attach(result.value)
                        _state.update { it.copy(busy = false, opened = true) }
                    }
                    is VaultResult.Err -> {
                        _state.update {
                            it.copy(busy = false, errorMessage = result.error.code, opened = false)
                        }
                    }
                }
            } finally {
                secret.wipe()
            }
        }
    }

    fun consumeOpened(): Boolean {
        val opened = _state.value.opened
        if (opened) {
            _state.update { it.copy(opened = false) }
        }
        return opened
    }
}
