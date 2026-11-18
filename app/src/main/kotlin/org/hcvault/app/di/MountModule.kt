package org.hcvault.app.di

import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton
import org.hcvault.core.mount.LibsuRootShell
import org.hcvault.core.mount.RootShell

@Module
@InstallIn(SingletonComponent::class)
abstract class MountModule {
    @Binds
    @Singleton
    abstract fun bindRootShell(impl: LibsuRootShell): RootShell
}