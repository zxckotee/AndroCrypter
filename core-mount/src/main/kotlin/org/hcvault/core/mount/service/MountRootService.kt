package org.hcvault.core.mount.service

import android.content.Intent
import android.os.IBinder
import com.topjohnwu.superuser.ipc.RootService
import org.hcvault.core.mount.IMountService
import org.hcvault.core.mount.MountPhase
import org.hcvault.core.mount.MountedModeSupport

/**
 * libsu [RootService] hosting [IMountService] in the root process (skeleton).
 */
class MountRootService : RootService() {

    private val binder = object : IMountService.Stub() {
        override fun getMountPhase(): Int = MountPhase.Idle.ordinal

        override fun getActiveMountPoint(): String? = null

        override fun requestUnmount(): Boolean {
            if (!MountedModeSupport.IMPLEMENTED) return false
            return false
        }

        override fun ping() = Unit
    }

    override fun onBind(intent: Intent): IBinder = binder
}