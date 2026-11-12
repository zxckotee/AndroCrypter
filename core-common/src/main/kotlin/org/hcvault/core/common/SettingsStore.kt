package org.hcvault.core.common

import kotlinx.coroutines.flow.Flow

/** Non-sensitive user settings only. Passwords, PIM, keyfiles and keys never go through here. */
data class AppSettings(
    val autoLockSeconds: Int = DEFAULT_AUTO_LOCK_SECONDS,
    val secureScreens: Boolean = true,
    val rememberRecentVolumes: Boolean = false,
) {
    companion object {
        const val DEFAULT_AUTO_LOCK_SECONDS = 120
    }
}

interface SettingsStore {
    val settings: Flow<AppSettings>
    suspend fun update(transform: (AppSettings) -> AppSettings)
}
