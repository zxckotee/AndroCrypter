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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.hcvault.app.R

const val TAG_OPEN_BUTTON = "home_open_volume"

@Composable
fun HomeScreen(onOpenVolume: () -> Unit = {}) {
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
            Button(onClick = onOpenVolume, modifier = Modifier.testTag(TAG_OPEN_BUTTON)) {
                Text(stringResource(R.string.home_open_volume))
            }
        }
    }
}
