package org.hcvault.core.mount

/**
 * In-memory [RootShell] for JVM unit tests.
 *
 * Register command handlers in [handlers]; unknown commands return exit code 127.
 */
class MockRootShell(
    private var granted: Boolean = true,
    private val handlers: MutableMap<String, ShellResult> = mutableMapOf(),
) : RootShell {

    fun setGranted(granted: Boolean) {
        this.granted = granted
    }

    fun whenCommand(command: String, result: ShellResult) {
        handlers[command] = result
    }

    val invocations: MutableList<Pair<Boolean, String>> = mutableListOf()

    override suspend fun isAppGrantedRoot(): Boolean = granted

    override suspend fun exec(vararg command: String): ShellResult =
        runRecorded(mountMaster = false, command)

    override suspend fun execMountMaster(vararg command: String): ShellResult =
        runRecorded(mountMaster = true, command)

    private fun runRecorded(mountMaster: Boolean, command: Array<out String>): ShellResult {
        val joined = command.joinToString(" ")
        invocations += mountMaster to joined
        return handlers[joined] ?: ShellResult(127, emptyList(), listOf("command not found: $joined"))
    }
}