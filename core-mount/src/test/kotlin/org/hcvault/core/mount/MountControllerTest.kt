package org.hcvault.core.mount

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class MountControllerTest {

    private val testDispatcher = StandardTestDispatcher()

    @Before
    fun setUp() {
        Dispatchers.setMain(testDispatcher)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun probeTransitionsFromNotCheckedToMagisk() = runTest(testDispatcher) {
        val shell = MockRootShell(granted = true)
        val controller = MountController(shell, rootProbe = { RootStatus.Magisk("26.4") })
        controller.refreshRootStatus()
        assertEquals(RootStatus.Magisk("26.4"), controller.state.value.rootStatus)
        assertEquals(MountPhase.Idle, controller.state.value.mountPhase)
    }

    @Test
    fun mountActionBlockedUntilImplemented() = runTest(testDispatcher) {
        val shell = MockRootShell(granted = true)
        val controller = MountController(shell, rootProbe = { RootStatus.Magisk("26.4") })
        advanceUntilIdle()
        controller.refreshRootStatus()
        controller.onMountActionRequested()
        advanceUntilIdle()
        val snapshot = controller.state.value
        assertEquals(MountPhase.Error, snapshot.mountPhase)
        assertEquals(MountController.NOT_IMPLEMENTED_MESSAGE, snapshot.errorMessage)
        assertFalse(MountedModeSupport.IMPLEMENTED)
    }

    @Test
    fun mountActionRejectedWithoutMagisk() = runTest(testDispatcher) {
        val shell = MockRootShell(granted = false)
        val controller = MountController(shell, rootProbe = { RootStatus.NoSu })
        controller.refreshRootStatus()
        controller.onMountActionRequested()
        val snapshot = controller.state.value
        assertEquals(MountPhase.Error, snapshot.mountPhase)
        assertTrue(snapshot.errorMessage!!.contains("Magisk"))
    }
}