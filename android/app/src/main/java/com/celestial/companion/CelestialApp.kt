package com.celestial.companion

import android.app.Application
import android.util.Log
import com.celestial.companion.ai.DualEngineProvider
import com.celestial.companion.ai.GemmaModelEngine
import com.celestial.companion.ai.OfflineEdgeEngine
import com.celestial.companion.domain.repository.StaffRepository
import com.celestial.companion.network.StaffUdpTransport

private const val TAG = "CelestialApp"

class CelestialApp : Application() {

    @Volatile
    private var staffRepoInitialized = false

    @Volatile
    var currentActivity: java.lang.ref.WeakReference<android.app.Activity>? = null

    val gemmaEngine: GemmaModelEngine by lazy {
        GemmaModelEngine(this)
    }

    val offlineEngine: OfflineEdgeEngine by lazy {
        OfflineEdgeEngine()
    }

    val dualEngineProvider: DualEngineProvider by lazy {
        DualEngineProvider(gemmaEngine, offlineEngine, this)
    }

    val staffRepository: StaffRepository by lazy {
        val udpTransport = StaffUdpTransport(context = this)
        val prefs = getSharedPreferences("celestial_prefs", MODE_PRIVATE)
        val savedPasskey = prefs.getString("auth_passkey", "apophis")?.ifEmpty { "apophis" } ?: "apophis"
        val savedIp = prefs.getString("staff_ip", "10.111.174.64") ?: "10.111.174.64"
        StaffRepository(
            transport = udpTransport,
            gemmaEngine = gemmaEngine,
            dualEngineProvider = dualEngineProvider,
            context = this
        ).also {
            it.passkey = savedPasskey
            it.setTargetStaffIp(savedIp)
            staffRepoInitialized = true
            Log.i(TAG, "Celestial Engine StaffRepository initialized lazily (target: $savedIp, auth passkey: $savedPasskey)")
        }
    }

    /**
     * Cleanly stops all background services, closes UDP sockets, releases WakeLocks/TTS,
     * removes the activity task from Recents, and unloads the process from memory.
     */
    fun shutdownAndExit(activity: android.app.Activity? = null) {
        Log.i(TAG, "shutdownAndExit: unloading Celestial Companion from memory...")
        try {
            if (staffRepoInitialized) {
                staffRepository.shutdown()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error shutting down staffRepository: ${e.message}")
        }

        try {
            val nm = getSystemService(NOTIFICATION_SERVICE) as? android.app.NotificationManager
            nm?.cancel(com.celestial.companion.service.StaffBrainService.NOTIFICATION_ID)
        } catch (_: Exception) {}

        try {
            stopService(android.content.Intent(this, com.celestial.companion.service.StaffBrainService::class.java))
        } catch (_: Exception) {}

        val targetActivity = activity ?: currentActivity?.get()
        try {
            targetActivity?.finishAndRemoveTask()
        } catch (_: Exception) {
            try { targetActivity?.finishAffinity() } catch (_: Exception) {}
        }

        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
            android.os.Process.killProcess(android.os.Process.myPid())
            kotlin.system.exitProcess(0)
        }, 120L)
    }

    override fun onCreate() {
        super.onCreate()
        registerActivityLifecycleCallbacks(object : ActivityLifecycleCallbacks {
            override fun onActivityCreated(activity: android.app.Activity, savedInstanceState: android.os.Bundle?) {
                currentActivity = java.lang.ref.WeakReference(activity)
            }
            override fun onActivityStarted(activity: android.app.Activity) {
                currentActivity = java.lang.ref.WeakReference(activity)
            }
            override fun onActivityResumed(activity: android.app.Activity) {
                currentActivity = java.lang.ref.WeakReference(activity)
            }
            override fun onActivityPaused(activity: android.app.Activity) {}
            override fun onActivityStopped(activity: android.app.Activity) {}
            override fun onActivitySaveInstanceState(activity: android.app.Activity, outState: android.os.Bundle) {}
            override fun onActivityDestroyed(activity: android.app.Activity) {
                if (currentActivity?.get() === activity) {
                    currentActivity = null
                }
            }
        })
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, throwable ->
            Log.e(TAG, "FATAL UNCAUGHT EXCEPTION on thread ${thread.name}: ${throwable.message}", throwable)
            defaultHandler?.uncaughtException(thread, throwable)
        }
        Log.i(TAG, "CelestialApp onCreate executed successfully")
    }
}
