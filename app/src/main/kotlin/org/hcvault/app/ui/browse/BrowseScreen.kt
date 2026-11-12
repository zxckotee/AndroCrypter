package org.hcvault.app.ui.browse

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import org.hcvault.app.R

@Composable
fun BrowseScreen(
    onClose: () -> Unit,
    viewModel: BrowseViewModel = hiltViewModel(),
) {
    val state by viewModel.state.collectAsState()

    Scaffold { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(
                stringResource(
                    R.string.browse_header,
                    state.dataSizeBytes,
                    state.entries.size,
                ),
            )
            when {
                state.loading -> CircularProgressIndicator()
                state.errorMessage != null -> Text(state.errorMessage!!)
                else -> {
                    LazyColumn(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                        items(state.entries, key = { it.name }) { entry ->
                            val suffix = if (entry.isDirectory) "/" else " (${entry.sizeBytes} B)"
                            Text(entry.name + suffix)
                        }
                    }
                }
            }
            Button(
                onClick = {
                    viewModel.closeSession()
                    onClose()
                },
            ) {
                Text(stringResource(R.string.browse_close))
            }
        }
    }
}
