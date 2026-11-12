package org.hcvault.core.common

import android.util.Log

/**
 * Only logging entry point of the app. It accepts plain messages only (no secret types) and is a
 * no-op unless [enabled] is set, which happens for debuggable builds only.
 */
object HcLog {
    @Volatile
    var enabled: Boolean = false

    @Suppress("ForbiddenMethodCall")
    fun d(tag: String, message: String) {
        if (enabled) Log.d(tag, message)
    }
}
