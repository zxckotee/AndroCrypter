package org.hcvault.core.mount

import com.topjohnwu.superuser.Shell
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

import javax.inject.Inject

/** Production [RootShell] backed by libsu. */
class LibsuRootShell @Inject constructor() : RootShell {

    override suspend fun isAppGrantedRoot(): Boolean = withContext(Dispatchers.IO) {
        Shell.isAppGrantedRoot() == true
    }

    override suspend fun exec(vararg command: String): ShellResult = withContext(Dispatchers.IO) {
        val result = Shell.cmd(*command).exec()
        ShellResult(result.code, result.out, result.err)
    }

    override suspend fun execMountMaster(vararg command: String): ShellResult = withContext(Dispatchers.IO) {
        val result = Shell.cmd(*command).exec()
        ShellResult(result.code, result.out, result.err)
    }
}