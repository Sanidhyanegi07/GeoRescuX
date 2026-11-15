package com.example.georescux.data.ble

import com.example.georescux.domain.ble.GeoRescueBleDiagnostics
import com.example.georescux.domain.ble.GeoRescueBlePacket
import com.example.georescux.domain.ble.GeoRescueBlePacketType
import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentMapper
import com.example.georescux.domain.incident.IncidentSeverity
import com.example.georescux.domain.incident.IncidentSyncState
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation

/**
 * The SOS <-> BLE bridge (Phase 9).
 *
 * Send side: converts a locally activated/completed [SosEmergency] into a
 * [GeoRescueBlePacket] and hands it to [GeoRescueBleManager.publishPacket].
 * The packet carries ONLY minimum emergency data: id, timestamps, location,
 * status — never credentials, tokens or personal information.
 *
 * Receive side: converts a received SOS packet back into a [SosEmergency]
 * for the app layer. The AppContainer listener persists it into the
 * existing local SOS store, where the EXISTING sync engine picks it up and
 * uploads it to Firebase when that device has Internet (Phase 10) — no
 * second sync system.
 *
 * Critical rule honored here: BLE failure must NEVER cancel or block SOS
 * activation. Every entry point is fire-and-forget and exception-contained.
 */
class BleSosBridge(
    private val manager: GeoRescueBleManager?,
    private val originDeviceId: String,
) {

    /**
     * Builds the wire packet for an SOS emergency.
     * [emergencyId] is the packet's originEmergencyId (the local record id).
     */
    fun buildSosPacket(emergency: SosEmergency, emergencyId: String): GeoRescueBlePacket =
        GeoRescueBlePacket(
            packetId = GeoRescueBlePacket.newPacketId(),
            originDeviceId = originDeviceId,
            originEmergencyId = emergencyId,
            timestampMs = emergency.startedAtMs,
            ttl = GeoRescueBlePacket.DEFAULT_TTL_HOPS,
            type = GeoRescueBlePacketType.SOS,
            latitude = emergency.location?.latitude,
            longitude = emergency.location?.longitude,
            status = if (emergency.isActive) SOS_STATUS_ACTIVE else SOS_STATUS_COMPLETED,
            payload = buildMap {
                put("emergencyId", emergencyId)
                put("status", if (emergency.isActive) SOS_STATUS_ACTIVE else SOS_STATUS_COMPLETED)
                put("startedAtMs", emergency.startedAtMs.toString())
                if (!emergency.isActive && emergency.stoppedAtMs != null) {
                    put("stoppedAtMs", emergency.stoppedAtMs.toString())
                }
                if (emergency.sequence > 0L) {
                    put("sequence", emergency.sequence.toString())
                }
                emergency.locationStatus?.let { put("locationStatus", it.name) }
                emergency.note?.let { put("note", it) }
            },
            noteText = emergency.note,
        )

    /**
     * Converts a received SOS packet into the canonical [IncidentEvent] format
     * used by [com.example.georescux.data.sos.SosIncomingEventCoordinator].
     * Returns null for non-SOS packets.
     */
    fun toIncidentEvent(packet: GeoRescueBlePacket): IncidentEvent? {
        if (packet.type != GeoRescueBlePacketType.SOS) return null

        val emergencyId = packet.originEmergencyId?.takeIf { it.isNotBlank() }
            ?: "LEGACY_${packet.originDeviceId}_${packet.timestampMs}"

        val isCompleted = packet.status == SOS_STATUS_COMPLETED
        val eventId = if (isCompleted) "$emergencyId:COMPLETED" else emergencyId

        val hasExplicitStartedAt = packet.payload.containsKey("startedAtMs")
        val hasExplicitStoppedAt = packet.payload.containsKey("stoppedAtMs")

        val startedAtMs = packet.payload["startedAtMs"]?.toLongOrNull() ?: packet.timestampMs
        val stoppedAtMs = packet.payload["stoppedAtMs"]?.toLongOrNull()
        val sequenceNum = if (packet.payload.containsKey("sequence")) {
            parseStrictSequence(packet.payload["sequence"]) ?: return null
        } else {
            0L
        }

        return IncidentEvent(
            eventId = eventId,
            originId = packet.originDeviceId,
            type = IncidentType.SOS,
            occurredAtMs = if (isCompleted) (stoppedAtMs ?: packet.timestampMs) else startedAtMs,
            latitude = packet.latitude,
            longitude = packet.longitude,
            severity = IncidentSeverity.CRITICAL,
            sequence = sequenceNum,
            signature = packet.payload["signature"],
            syncState = IncidentSyncState.RELAYED,
            payload = buildMap {
                put("emergencyId", emergencyId)
                put("status", if (isCompleted) IncidentMapper.STATUS_COMPLETED else IncidentMapper.STATUS_ACTIVE)
                if (hasExplicitStartedAt) {
                    put("startedAtMs", startedAtMs.toString())
                }
                if (isCompleted && hasExplicitStoppedAt && stoppedAtMs != null) {
                    put("stoppedAtMs", stoppedAtMs.toString())
                }
                if (sequenceNum > 0L) {
                    put("sequence", sequenceNum.toString())
                }
                packet.payload["locationStatus"]?.let { put("locationStatus", it) }
                val note = packet.noteText ?: packet.payload["note"]
                if (!note.isNullOrBlank()) {
                    put("note", note)
                }
            }
        )
    }

    /**
     * Publishes an SOS packet over BLE. Fire-and-forget: returns false (not
     * throws) for any failure — SOS activation is never blocked by BLE.
     */
    fun publishSos(emergency: SosEmergency, emergencyId: String): Boolean {
        val bleManager = manager ?: return false
        return try {
            val packet = buildSosPacket(emergency, emergencyId)
            val decision = bleManager.publishPacket(packet)
            GeoRescueBleDiagnostics.info(
                GeoRescueBleDiagnostics.SOS_PACKET_ENQUEUED,
                "packetId=${packet.packetId} emergencyId=$emergencyId decision=$decision"
            )
            true
        } catch (e: Exception) {
            // Contained by design: BLE failure never breaks SOS.
            GeoRescueBleDiagnostics.error(
                GeoRescueBleDiagnostics.BLE_ERROR,
                "code=SOS_PUBLISH_FAILED detail=${e.message}"
            )
            false
        }
    }

    /**
     * Converts a received SOS packet directly into the existing SOS model. Returns
     * null for non-SOS packets. Uses the canonical MESH_ prefix and stable emergency identity.
     */
    fun emergencyFromPacket(packet: GeoRescueBlePacket): SosEmergency? {
        if (packet.type != GeoRescueBlePacketType.SOS) return null
        val emergencyId = packet.originEmergencyId?.takeIf { it.isNotBlank() }
            ?: "LEGACY_${packet.originDeviceId}_${packet.timestampMs}"
        val location = if (packet.latitude != null && packet.longitude != null) {
            SosLocation(
                latitude = packet.latitude,
                longitude = packet.longitude,
                accuracyMeters = 0f,
                timestampMs = packet.timestampMs,
                provider = "ble_relay",
            )
        } else {
            null
        }
        val status = packet.status ?: SOS_STATUS_ACTIVE
        val isCompleted = status == SOS_STATUS_COMPLETED
        val stoppedAtMs = if (isCompleted) {
            packet.payload["stoppedAtMs"]?.toLongOrNull() ?: (packet.timestampMs + 1000L)
        } else null
        val sequenceNum = if (packet.payload.containsKey("sequence")) {
            parseStrictSequence(packet.payload["sequence"]) ?: return null
        } else {
            0L
        }

        return SosEmergency(
            id = "MESH_${packet.originDeviceId}_$emergencyId",
            startedAtMs = packet.payload["startedAtMs"]?.toLongOrNull() ?: packet.timestampMs,
            stoppedAtMs = stoppedAtMs,
            location = location,
            locationStatus = location?.let {
                runCatching {
                    com.example.georescux.domain.sos.SosLocationStatus.valueOf(
                        packet.payload["locationStatus"] ?: "ACQUIRED"
                    )
                }.getOrNull() ?: com.example.georescux.domain.sos.SosLocationStatus.ACQUIRED
            },
            note = packet.noteText ?: packet.payload["note"],
            sequence = sequenceNum,
        )
    }

    companion object {
        const val SOS_STATUS_ACTIVE = "ACTIVE"
        const val SOS_STATUS_COMPLETED = "COMPLETED"

        /**
         * Strictly validates and parses a sequence string.
         * Returns the positive or zero sequence number, or null if the string is empty,
         * contains non-digits, has leading zeroes, overflows Long.MAX_VALUE, or is negative.
         */
        fun parseStrictSequence(raw: String?): Long? {
            if (raw == null || raw.isEmpty()) return null
            if (!raw.all { it in '0'..'9' }) return null
            if (raw.length > 1 && raw.startsWith('0')) return null
            val parsed = raw.toLongOrNull() ?: return null
            if (parsed < 0L) return null
            return parsed
        }
    }
}
