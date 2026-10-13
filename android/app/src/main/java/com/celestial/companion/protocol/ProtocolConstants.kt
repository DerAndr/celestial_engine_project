package com.celestial.companion.protocol

object ProtocolConstants {
    /**
     * Binary protocol magic: 0x454C4543 ('C', 'E', 'L', 'E' in Little-Endian)
     */
    const val MAGIC: Int = 0x454C4543

    /**
     * Protocol version
     */
    const val VERSION: Int = 2

    /**
     * Standard UDP port for staff streaming and telemetry
     */
    const val DEFAULT_PORT: Int = 5005

    /**
     * Maximum payload size per UDP packet
     */
    const val MAX_PAYLOAD_SIZE: Int = 1024

    /**
     * Fixed binary header size (14 bytes)
     */
    const val HEADER_SIZE: Int = 14
}
