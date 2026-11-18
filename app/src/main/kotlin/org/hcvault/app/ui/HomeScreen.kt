package org.hcvault.app.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.hcvault.app.R

const val TAG_OPEN_BUTTON = "home_open_volume"
const val TAG_MOUNT_BUTTON = "home_mount_action"
const val TAG_ROOT_STATUS = "home_root_status"

@Composable
fun HomeScreen(
    onOpenVolume: () -> Unit = {},
    previewState: HomeUiState? = null,
    onMountAction: (() -> Unit)? = null,
) {
    if (previewState != null) {
        HomeScreenContent(
            state = previewState,
            onOpenVolume = onOpenVolume,
            onMountAction = onMountAction,
        )
        return
    }
    val viewModel: HomeViewModel = hiltViewModel()
    val state by viewModel.state.collectAsStateWithLifecycle()
    HomeScreenContent(
        state = state,
        onOpenVolume = onOpenVolume,
        onMountAction = { viewModel.onMountAction() },
    )
}

@Composable
private fun HomeScreenContent(
    state: HomeUiState,
    onOpenVolume: () -> Unit,
    onMountAction: (() -> Unit)?,
) {
    Scaffold { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp, Alignment.CenterVertically),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(stringResource(R.string.home_title), style = MaterialTheme.typography.headlineLarge)
            Text(stringResource(R.string.home_subtitle), style = MaterialTheme.typography.bodyMedium)
            if (state.rootTitle != null) {
                Text(
                    state.rootTitle!!,
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.testTag(TAG_ROOT_STATUS),
                )
            }
            if (state.rootDetail != null) {
                Text(state.rootDetail!!, style = MaterialTheme.typography.bodySmall)
            }
            if (state.mountStatusLine != null) {
                Text(state.mountStatusLine!!, style = MaterialTheme.typography.bodySmall)
            }
            Button(
                onClick = { onMountAction?.invoke() },
                enabled = state.mountActionEnabled && onMountAction != null,
                modifier = Modifier.testTag(TAG_MOUNT_BUTTON),
            ) {
                Text(state.mountActionLabel)
            }
            Button(onClick = onOpenVolume, modifier = Modifier.testTag(TAG_OPEN_BUTTON)) {
                Text(stringResource(R.string.home_open_volume))
            }
        }
    }
}