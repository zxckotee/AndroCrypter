package org.hcvault.app.ui

import androidx.compose.runtime.Composable
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import org.hcvault.app.ui.browse.BrowseScreen
import org.hcvault.app.ui.open.OpenScreen

@Composable
fun HcVaultNavHost() {
    val navController = rememberNavController()
    NavHost(navController = navController, startDestination = HomeRoute) {
        composable<HomeRoute> {
            HomeScreen(
                onOpenVolume = { navController.navigate(OpenRoute) },
            )
        }
        composable<OpenRoute> {
            OpenScreen(
                onOpened = {
                    navController.navigate(BrowseRoute) {
                        popUpTo(HomeRoute)
                    }
                },
                onBack = { navController.popBackStack() },
            )
        }
        composable<BrowseRoute> {
            BrowseScreen(onClose = { navController.navigate(HomeRoute) { popUpTo(HomeRoute) } })
        }
    }
}
