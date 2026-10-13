package com.celestial.companion.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * 14-byte binary packet header matching Pico SDK struct:
 *
 * typedef struct {
 *     uint32_t magic;         // 0x454C4543 ("CELE")
 *     uint8_t  version;       // 2
 *     uint8_t  msg_type;      // net_msg_type_t
 *     uint16_t seq_num;       // Monotonically increasing sequence number
 *     uint16_t payload_len;   // Byte count of payload following this header
 *     uint32_t request_id;    // Transaction identifier; heartbeat uses 0
 * } net_packet_header_t;
 */
data class CelestialPacket(
    val magic: Int = ProtocolConstants.MAGIC,
    val version: Int = ProtocolConstants.VERSION,
    val msgType: NetMessageType,
    val seqNum: Int,
    val payloadLen: Int,
    val requestId: Long,
    val payload: ByteArray = ByteArray(0)
) {
    /**
     * Serialize header and payload into a byte array in Little-Endian byte order.
     */
    fun toBytes(): ByteArray {
        val totalLen = ProtocolConstants.HEADER_SIZE + payload.size
        val buffer = ByteBuffer.allocate(totalLen).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            putInt(magic)
            put(version.toByte())
            put(msgType.code.toByte())
            putShort((seqNum and 0xFFFF).toShort())
            putShort((payloadLen and 0xFFFF).toShort())
            putInt((requestId and 0xFFFFFFFFL).toInt())
            if (payload.isNotEmpty()) {
                put(payload)
            }
        }
        return buffer.array()
    }

    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false

        other as CelestialPacket
        if (magic != other.magic) return false
        if (version != other.version) return false
        if (msgType != other.msgType) return false
        if (seqNum != other.seqNum) return false
        if (payloadLen != other.payloadLen) return false
        if (requestId != other.requestId) return false
        if (!payload.contentEquals(other.payload)) return false

        return true
    }

    override fun hashCode(): Int {
        var result = magic
        result = 31 * result + version
        result = 31 * result + msgType.hashCode()
        result = 31 * result + seqNum
        result = 31 * result + payloadLen
        result = 31 * result + requestId.hashCode()
        result = 31 * result + payload.contentHashCode()
        return result
    }

    companion object {
        /**
         * Deserialize raw UDP bytes into a CelestialPacket.
         * Returns null if magic doesn't match or buffer is truncated.
         */
        fun fromBytes(bytes: ByteArray, length: Int = bytes.size): CelestialPacket? {
            if (length < ProtocolConstants.HEADER_SIZE) return null

            val buffer = ByteBuffer.wrap(bytes, 0, length).apply {
                order(ByteOrder.LITTLE_ENDIAN)
            }

            val magic = buffer.int
            if (magic != ProtocolConstants.MAGIC) return null

            val version = buffer.get().toInt() and 0xFF
            val msgTypeCode = buffer.get().toInt() and 0xFF
            val msgType = NetMessageType.fromCode(msgTypeCode) ?: return null

            val seqNum = buffer.short.toInt() and 0xFFFF
            val payloadLen = buffer.short.toInt() and 0xFFFF
            val requestId = buffer.int.toLong() and 0xFFFFFFFFL

            val payload = if (payloadLen > 0 && buffer.remaining() >= payloadLen) {
                ByteArray(payloadLen).also { buffer.get(it) }
            } else {
                ByteArray(0)
            }

            return CelestialPacket(
                magic = magic,
                version = version,
                msgType = msgType,
                seqNum = seqNum,
                payloadLen = payloadLen,
                requestId = requestId,
                payload = payload
            )
        }
    }
}
