package org.hcvault.core.storage

import android.content.Context
import android.net.Uri
import android.os.ParcelFileDescriptor
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import org.hcvault.core.common.VaultError
import org.hcvault.core.common.VaultResult

/** Opens a content [Uri] or file [Uri] and returns a dup'd read-only descriptor for native I/O. */
class SafVolumeIo @Inject constructor(
    @ApplicationContext private val context: Context,
) {
    fun openReadOnly(uri: Uri): VaultResult<ParcelFileDescriptor> {
        return try {
            val descriptor = context.contentResolver.openFileDescriptor(uri, "r")
                ?: return VaultResult.Err(VaultError.Io("no descriptor"))
            VaultResult.Ok(descriptor)
        } catch (_: SecurityException) {
            VaultResult.Err(VaultError.Io("permission denied"))
        } catch (_: java.io.FileNotFoundException) {
            VaultResult.Err(VaultError.Io("not found"))
        }
    }

    fun openReadOnlyPath(path: String): VaultResult<ParcelFileDescriptor> {
        val file = java.io.File(path)
        return if (!file.isFile) {
            VaultResult.Err(VaultError.Io("not a file"))
        } else {
            try {
                VaultResult.Ok(ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY))
            } catch (_: java.io.IOException) {
                VaultResult.Err(VaultError.Io("open failed"))
            }
        }
    }

    fun displayName(uri: Uri): String? = when (uri.scheme) {
        "file" -> uri.path?.substringAfterLast('/')
        else -> {
            val projection = arrayOf(android.provider.OpenableColumns.DISPLAY_NAME)
            context.contentResolver.query(uri, projection, null, null, null)?.use { cursor ->
                if (cursor.moveToFirst()) cursor.getString(0) else null
            }
        }
    }
}
