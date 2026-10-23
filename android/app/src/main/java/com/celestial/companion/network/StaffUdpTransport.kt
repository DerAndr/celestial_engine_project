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
    private val localPort: Int = ProtocolConstants.DEFAULT_PORT,
    private val context: android.content.Context? = null
) : StaffTransport {
    private var multicastLock: android.net.wifi.WifiManager.MulticastLock? = null

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

        try {
            val wifiManager = context?.applicationContext?.getSystemService(android.content.Context.WIFI_SERVICE) as? android.net.wifi.WifiManager
            multicastLock = wifiManager?.createMulticastLock("celestial_udp_lock")?.apply {
                setReferenceCounted(true)
                acquire()
                Log.i(TAG, "Acquired WifiManager.MulticastLock successfully")
            }
        } catch (e: Exception) {
            Log.w(TAG, "Could not acquire MulticastLock: ${e.message}")
        }

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

                if (targetIp == "255.255.255.255") {
                    var sentAny = false
                    try {
                        val interfaces = java.net.NetworkInterface.getNetworkInterfaces()?.toList() ?: emptyList()
                        for (iface in interfaces) {
                            if (!iface.isUp || iface.isLoopback) continue
                            for (ifaceAddr in iface.interfaceAddresses) {
                                val bcast = ifaceAddr.broadcast
                                val local = ifaceAddr.address
                                if (bcast != null && local is java.net.Inet4Address) {
                                    try {
                                        DatagramSocket(InetSocketAddress(local, 0)).use { boundSocket ->
                                            boundSocket.broadcast = true
                                            val bcastDatagram = DatagramPacket(bytes, bytes.size, bcast, targetPort)
                                            boundSocket.send(bcastDatagram)
                                            sentAny = true
                                        }
                                    } catch (_: Exception) {}
                                }
                            }
                        }
                    } catch (e: Exception) {
                        Log.w(TAG, "Multi-interface broadcast traversal: ${e.message}")
                    }
                    if (!sentAny) {
                        val s = socket ?: DatagramSocket().apply { broadcast = true }
                        s.send(datagram)
                    }
                } else {
                    val matchedLocal = findMatchingLocalAddress(targetAddr)
                    if (matchedLocal != null) {
                        try {
                            DatagramSocket(InetSocketAddress(matchedLocal, 0)).use { boundSocket ->
                                boundSocket.send(datagram)
                            }
                            return@withContext
                        } catch (e: Exception) {
                            Log.w(TAG, "Failed bound send from $matchedLocal to $targetIp: ${e.message}, falling back")
                        }
                    }
                    val s = socket ?: DatagramSocket().apply { broadcast = true }
                    s.send(datagram)
                }
            } catch (e: Exception) {
                Log.e(TAG, "UDP send failed to $targetIp:$targetPort: ${e.message}")
            }
        }
    }

    private fun findMatchingLocalAddress(target: InetAddress): InetAddress? {
        try {
            val interfaces = java.net.NetworkInterface.getNetworkInterfaces()?.toList() ?: return null
            val targetBytes = target.address
            if (targetBytes.size != 4) return null
            for (iface in interfaces) {
                if (!iface.isUp || iface.isLoopback) continue
                for (ifaceAddr in iface.interfaceAddresses) {
                    val addr = ifaceAddr.address
                    if (addr is java.net.Inet4Address && !addr.isLoopbackAddress) {
                        val addrBytes = addr.address
                        if (targetBytes[0] == addrBytes[0] &&
                            targetBytes[1] == addrBytes[1] &&
                            targetBytes[2] == addrBytes[2]
                        ) {
                            return addr
                        }
                    }
                }
            }
        } catch (_: Exception) {}
        return null
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
