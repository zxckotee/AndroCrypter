package org.hcvault.core.common

/**
 * Wipeable holder for password-like input. Secrets live only in memory and are never
 * converted to [String], never logged and never persisted.
 */
class SecretChars private constructor(private var chars: CharArray) : AutoCloseable {

    @Volatile
    private var wiped = false

    val length: Int
        get() = chars.size

    val isEmpty: Boolean
        get() = chars.isEmpty()

    /** Runs [block] with the backing array. The array must not escape [block]. */
    fun <T> use(block: (CharArray) -> T): T {
        check(!wiped) { "secret already wiped" }
        return block(chars)
    }

    /** UTF-8 bytes of the secret; the caller owns the returned array and must wipe it. */
    fun toUtf8Bytes(): ByteArray {
        check(!wiped) { "secret already wiped" }
        val encoder = Charsets.UTF_8.newEncoder()
        val buffer = encoder.encode(java.nio.CharBuffer.wrap(chars))
        val out = ByteArray(buffer.remaining())
        buffer.get(out)
        if (buffer.hasArray()) buffer.array().fill(0)
        return out
    }

    fun wipe() {
        chars.fill('\u0000')
        chars = CharArray(0)
        wiped = true
    }

    override fun close() = wipe()

    override fun toString(): String = "SecretChars(length=$length)"

    companion object {
        /** Takes ownership of [source]: the caller's array is zeroed after copying. */
        fun takeOwnership(source: CharArray): SecretChars {
            val copy = source.copyOf()
            source.fill('\u0000')
            return SecretChars(copy)
        }

        fun empty(): SecretChars = SecretChars(CharArray(0))
    }
}
