package com.example.georescux.data.sos

import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentMapper
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus

/**
 * Single authoritative production coordinator for correlating incoming SOS mesh/relay events
 * into local [SosEmergency] models.
 *
 * Responsibilities:
 * - Stable emergency identity and MESH ID generation.
 * - Exact timestamp preservation for new-format completion events (no artificial delay).
 * - Documented fallback compatibility for legacy completion events.
 * - Out-of-order protection: prevents delayed ACTIVE events from resurrecting or downgrading
 *   completed emergencies.
 * - Completion-before-start support: reconstructs completed emergency with transmitted
 *   original start time and completion time.
 * - Safe merging of metadata (location, notes, location status).
 */
object SosIncomingEventCoordinator {

    const val MESH_PREFIX = "MESH"

    /** Computes the stable mesh emergency ID correlating all events for a given emergency from an origin. */
    fun computeMeshId(originId: String, emergencyId: String): String =
        "${MESH_PREFIX}_${originId}_$emergencyId"

    /**
     * Determines whether [event] represents an emergency completion (stop) event.
     */
    fun isCompletionEvent(event: IncidentEvent): Boolean {
        val status = event.payload["status"]
        return status == IncidentMapper.STATUS_COMPLETED ||
            event.eventId.endsWith(":COMPLETED") ||
            event.eventId.endsWith(":STOP")
    }

    /**
     * Identifies legacy completion events that lack explicit lifecycle timestamps
     * (`startedAtMs` and `stoppedAtMs`).
     *
     * In legacy GeoRescuX versions:
     * - `event.occurredAtMs` was set to the emergency's original `startedAtMs`.
     * - There was no explicit `startedAtMs` or `stoppedAtMs` in `payload`.
     * - Receivers synthesized `stoppedAtMs = occurredAtMs + 1000L` to ensure positive duration.
     */
    fun isLegacyCompletion(event: IncidentEvent): Boolean =
        isCompletionEvent(event) &&
            !event.payload.containsKey("startedAtMs") &&
            !event.payload.containsKey("stoppedAtMs")

    /**
     * Pure correlation function that calculates the next [SosEmergency] state given an incoming
     * [IncidentEvent] and an optional [existing] emergency with the same correlated ID.
     *
     * Returns:
     * - `null` if the event is rejected (e.g. an out-of-order ACTIVE event for an already-completed emergency).
     * - A [SosEmergency] representing the reconciled emergency state to be persisted.
     */
    fun correlate(event: IncidentEvent, existing: SosEmergency?): SosEmergency? {
        val isCompleted = isCompletionEvent(event)

        // Ordering protection: an out-of-order ACTIVE event must never reactivate,
        // downgrade, or overwrite an emergency that was already completed locally.
        if (existing != null && !existing.isActive && !isCompleted) {
            return null
        }

        val emergencyId = IncidentMapper.emergencyIdFor(event)
        val meshId = computeMeshId(event.originId, emergencyId)

        // Extract location and note metadata
        val locationStatusName = event.payload["locationStatus"]
        val locStatus = locationStatusName?.let {
            runCatching { SosLocationStatus.valueOf(it) }.getOrNull()
        }
        val location = if (event.latitude != null && event.longitude != null) {
            SosLocation(
                latitude = event.latitude,
                longitude = event.longitude,
                accuracyMeters = 0f,
                timestampMs = event.occurredAtMs,
                provider = "mesh_relay",
            )
        } else null

        val note = event.payload["note"] ?: existing?.note

        val (startedAtMs, stoppedAtMs) = if (!isCompleted) {
            // ACTIVE event: retain existing start time if present, else use payload or occurredAtMs
            val start = existing?.startedAtMs
                ?: event.payload["startedAtMs"]?.toLongOrNull()
                ?: event.occurredAtMs
            Pair(start, null)
        } else {
            // COMPLETED event
            if (isLegacyCompletion(event)) {
                // Documented legacy fallback:
                // Legacy sender sent occurredAtMs = startedAtMs.
                // Synthesize stoppedAtMs with +1000L to maintain strictly positive duration.
                val baseTime = existing?.startedAtMs ?: event.occurredAtMs
                val legacyStopped = (event.occurredAtMs + 1000L).coerceAtLeast(baseTime + 1000L)
                Pair(baseTime, legacyStopped)
            } else {
                // New-format event:
                // 1. startedAtMs is explicitly transmitted or resolved from existing record.
                val start = existing?.startedAtMs
                    ?: event.payload["startedAtMs"]?.toLongOrNull()
                    ?: event.occurredAtMs
                // 2. stoppedAtMs is the actual completion timestamp from payload or occurredAtMs.
                // No artificial second is added.
                val rawStopped = event.payload["stoppedAtMs"]?.toLongOrNull() ?: event.occurredAtMs
                val stopped = (existing?.stoppedAtMs ?: rawStopped).coerceAtLeast(start)
                Pair(start, stopped)
            }
        }

        return SosEmergency(
            id = meshId,
            startedAtMs = startedAtMs,
            stoppedAtMs = stoppedAtMs,
            location = location ?: existing?.location,
            locationStatus = locStatus ?: (if (location != null) SosLocationStatus.ACQUIRED else existing?.locationStatus),
            note = note,
        )
    }

    /**
     * Full production entry point for processing an incoming [IncidentEvent] against [sosStore].
     *
     * Handles:
     * - Filtering out non-SOS events and own-device echoes.
     * - Correlation and ordering protection via [correlate].
     * - Updating [sosStore] via [LocalSosStore.addToHistory].
     *
     * Returns the reconciled [SosEmergency] if updated/saved, or `null` if ignored.
     */
    fun processIncomingSos(
        event: IncidentEvent,
        sosStore: LocalSosStore,
        selfOriginId: String? = null,
    ): SosEmergency? {
        if (event.type != IncidentType.SOS) return null
        if (selfOriginId != null && event.originId == selfOriginId) return null

        val emergencyId = IncidentMapper.emergencyIdFor(event)
        val meshId = computeMeshId(event.originId, emergencyId)
        val existing = sosStore.loadHistory().firstOrNull { it.id == meshId }

        val emergency = correlate(event, existing) ?: return null
        sosStore.addToHistory(emergency)
        return emergency
    }
}

