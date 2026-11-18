package org.hcvault.core.mount

/** Executes shell commands in the app or mount-master root context. */
interface RootShell {
    suspend fun isAppGrantedRoot(): Boolean

    suspend fun exec(vararg command: String): ShellResult

    /** Runs in the global mount namespace when supported (libsu `-M`). */
    suspend fun execMountMaster(vararg command: String): ShellResult
}

data class ShellResult(
    val exitCode: Int,
    val stdout: List<String>,
    val stderr: List<String>,
) {
    val isSuccess: Boolean get() = exitCode == 0
}