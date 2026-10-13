package com.celestial.companion.network

import com.celestial.companion.protocol.CelestialPacket
import com.celestial.companion.protocol.NetMessageType
import com.celestial.companion.protocol.ProtocolConstants
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.util.concurrent.atomic.AtomicInteger

class StaffUdpTransport(
    private val localPort: Int = ProtocolConstants.DEFAULT_PORT
) : StaffTransport {

    private var socket: DatagramSocket? = null
    private val seqCounter = AtomicInteger(1)
    private var isRunning = false
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private val _incomingPackets = MutableSharedFlow<CelestialPacket>(extraBufferCapacity = 64)
    override val incomingPackets: SharedFlow<CelestialPacket> = _incomingPackets

    override fun start() {
        if (isRunning) return
        isRunning = true

        scope.launch {
            try {
                socket = DatagramSocket(localPort).apply {
                    broadcast = true
                    reuseAddress = true
                }
            } catch (e: Exception) {
                // If standard port is in use, bind to ephemeral port for outbound control
                socket = DatagramSocket().apply {
                    broadcast = true
                }
            }

            val rxBuffer = ByteArray(2048)
            while (isActive && isRunning) {
                try {
                    val datagram = DatagramPacket(rxBuffer, rxBuffer.size)
                    socket?.receive(datagram)
                    val packet = CelestialPacket.fromBytes(datagram.data, datagram.length)
                    if (packet != null) {
                        _incomingPackets.emit(packet)
                    }
                } catch (e: Exception) {
                    if (!isRunning) break
                }
            }
        }
    }

    override suspend fun sendPacket(targetIp: String, targetPort: Int, packet: CelestialPacket) {
        withContext(Dispatchers.IO) {
            try {
                val bytes = packet.toBytes()
                val targetAddr = InetAddress.getByName(targetIp)
                val datagram = DatagramPacket(bytes, bytes.size, targetAddr, targetPort)
                socket?.send(datagram)
            } catch (e: Exception) {
                // Handle socket transmit error
            }
        }
    }

    override suspend fun sendCommand(targetIp: String, cmdCode: Byte, param: Byte) {
        val payload = byteArrayOf(cmdCode, param)
        val packet = CelestialPacket(
            msgType = NetMessageType.APP_COMMAND,
            seqNum = seqCounter.getAndIncrement() and 0xFFFF,
            payloadLen = payload.size,
            requestId = 0L,
            payload = payload
        )
        sendPacket(targetIp, ProtocolConstants.DEFAULT_PORT, packet)
    }

    override fun stop() {
        isRunning = false
        socket?.close()
        socket = null
        scope.cancel()
    }
}
