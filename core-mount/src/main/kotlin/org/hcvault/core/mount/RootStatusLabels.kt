package org.hcvault.core.mount

fun RootStatus.summaryForHome(): Pair<String, String> = when (this) {
    RootStatus.NotChecked -> "" to ""
    RootStatus.NoSu ->
        "No root" to "Mounted mode needs Magisk (or compatible su). " +
            "You can still unlock volumes inside the app (no kernel mount)."
    RootStatus.SuDenied ->
        "Root denied" to "Grant superuser access to HCVault in Magisk, then reopen the app."
    RootStatus.SuOtherImpl ->
        "Non-Magisk su" to "Mounted mode is tested with Magisk; other su implementations are unsupported for now."
    is RootStatus.Magisk -> {
        val version = version
        if (MountedModeSupport.IMPLEMENTED) {
            "Magisk $version" to "Mounted mode is available."
        } else {
            "Magisk $version" to "Root OK — kernel mount is still in development. " +
                "Use «Open volume» for userspace browsing."
        }
    }
}