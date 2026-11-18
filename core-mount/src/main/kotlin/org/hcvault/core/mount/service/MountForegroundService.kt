package org.hcvault.core.mount.service

import android.app.Service
import android.content.Intent
import android.os.IBinder
import org.hcvault.core.mount.MountForegroundContract

/**
 * App-process foreground anchor while a volume is kernel-mounted (placeholder).
 *
 * Start via [MountForegroundContract.buildStartIntent]; implementation completes when sim acceptance passes.
 */
class MountForegroundService : Service() {

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val mountPoint = intent?.getStringExtra(MountForegroundContract.EXTRA_MOUNT_POINT)
        val notification = MountForegroundContract.placeholderNotification(
            context = this,
            title = "HCVault mount",
            text = mountPoint?.let { "Mounted at $it" } ?: "Mount service (not active)",
            unmountIntent = null,
        )
        startForeground(MountForegroundContract.NOTIFICATION_ID, notification)
        return START_STICKY
    }
}