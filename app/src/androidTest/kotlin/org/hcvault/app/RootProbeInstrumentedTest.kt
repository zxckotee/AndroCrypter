package org.hcvault.app

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import kotlinx.coroutines.runBlocking
import org.hcvault.core.mount.RootEnvironmentProbe
import org.hcvault.core.mount.RootStatus
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class RootProbeInstrumentedTest {

    @Test
    fun rootProbeMatchesEnvironment() {
        val expectMagisk = InstrumentationRegistry.getArguments().getString("expect_magisk") == "1"
        val status = runBlocking { RootEnvironmentProbe.probe() }
        if (expectMagisk) {
            assertTrue(
                "Expected Magisk on device; got $status. Grant su to org.hcvault.app in Magisk.",
                status is RootStatus.Magisk,
            )
        } else {
            assertTrue(
                "CI emulator should not have Magisk yet; got $status",
                status === RootStatus.NoSu || status === RootStatus.SuDenied,
            )
        }
    }
}