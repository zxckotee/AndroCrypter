package org.hcvault.app.settings

import android.content.Context
import android.content.SharedPreferences
import androidx.core.content.edit
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.hcvault.core.common.AppSettings
import org.hcvault.core.common.SettingsStore

/** Non-sensitive settings only; see [SettingsStore] for what must never be stored here. */
class EncryptedSettingsStore(context: Context) : SettingsStore {
    private val prefs: SharedPreferences by lazy {
        val key = MasterKey.Builder(context).setKeyScheme(MasterKey.KeyScheme.AES256_GCM).build()
        EncryptedSharedPreferences.create(
            context,
            FILE_NAME,
            key,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM,
        )
    }
    private val state by lazy { MutableStateFlow(read()) }
    private val mutex = Mutex()

    override val settings: Flow<AppSettings>
        get() = state.asStateFlow()

    override suspend fun update(transform: (AppSettings) -> AppSettings) = mutex.withLock {
        val next = transform(read())
        prefs.edit {
            putInt(KEY_AUTO_LOCK, next.autoLockSeconds)
            putBoolean(KEY_SECURE, next.secureScreens)
            putBoolean(KEY_RECENT, next.rememberRecentVolumes)
        }
        state.value = next
    }

    private fun read() = AppSettings(
        autoLockSeconds = prefs.getInt(KEY_AUTO_LOCK, AppSettings.DEFAULT_AUTO_LOCK_SECONDS),
        secureScreens = prefs.getBoolean(KEY_SECURE, true),
        rememberRecentVolumes = prefs.getBoolean(KEY_RECENT, false),
    )

    private companion object {
        const val FILE_NAME = "hcvault_settings"
        const val KEY_AUTO_LOCK = "auto_lock_seconds"
        const val KEY_SECURE = "secure_screens"
        const val KEY_RECENT = "remember_recent"
    }
}
