package com.celestial.companion

import android.app.Application
import android.util.Log
import com.celestial.companion.domain.repository.StaffRepository
import com.celestial.companion.network.StaffUdpTransport

private const val TAG = "CelestialApp"

class CelestialApp : Application() {

    val staffRepository: StaffRepository by lazy {
        val udpTransport = StaffUdpTransport()
        StaffRepository(udpTransport).also {
            Log.i(TAG, "Celestial Engine StaffRepository initialized lazily")
        }
    }

    override fun onCreate() {
        super.onCreate()
        Log.i(TAG, "CelestialApp onCreate executed successfully")
    }
}
