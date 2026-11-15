package com.example.georescux.data.relay

import android.util.Log
import com.example.georescux.data.ble.BleSosBridge
import com.example.georescux.data.ble.GeoRescueBleManager
import com.example.georescux.domain.ble.GeoRescueBlePacket
import com.example.georescux.domain.ble.GeoRescueBlePacketType
import com.example.georescux.domain.ble.GeoRescueBleRelayDecision
import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentMapper
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.relay.CrossTransportDeduplicator
import com.example.georescux.domain.relay.RelayDecision
import com.example.georescux.domain.relay.RelayEngine
import com.example.georescux.domain.relay.RelayEnvelope
import com.example.georescux.domain.sos.SosEmergency

/**
 * Bi-directional bridge connecting the Bluetooth Low Energy (BLE) GATT mesh
 * with the Google Nearby Connections mesh.
 *
 * Coordinates:
 * - Forwarding accepted inbound BLE emergency packets to Nearby peers.
 * - Forwarding accepted inbound Nearby emergency events to BLE peers.
 * - Unified cross-transport loop suppression via [CrossTransportDeduplicator].
 * - Hop-decrement and TTL expiry compliance.
 * - Strict preservation of original sender identity and source-owned sequence.
 */
class CrossTransportRelayBridge(
    private val blePublisher: (GeoRescueBlePacket) -> GeoRescueBleRelayDecision,
    private val relayEngine: RelayEngine,
    private val selfOriginId: () -> String,
    val deduplicator: CrossTransportDeduplicator = CrossTransportDeduplicator(),
    private val nowMs: () -> Long = System::currentTimeMillis,
) {

    constructor(
        bleManager: GeoRescueBleManager,
        relayEngine: RelayEngine,
        selfOriginId: () -> String,
        deduplicator: CrossTransportDeduplicator = CrossTransportDeduplicator(),
        nowMs: () -> Long = System::currentTimeMillis,
    ) : this(
        blePublisher = { bleManager.publishPacket(it) },
        relayEngine = relayEngine,
        selfOriginId = selfOriginId,
        deduplicator = deduplicator,
        nowMs = nowMs,
    )

    /**
     * Forwards an accepted inbound BLE SOS packet to Nearby Connections peers.
     * Returns true if forwarded or queued, false if rejected/dropped.
     */
    fun forwardBleToNearby(packet: GeoRescueBlePacket, event: IncidentEvent): Boolean {
        if (packet.originDeviceId == selfOriginId()) return false
        if (packet.type != GeoRescueBlePacketType.SOS) return false
        if (!packet.hasHopsRemaining() || packet.ttl <= 1) return false

        val isCompleted = packet.status == BleSosBridge.SOS_STATUS_COMPLETED
        val eventTimestamp = if (isCompleted) {
            packet.payload["stoppedAtMs"]?.toLongOrNull() ?: event.occurredAtMs
        } else {
            event.occurredAtMs
        }
        if (isEventExpired(eventTimestamp, event.ttlSeconds, nowMs())) return false

        val remainingHops = packet.ttl - 1
        if (remainingHops <= 0) return false

        val originEmergencyId = packet.originEmergencyId?.takeIf { it.isNotBlank() }
            ?: IncidentMapper.emergencyIdFor(event)

        val shouldForward = deduplicator.shouldForwardToNearby(
            originDeviceId = packet.originDeviceId,
            originEmergencyId = originEmergencyId,
            sequence = event.sequence,
            isCompleted = isCompleted,
            nowMs = nowMs(),
        )
        if (!shouldForward) return false

        val updatedEvent = event.copy(
            payload = event.payload + (KEY_REMAINING_HOPS to remainingHops.toString())
        )

        val envelope = RelayEnvelope(
            event = updatedEvent,
            payload = updatedEvent.canonicalString(),
            fromPeerId = null,
        )

        return try {
            val decision = relayEngine.onEventReceived(envelope)
            decision == RelayDecision.ACCEPTED_FORWARDED || decision == RelayDecision.ACCEPTED_QUEUED
        } catch (e: Exception) {
            runCatching { Log.e(TAG, "Failed forwarding BLE SOS to Nearby", e) }
            false
        }
    }

    /**
     * Forwards an accepted inbound Nearby SOS event to BLE peers.
     * Returns true if forwarded or queued, false if rejected/dropped.
     */
    fun forwardNearbyToBle(envelope: RelayEnvelope, emergency: SosEmergency): Boolean {
        val event = envelope.event
        if (event.originId == selfOriginId()) return false
        if (event.type != IncidentType.SOS) return false

        // Expiry check safe against negative values and overflow
        if (isEventExpired(event.occurredAtMs, event.ttlSeconds, nowMs())) return false

        val budget = event.payload[KEY_REMAINING_HOPS]?.toIntOrNull()
        val nextBleTtl = if (budget != null) {
            budget - 1
        } else {
            DEFAULT_LEGACY_HOPS
        }
        if (nextBleTtl <= 0) return false

        val isCompleted = !emergency.isActive
        val originEmergencyId = IncidentMapper.emergencyIdFor(event)

        val shouldForward = deduplicator.shouldForwardToBle(
            originDeviceId = event.originId,
            originEmergencyId = originEmergencyId,
            sequence = emergency.sequence,
            isCompleted = isCompleted,
            nowMs = nowMs(),
        )
        if (!shouldForward) return false

        val blePacket = GeoRescueBlePacket(
            packetId = GeoRescueBlePacket.newPacketId(),
            originDeviceId = event.originId,
            originEmergencyId = originEmergencyId,
            timestampMs = emergency.startedAtMs,
            ttl = nextBleTtl,
            type = GeoRescueBlePacketType.SOS,
            latitude = emergency.location?.latitude ?: event.latitude,
            longitude = emergency.location?.longitude ?: event.longitude,
            status = if (isCompleted) BleSosBridge.SOS_STATUS_COMPLETED else BleSosBridge.SOS_STATUS_ACTIVE,
            payload = buildMap {
                put("emergencyId", originEmergencyId)
                put("status", if (isCompleted) BleSosBridge.SOS_STATUS_COMPLETED else BleSosBridge.SOS_STATUS_ACTIVE)
                put("startedAtMs", emergency.startedAtMs.toString())
                if (isCompleted && emergency.stoppedAtMs != null) {
                    put("stoppedAtMs", emergency.stoppedAtMs.toString())
                }
                if (emergency.sequence > 0L) {
                    put("sequence", emergency.sequence.toString())
                }
                emergency.locationStatus?.let { put("locationStatus", it.name) }
                val note = emergency.note ?: event.payload["note"]
                if (!note.isNullOrBlank()) {
                    put("note", note)
                }
                put(KEY_REMAINING_HOPS, nextBleTtl.toString())
            },
            noteText = (emergency.note ?: event.payload["note"])?.take(GeoRescueBlePacket.MAX_NOTE_LENGTH),
        )

        if (blePacket.ttl <= 0) return false

        return try {
            val decision = blePublisher(blePacket)
            decision == GeoRescueBleRelayDecision.ACCEPTED_FORWARDED || decision == GeoRescueBleRelayDecision.ACCEPTED_QUEUED
        } catch (e: Exception) {
            runCatching { Log.e(TAG, "Failed forwarding Nearby SOS to BLE", e) }
            false
        }
    }

    companion object {
        const val TAG = "CrossTransportBridge"
        const val KEY_REMAINING_HOPS = "remainingHops"
        const val DEFAULT_LEGACY_HOPS = 1

        fun isEventExpired(occurredAtMs: Long, ttlSeconds: Long, nowMs: Long): Boolean {
            if (occurredAtMs <= 0L || ttlSeconds <= 0L || nowMs <= 0L) return true
            val ttlMs = if (ttlSeconds > Long.MAX_VALUE / 1000L) Long.MAX_VALUE else ttlSeconds * 1000L
            val expiryTimeMs = if (Long.MAX_VALUE - ttlMs < occurredAtMs) Long.MAX_VALUE else occurredAtMs + ttlMs
            return expiryTimeMs < nowMs
        }
    }
}
