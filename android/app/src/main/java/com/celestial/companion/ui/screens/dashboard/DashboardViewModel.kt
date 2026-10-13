package com.celestial.companion.ui.screens.dashboard

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.celestial.companion.domain.model.*
import com.celestial.companion.domain.repository.StaffRepository
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch

class DashboardViewModel(
    private val repository: StaffRepository
) : ViewModel() {

    val telemetry: StateFlow<StaffTelemetry> = repository.telemetry

    fun setStaffIp(ip: String) {
        repository.setTargetStaffIp(ip)
    }

    fun getStaffIp(): String = repository.getTargetStaffIp()

    fun setPasskey(key: String) {
        repository.passkey = key
    }

    fun getPasskey(): String = repository.passkey

    fun saveConnectionSettings(context: android.content.Context, ip: String, passkey: String) {
        setStaffIp(ip)
        setPasskey(passkey)
        context.getSharedPreferences("celestial_prefs", android.content.Context.MODE_PRIVATE)
            .edit()
            .putString("staff_ip", ip)
            .putString("auth_passkey", passkey)
            .apply()
    }

    fun selectMode(mode: StaffMode) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetMode(mode))
        }
    }

    fun setVolume(volumePercent: Int) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetVolume(volumePercent))
        }
    }

    fun setClockStyle(style: ClockStyle) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetClockStyle(style))
        }
    }

    fun setLedAnimation(anim: LedAnimation) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SetLedAnimation(anim))
        }
    }

    fun playSfx(sfxId: Int) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.PlaySfx(sfxId))
        }
    }

    fun playChiptune(trackId: Int) {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.PlayChiptune(trackId))
        }
    }

    fun stopAudio() {
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.StopAudio)
        }
    }

    fun syncSystemTime() {
        val now = System.currentTimeMillis()
        val seconds = now / 1000
        val millis = (now % 1000).toInt()
        viewModelScope.launch {
            repository.executeCommand(StaffCommand.SyncTime(seconds, millis))
        }
    }
}
