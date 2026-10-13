package com.celestial.companion.protocol

/**
 * Message types matching Celestial Engine Mk.V net_protocol.h
 */
enum class NetMessageType(val code: Int) {
    HEARTBEAT(0x01),
    QUERY_START(0x02),
    AUDIO_CHUNK(0x03),
    QUERY_END(0x04),
    RESP_START(0x05),
    RESP_CHUNK(0x06),
    RESP_END(0x07),
    CANCEL(0x08),
    RESP_TEXT(0x09),
    ACK(0x0A),
    PROGRESS(0x0B),
    ERROR(0x0C),

    // Extensions for Android Companion Remote Control:
    APP_COMMAND(0x20),     // Set mode, animation, volume, SFX
    APP_TIME_SYNC(0x21),   // Push local Android system time to Chronos clock
    APP_TELEMETRY(0x22);   // Full staff telemetry broadcast

    companion object {
        fun fromCode(code: Int): NetMessageType? {
            return entries.find { it.code == code }
        }
    }
}
