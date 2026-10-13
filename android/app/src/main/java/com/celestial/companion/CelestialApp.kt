package com.celestial.companion

import android.app.Application
import com.celestial.companion.domain.repository.StaffRepository
import com.celestial.companion.network.StaffUdpTransport

class CelestialApp : Application() {

    lateinit var staffRepository: StaffRepository
        private set

    override fun onCreate() {
        super.onCreate()
        val udpTransport = StaffUdpTransport()
        staffRepository = StaffRepository(udpTransport)
    }
}
