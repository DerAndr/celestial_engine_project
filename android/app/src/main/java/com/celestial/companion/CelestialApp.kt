package com.celestial.companion

import android.app.Application
import android.util.Log
import com.celestial.companion.domain.repository.StaffRepository
import com.celestial.companion.network.StaffUdpTransport
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter

private const val TAG = "CelestialApp"

class CelestialApp : Application() {

    private var _staffRepository: StaffRepository? = null

    val staffRepository: StaffRepository
        get() = _staffRepository ?: StaffRepository(StaffUdpTransport()).also { _staffRepository = it }

    override fun onCreate() {
        super.onCreate()

        // Set global crash handler to log any uncaught exception to logcat & local file
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            try {
                Log.e(TAG, "FATAL CRASH on thread ${thread.name}", throwable)
                val sw = StringWriter()
                throwable.printStackTrace(PrintWriter(sw))
                val crashFile = File(filesDir, "last_crash.txt")
                crashFile.writeText("Time: ${System.currentTimeMillis()}\nThread: ${thread.name}\n$sw")
            } catch (_: Exception) {}
            defaultHandler?.uncaughtException(thread, throwable)
        }

        try {
            val udpTransport = StaffUdpTransport()
            staffRepository = StaffRepository(udpTransport)
            Log.i(TAG, "Celestial Engine companion initialized successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error initializing StaffRepository: ${e.message}", e)
        }
    }
}
