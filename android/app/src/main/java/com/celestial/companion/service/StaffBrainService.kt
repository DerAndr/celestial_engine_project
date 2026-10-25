package com.celestial.companion.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.net.wifi.WifiManager
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat
import com.celestial.companion.CelestialApp
import com.celestial.companion.MainActivity
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch

/**
 * Foreground Service that maintains active UDP socket communications,
 * high-performance Wi-Fi lock, and CPU wake lock for the Celestial Staff
 * when the phone is locked or in the user's pocket.
 */
class StaffBrainService : Service() {

    private val serviceScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private var wakeLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
        acquireLocks()
        startForeground(NOTIFICATION_ID, buildNotification("Staff Link Initializing...", false))
        observeStaffTelemetry()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf()
            return START_NOT_STICKY
        }
        return START_STICKY
    }

    @Suppress("DEPRECATION")
    private fun acquireLocks() {
        try {
            val pm = getSystemService(Context.POWER_SERVICE) as? PowerManager
            wakeLock = pm?.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "CelestialEngine::StaffBrainWakeLock")?.apply {
                setReferenceCounted(false)
                acquire(12 * 60 * 60 * 1000L) // 12 hours max guard
            }

            val wm = applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
            wifiLock = wm?.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "CelestialEngine::StaffBrainWifiLock")?.apply {
                setReferenceCounted(false)
                acquire()
            }
        } catch (_: Exception) {}
    }

    private fun releaseLocks() {
        try {
            wakeLock?.let { if (it.isHeld) it.release() }
            wifiLock?.let { if (it.isHeld) it.release() }
        } catch (_: Exception) {}
    }

    private var lastNotificationUpdateMs = 0L
    private var lastReportedOnlineState: Boolean? = null

    private fun observeStaffTelemetry() {
        val app = application as? CelestialApp ?: return
        serviceScope.launch {
            app.staffRepository.telemetry.collect { tel ->
                val now = System.currentTimeMillis()
                val onlineChanged = lastReportedOnlineState != tel.isOnline
                if (!onlineChanged && (now - lastNotificationUpdateMs < 3000L)) {
                    return@collect
                }
                lastNotificationUpdateMs = now
                lastReportedOnlineState = tel.isOnline

                val statusText = if (tel.isOnline) {
                    "Staff ONLINE (#${tel.tickNumber}) | Mode: ${tel.mode.displayName} | Vol: ${tel.volumePercent}%"
                } else {
                    "Staff OFFLINE — Waiting for connection..."
                }
                val notification = buildNotification(statusText, tel.isOnline)
                val nm = getSystemService(Context.NOTIFICATION_SERVICE) as? NotificationManager
                nm?.notify(NOTIFICATION_ID, notification)
            }
        }
    }

    private fun buildNotification(contentText: String, isOnline: Boolean): Notification {
        val openIntent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP or Intent.FLAG_ACTIVITY_CLEAR_TOP
        }
        val pendingOpen = PendingIntent.getActivity(
            this,
            0,
            openIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = Intent(this, StaffBrainService::class.java).apply {
            action = ACTION_STOP
        }
        val pendingStop = PendingIntent.getService(
            this,
            1,
            stopIntent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle(if (isOnline) "Celestial Engine — Mobile Brain Active" else "Celestial Engine — Standby")
            .setContentText(contentText)
            .setSmallIcon(android.R.drawable.ic_dialog_info)
            .setContentIntent(pendingOpen)
            .addAction(android.R.drawable.ic_menu_close_clear_cancel, "Stop", pendingStop)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Celestial Staff Service",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Keeps wireless UDP communications and voice brain active when phone is in pocket"
                setShowBadge(false)
            }
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as? NotificationManager
            nm?.createNotificationChannel(channel)
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        serviceScope.cancel()
        releaseLocks()
    }

    companion object {
        const val NOTIFICATION_ID = 2026
        const val CHANNEL_ID = "celestial_staff_service_channel"
        const val ACTION_STOP = "com.celestial.companion.action.STOP_SERVICE"

        fun start(context: Context) {
            val intent = Intent(context, StaffBrainService::class.java)
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    context.startForegroundService(intent)
                } else {
                    context.startService(intent)
                }
            } catch (_: Exception) {}
        }

        fun stop(context: Context) {
            val intent = Intent(context, StaffBrainService::class.java).apply {
                action = ACTION_STOP
            }
            context.startService(intent)
        }
    }
}
