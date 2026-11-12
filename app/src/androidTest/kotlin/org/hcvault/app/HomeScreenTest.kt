package org.hcvault.app

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithTag
import org.hcvault.app.ui.HcVaultTheme
import org.hcvault.app.ui.HomeScreen
import org.hcvault.app.ui.TAG_OPEN_BUTTON
import org.junit.Rule
import org.junit.Test

class HomeScreenTest {
    @get:Rule
    val compose = createComposeRule()

    @Test
    fun openButtonIsShown() {
        compose.setContent { HcVaultTheme { HomeScreen() } }
        compose.onNodeWithTag(TAG_OPEN_BUTTON).assertIsDisplayed()
    }
}
