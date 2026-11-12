package org.hcvault.app.session

import javax.inject.Inject
import javax.inject.Singleton
import org.hcvault.core.crypto.VolumeSession

/** Holds the active decrypted volume for the UI layer; wiped on [clear]. */
@Singleton
class SessionManager @Inject constructor() {
    private var session: VolumeSession? = null

    val active: VolumeSession?
        get() = session

    fun attach(opened: VolumeSession) {
        clear()
        session = opened
    }

    fun clear() {
        session?.close()
        session = null
    }
}
