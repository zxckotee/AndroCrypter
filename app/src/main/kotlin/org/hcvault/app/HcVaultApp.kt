package org.hcvault.app

import android.app.Application
import android.content.pm.ApplicationInfo
import dagger.hilt.android.HiltAndroidApp
import org.hcvault.core.common.HcLog

@HiltAndroidApp
class HcVaultApp : Application() {
    override fun onCreate() {
        super.onCreate()
        HcLog.enabled = (applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0
    }
}
