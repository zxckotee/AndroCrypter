package org.hcvault.core.mount

/** Result of probing the root environment; mounted mode is offered only for [Magisk]. */
sealed interface RootStatus {
    data object NotChecked : RootStatus
    data object NoSu : RootStatus
    data object SuDenied : RootStatus
    data object SuOtherImpl : RootStatus
    data class Magisk(val version: String) : RootStatus
}
