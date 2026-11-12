package org.hcvault.app

import android.os.ParcelFileDescriptor
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import java.io.File
import kotlinx.coroutines.runBlocking
import org.hcvault.core.common.SecretChars
import org.hcvault.core.common.VaultResult
import org.hcvault.core.crypto.NativeVolumeEngine
import org.hcvault.core.crypto.VolumeCredentials
import org.hcvault.core.crypto.fat.Fat12RootBrowser
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class VolumeOpenInstrumentedTest {

    @Test
    fun openFixtureAndListFatRoot() {
        val args = InstrumentationRegistry.getArguments()
        val password = args.getString("TEST_VOLUME_PASSWORD")
            ?: error("TEST_VOLUME_PASSWORD instrumentation argument required")
        val path = args.getString("fixture_path")
            ?: error("fixture_path instrumentation argument required")

        val file = File(path)
        check(file.isFile) { "fixture missing at $path" }

        val engine = NativeVolumeEngine()
        val descriptor = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
        val secret = SecretChars.takeOwnership(password.toCharArray())
        try {
            runBlocking {
                when (val opened = engine.open(descriptor.fd, VolumeCredentials(secret))) {
                    is VaultResult.Ok -> {
                        assertTrue(opened.value.dataSizeBytes > 0)
                        when (val root = Fat12RootBrowser(opened.value).listRoot()) {
                            is VaultResult.Ok -> assertTrue(root.value.isNotEmpty())
                            is VaultResult.Err -> error(root.error.code)
                        }
                        opened.value.close()
                    }
                    is VaultResult.Err -> error(opened.error.code)
                }
            }
        } finally {
            secret.wipe()
            descriptor.close()
        }
    }
}
