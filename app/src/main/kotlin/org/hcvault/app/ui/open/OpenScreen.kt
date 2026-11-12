package org.hcvault.app.ui.open

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import org.hcvault.app.R
import org.hcvault.core.storage.SafVolumeIo

const val TAG_OPEN_UNLOCK = "open_unlock_button"

@Composable
fun OpenScreen(
    onOpened: () -> Unit,
    onBack: () -> Unit,
    viewModel: OpenViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsState()
    val context = LocalContext.current
    val saf = remember { SafVolumeIo(context) }
    var password by remember { mutableStateOf("") }

    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) {
            runCatching {
                context.contentResolver.takePersistableUriPermission(
                    uri,
                    android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION,
                )
            }
            viewModel.onVolumePicked(uri, saf.displayName(uri))
        }
    }

    LaunchedEffect(state.opened) {
        if (viewModel.consumeOpened()) {
            onOpened()
        }
    }

    Scaffold { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(stringResource(R.string.open_title))
            Button(onClick = { picker.launch(arrayOf("*/*")) }) {
                Text(
                    state.displayName ?: stringResource(R.string.open_pick_volume),
                )
            }
            OutlinedTextField(
                modifier = Modifier.fillMaxWidth(),
                value = password,
                onValueChange = { password = it },
                label = { Text(stringResource(R.string.open_password)) },
                singleLine = true,
            )
            if (state.errorMessage != null) {
                Text(state.errorMessage!!)
            }
            if (state.busy) {
                CircularProgressIndicator()
            } else {
                Button(
                    onClick = {
                        val chars = password.toCharArray()
                        viewModel.unlock(chars)
                        chars.fill('\u0000')
                    },
                    enabled = state.volumeUri != null && password.isNotEmpty(),
                    modifier = Modifier.testTag(TAG_OPEN_UNLOCK),
                ) {
                    Text(stringResource(R.string.open_unlock))
                }
            }
            Button(onClick = onBack) {
                Text(stringResource(R.string.open_back))
            }
        }
    }
}
