package com.celestial.companion

import android.app.Application
import android.util.Log
import com.celestial.companion.ai.GemmaModelEngine
import com.celestial.companion.domain.repository.StaffRepository
import com.celestial.companion.network.StaffUdpTransport

private const val TAG = "CelestialApp"

class CelestialApp : Application() {

    val gemmaEngine: GemmaModelEngine by lazy {
        GemmaModelEngine(this)
    }

    val staffRepository: StaffRepository by lazy {
        val udpTransport = StaffUdpTransport()
        val prefs = getSharedPreferences("celestial_prefs", MODE_PRIVATE)
        val savedPasskey = prefs.getString("auth_passkey", "") ?: ""
        val savedIp = prefs.getString("staff_ip", "192.168.178.101") ?: "192.168.178.101"
        StaffRepository(udpTransport, gemmaEngine).also {
            it.passkey = savedPasskey
            it.setTargetStaffIp(savedIp)
            Log.i(TAG, "Celestial Engine StaffRepository initialized lazily (target: $savedIp, auth: ${savedPasskey.isNotEmpty()})")
        }
    }

    override fun onCreate() {
        super.onCreate()
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            Log.e(TAG, "FATAL UNCAUGHT EXCEPTION on thread ${thread.name}: ${throwable.message}", throwable)
            defaultHandler?.uncaughtException(thread, throwable)
        }
        Log.i(TAG, "CelestialApp onCreate executed successfully")
    }
}
