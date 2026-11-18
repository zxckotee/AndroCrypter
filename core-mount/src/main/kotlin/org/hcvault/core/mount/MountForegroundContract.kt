package org.hcvault.core.mount

import android.app.Notification
import android.app.PendingIntent
import android.content.Context
import android.content.Intent

/**
 * Shared constants and helpers for the mount [android.app.Service] foreground notification.
 * Full wiring lands when mounted mode is enabled (E6 follow-up).
 */
object MountForegroundContract {
    const val SERVICE_CLASS = "org.hcvault.core.mount.service.MountForegroundService"

    const val NOTIFICATION_CHANNEL_ID = "hcvault_mount"
    const val NOTIFICATION_ID = 0x4856_14

    const val ACTION_UNMOUNT = "org.hcvault.core.mount.action.UNMOUNT"
    const val ACTION_OPEN_APP = "org.hcvault.core.mount.action.OPEN_APP"

    const val EXTRA_MOUNT_POINT = "mount_point"

    fun buildStartIntent(context: Context, mountPoint: String?): Intent =
        Intent().setClassName(context, SERVICE_CLASS).putExtra(EXTRA_MOUNT_POINT, mountPoint)

    fun placeholderNotification(
        context: Context,
        title: String,
        text: String,
        unmountIntent: PendingIntent?,
    ): Notification {
        // Skeleton: callers replace with NotificationCompat when the service is implemented.
        @Suppress("DEPRECATION")
        return Notification.Builder(context, NOTIFICATION_CHANNEL_ID)
            .setContentTitle(title)
            .setContentText(text)
            .setSmallIcon(android.R.drawable.ic_menu_upload)
            .apply {
                if (unmountIntent != null) {
                    addAction(
                        Notification.Action.Builder(null, "Unmount", unmountIntent).build(),
                    )
                }
            }
            .build()
    }
}