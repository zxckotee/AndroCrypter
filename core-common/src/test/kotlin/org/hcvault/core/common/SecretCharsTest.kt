package org.hcvault.core.common

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class SecretCharsTest {
    @Test
    fun takeOwnershipZeroesSource() {
        val source = "hunter2".toCharArray()
        val secret = SecretChars.takeOwnership(source)
        assertTrue(source.all { it == '\u0000' })
        assertEquals(7, secret.length)
    }

    @Test
    fun wipeClearsBackingArray() {
        val secret = SecretChars.takeOwnership("hunter2".toCharArray())
        var leaked: CharArray? = null
        secret.use { leaked = it }
        secret.wipe()
        assertTrue(leaked!!.all { it == '\u0000' })
        assertThrows(IllegalStateException::class.java) { secret.use { } }
    }

    @Test
    fun utf8BytesRoundTrip() {
        val secret = SecretChars.takeOwnership("pässwörd".toCharArray())
        val bytes = secret.toUtf8Bytes()
        assertArrayEquals("pässwörd".toByteArray(Charsets.UTF_8), bytes)
    }

    @Test
    fun toStringDoesNotLeakContent() {
        val secret = SecretChars.takeOwnership("hunter2".toCharArray())
        assertFalse(secret.toString().contains("hunter2"))
    }
}
