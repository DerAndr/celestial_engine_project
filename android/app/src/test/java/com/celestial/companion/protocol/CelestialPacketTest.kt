package com.celestial.companion.protocol

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class CelestialPacketTest {

    @Test
    fun `round-trip serialization for HEARTBEAT with empty payload`() {
        val original = CelestialPacket(
            msgType = NetMessageType.HEARTBEAT,
            seqNum = 1,
            payloadLen = 0,
            requestId = 0L,
            payload = ByteArray(0),
            senderIp = "10.111.174.101"
        )
        val raw = original.toBytes()
        assertEquals(ProtocolConstants.HEADER_SIZE, raw.size)

        val decoded = CelestialPacket.fromBytes(raw, raw.size, "10.111.174.101")
        assertNotNull(decoded)
        assertEquals(original, decoded)
        assertEquals("10.111.174.101", decoded!!.senderIp)
    }

    @Test
    fun `round-trip serialization for QUERY_START and AUDIO_CHUNK`() {
        val queryPayload = byteArrayOf(0x01, 0x02, 0x03, 0x04)
        val queryStart = CelestialPacket(
            msgType = NetMessageType.QUERY_START,
            seqNum = 42,
            payloadLen = queryPayload.size,
            requestId = 1001L,
            payload = queryPayload
        )
        val queryDecoded = CelestialPacket.fromBytes(queryStart.toBytes())
        assertNotNull(queryDecoded)
        assertEquals(queryStart, queryDecoded)

        val audioPcm = ByteArray(512) { (it and 0xFF).toByte() }
        val audioChunk = CelestialPacket(
            msgType = NetMessageType.AUDIO_CHUNK,
            seqNum = 43,
            payloadLen = audioPcm.size,
            requestId = 1001L,
            payload = audioPcm
        )
        val audioDecoded = CelestialPacket.fromBytes(audioChunk.toBytes())
        assertNotNull(audioDecoded)
        assertEquals(audioChunk, audioDecoded)
        assertArrayEquals(audioPcm, audioDecoded!!.payload)
    }

    @Test
    fun `round-trip serialization for CATALOG_RESP, APP_COMMAND, APP_TIME_SYNC, and APP_TELEMETRY`() {
        val typesAndPayloads = listOf(
            NetMessageType.CATALOG_RESP to "Gemma-4-E2B|OpenRouter".toByteArray(Charsets.UTF_8),
            NetMessageType.APP_COMMAND to byteArrayOf(0x07, 0x05, 0x50, 0x03),
            NetMessageType.APP_TIME_SYNC to byteArrayOf(12, 34, 56, 0),
            NetMessageType.APP_TELEMETRY to ByteArray(32) { (it * 3).toByte() }
        )

        for ((idx, pair) in typesAndPayloads.withIndex()) {
            val (msgType, payload) = pair
            val packet = CelestialPacket(
                msgType = msgType,
                seqNum = 200 + idx,
                payloadLen = payload.size,
                requestId = 9000L + idx,
                payload = payload
            )
            val bytes = packet.toBytes()
            assertEquals(ProtocolConstants.HEADER_SIZE + payload.size, bytes.size)

            val parsed = CelestialPacket.fromBytes(bytes)
            assertNotNull("Failed for $msgType", parsed)
            assertEquals(packet, parsed)
            assertEquals(msgType, parsed!!.msgType)
            assertArrayEquals(payload, parsed.payload)
        }
    }

    @Test
    fun `rejects packet with invalid magic header`() {
        val badBuffer = ByteBuffer.allocate(ProtocolConstants.HEADER_SIZE).apply {
            order(ByteOrder.LITTLE_ENDIAN)
            putInt(0x12345678) // Wrong magic
            put(ProtocolConstants.VERSION.toByte())
            put(NetMessageType.HEARTBEAT.code.toByte())
            putShort(1)
            putShort(0)
            putInt(0)
        }
        val decoded = CelestialPacket.fromBytes(badBuffer.array())
        assertNull(decoded)
    }

    @Test
    fun `rejects truncated header shorter than 14 bytes`() {
        val validBytes = CelestialPacket(
            msgType = NetMessageType.HEARTBEAT,
            seqNum = 10
        ).toBytes()

        for (len in 0 until ProtocolConstants.HEADER_SIZE) {
            assertNull("Expected null for length $len", CelestialPacket.fromBytes(validBytes, length = len))
        }
        assertNull(CelestialPacket.fromBytes(ByteArray(5)))
    }

    @Test
    fun `handles unsigned 16-bit seqNum and unsigned 32-bit requestId correctly`() {
        val maxUint16 = 0xFFFF // 65535
        val highUint32 = 0xFEDCBA98L // > Int.MAX_VALUE (4,275,878,552)
        val payload = byteArrayOf(0xAA.toByte(), 0x55.toByte())

        val packet = CelestialPacket(
            msgType = NetMessageType.APP_COMMAND,
            seqNum = maxUint16,
            payloadLen = payload.size,
            requestId = highUint32,
            payload = payload
        )

        val decoded = CelestialPacket.fromBytes(packet.toBytes())
        assertNotNull(decoded)
        assertEquals(maxUint16, decoded!!.seqNum)
        assertEquals(highUint32, decoded.requestId)
        assertArrayEquals(payload, decoded.payload)
    }
}
