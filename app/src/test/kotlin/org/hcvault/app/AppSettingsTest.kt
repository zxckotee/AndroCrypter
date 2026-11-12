package org.hcvault.app

import org.hcvault.core.common.AppSettings
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AppSettingsTest {
    @Test
    fun defaultsAreConservative() {
        val defaults = AppSettings()
        assertTrue(defaults.secureScreens)
        assertFalse(defaults.rememberRecentVolumes)
        assertEquals(AppSettings.DEFAULT_AUTO_LOCK_SECONDS, defaults.autoLockSeconds)
    }
}
