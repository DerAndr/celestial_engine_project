package com.celestial.companion.network

import android.util.Log
import com.celestial.companion.protocol.CelestialPacket
import com.celestial.companion.protocol.NetMessageType
import com.celestial.companion.protocol.ProtocolConstants
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.net.InetSocketAddress
import java.util.concurrent.atomic.AtomicInteger

private const val TAG = "StaffUdpTransport"

class StaffUdpTransport(
    private val localPort: Int = ProtocolConstants.DEFAULT_PORT
) : StaffTransport {

    private var socket: DatagramSocket? = null
    private val seqCounter = AtomicInteger(1)
    @Volatile
    private var isRunning = false
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private val _incomingPackets = MutableSharedFlow<CelestialPacket>(extraBufferCapacity = 64)
    override val incomingPackets: SharedFlow<CelestialPacket> = _incomingPackets

    override fun start() {
        if (isRunning) return
        isRunning = true

        scope.launch {
            while (isActive && isRunning) {
                // Initialize socket if not already open
                if (socket == null || socket?.isClosed == true) {
                    try {
                        val s = DatagramSocket(null)
                        s.reuseAddress = true
                        try {
                            s.bind(InetSocketAddress(localPort))
                            Log.i(TAG, "Bound UDP socket to port $localPort")
                        } catch (e: Exception) {
                            Log.w(TAG, "Port $localPort busy, binding to ephemeral port: ${e.message}")
                            s.bind(InetSocketAddress(0))
                        }
                        // Set broadcast and timeout strictly after binding
                        s.broadcast = true
                        s.soTimeout = 2000
                        socket = s
                    } catch (e: Exception) {
                        Log.e(TAG, "Failed to initialize DatagramSocket: ${e.message}", e)
                        delay(2000)
                        continue
                    }
                }

                val currentSocket = socket
                if (currentSocket == null || currentSocket.isClosed) {
                    delay(1000)
                    continue
                }

                val rxBuffer = ByteArray(2048)
                try {
                    val datagram = DatagramPacket(rxBuffer, rxBuffer.size)
                    currentSocket.receive(datagram)
                    val senderIp = datagram.address?.hostAddress ?: ""
                    val packet = CelestialPacket.fromBytes(datagram.data, datagram.length, senderIp)
                    if (packet != null) {
                        _incomingPackets.emit(packet)
                    }
                } catch (e: java.net.SocketTimeoutException) {
                    // Periodic timeout to evaluate isRunning/isActive
                } catch (e: Exception) {
                    if (!isRunning) break
                    Log.w(TAG, "UDP receive exception: ${e.message}")
                    delay(500)
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
                val s = socket ?: DatagramSocket().apply {
                    broadcast = true
                }
                s.send(datagram)
            } catch (e: Exception) {
                Log.e(TAG, "UDP send failed to $targetIp:$targetPort: ${e.message}")
            }
        }
    }

    override suspend fun sendCommand(targetIp: String, cmdCode: Byte, param: Byte, authToken: Int) {
        val payload = ByteArray(6)
        payload[0] = (authToken and 0xFF).toByte()
        payload[1] = ((authToken shr 8) and 0xFF).toByte()
        payload[2] = ((authToken shr 16) and 0xFF).toByte()
        payload[3] = ((authToken shr 24) and 0xFF).toByte()
        payload[4] = cmdCode
        payload[5] = param

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
        try {
            socket?.close()
        } catch (_: Exception) {}
        socket = null
        scope.cancel()
    }
}
