package com.celestial.companion.network

import com.celestial.companion.protocol.CelestialPacket
import kotlinx.coroutines.flow.SharedFlow

interface StaffTransport {
    val incomingPackets: SharedFlow<CelestialPacket>
    fun start()
    suspend fun sendPacket(targetIp: String, targetPort: Int, packet: CelestialPacket)
    suspend fun sendCommand(targetIp: String, cmdCode: Byte, param: Byte, authToken: Int = 0)
    fun stop()
}
