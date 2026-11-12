package org.hcvault.app.di

import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton
import org.hcvault.core.crypto.NativeVolumeEngine
import org.hcvault.core.crypto.VolumeEngine

@Module
@InstallIn(SingletonComponent::class)
abstract class CryptoModule {
    @Binds
    @Singleton
    abstract fun bindVolumeEngine(impl: NativeVolumeEngine): VolumeEngine
}
