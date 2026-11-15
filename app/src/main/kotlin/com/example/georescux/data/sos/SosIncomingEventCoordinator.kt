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

        // Ordering & terminal-state protection: an out-of-order ACTIVE event must never reactivate,
        // downgrade, or overwrite an emergency that was already completed locally.
        if (existing != null && !existing.isActive && !isCompleted) {
            return null
        }

        // Sequence ordering and legacy protection rules
        if (existing != null) {
            if (existing.sequence > 0L) {
                // Existing incident is sequenced.
                if (event.sequence == 0L) {
                    // Rule: A legacy sequence-zero event cannot update or cancel an already sequenced incident.
                    return null
                }
                if (event.sequence < existing.sequence) {
                    // Rule: Stale out-of-order sequence is rejected.
                    return null
                }
                if (event.sequence == existing.sequence) {
                    // Equal sequence: conflicting lifecycle status is invalid
                    if (existing.isActive != !isCompleted) {
                        return null
                    }
                    if (isCompleted) {
                        return null // Duplicate completion
                    }
                    val newLat = event.latitude
                    val newLon = event.longitude
                    val existLat = existing.location?.latitude
                    val existLon = existing.location?.longitude
                    val newNote = event.payload["note"]
                    val existNote = existing.note
                    if (newLat == existLat && newLon == existLon && newNote == existNote) {
                        return null // Idempotent duplicate
                    } else {
                        return null // Conflicting content at same sequence: reject
                    }
                }
            } else {
                // Existing incident is legacy (sequence == 0L).
                if (existing.isActive && event.sequence == 0L && !isCompleted) {
                    val newLat = event.latitude
                    val newLon = event.longitude
                    val existLat = existing.location?.latitude
                    val existLon = existing.location?.longitude
                    val newNote = event.payload["note"]
                    val existNote = existing.note
                    if (newLat == existLat && newLon == existLon && newNote == existNote) {
                        return null // Idempotent duplicate
                    }
                }
            }
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

        val finalSequence = if (event.sequence > 0L) event.sequence else (existing?.sequence ?: 0L)

        return SosEmergency(
            id = meshId,
            startedAtMs = startedAtMs,
            stoppedAtMs = stoppedAtMs,
            location = location ?: existing?.location,
            locationStatus = locStatus ?: (if (location != null) SosLocationStatus.ACQUIRED else existing?.locationStatus),
            note = note,
            sequence = finalSequence,
        )
    }

    private val ingestionLock = Any()

    /**
     * Determines whether an emergency timestamp has expired, safe against negative values and Long overflow.
     */
    fun isEventExpired(occurredAtMs: Long, ttlSeconds: Long, nowMs: Long): Boolean {
        if (occurredAtMs <= 0L || ttlSeconds <= 0L || nowMs <= 0L) return true
        val ttlMs = if (ttlSeconds > Long.MAX_VALUE / 1000L) Long.MAX_VALUE else ttlSeconds * 1000L
        val expiryTimeMs = if (Long.MAX_VALUE - ttlMs < occurredAtMs) Long.MAX_VALUE else occurredAtMs + ttlMs
        return expiryTimeMs < nowMs
    }

    /**
     * Full production entry point for processing an incoming [IncidentEvent] against [sosStore].
     *
     * Handles:
     * - Filtering out non-SOS events, own-device echoes, and expired events.
     * - Correlation and ordering protection via [correlate].
     * - Atomic deduplication and persistence across concurrent BLE and Nearby paths.
     * - Updating [sosStore] via [LocalSosStore.addToHistory].
     *
     * Returns the reconciled [SosEmergency] if updated/saved, or `null` if ignored.
     */
    fun processIncomingSos(
        event: IncidentEvent,
        sosStore: LocalSosStore,
        selfOriginId: String? = null,
        nowMs: Long? = null,
    ): SosEmergency? = synchronized(ingestionLock) {
        if (event.type != IncidentType.SOS) return null
        if (selfOriginId != null && event.originId == selfOriginId) return null

        // Expiry check: reject expired events before persistence when nowMs is provided
        if (nowMs != null) {
            val candidateTimestamp: Long = if (isCompletionEvent(event)) {
                event.payload["stoppedAtMs"]?.toLongOrNull() ?: event.occurredAtMs
            } else {
                event.payload["startedAtMs"]?.toLongOrNull() ?: event.occurredAtMs
            }
            if (isEventExpired(candidateTimestamp, event.ttlSeconds, nowMs)) {
                return null
            }
        }

        val emergencyId = IncidentMapper.emergencyIdFor(event)
        val meshId = computeMeshId(event.originId, emergencyId)
        val canonicalExisting = sosStore.loadHistory().firstOrNull { it.id == meshId }

        var migratedOldId: String? = null
        val existing = if (canonicalExisting != null) {
            canonicalExisting
        } else {
            // Conservative pre-P0 BLE record matching: MESH_BLE_<origin>_<packetId>
            val isCompleted = isCompletionEvent(event)
            val candidateStartMs: Long? = if (isCompleted) {
                // For completion events, strictly require explicit positive startedAtMs payload.
                // Never substitute occurredAtMs (which represents stoppedAtMs).
                event.payload["startedAtMs"]?.toLongOrNull()?.takeIf { it > 0L }
            } else {
                event.payload["startedAtMs"]?.toLongOrNull()?.takeIf { it > 0L } ?: event.occurredAtMs
            }

            val legacyPrefix = "${MESH_PREFIX}_BLE_"
            val candidates = if (candidateStartMs == null) {
                emptyList()
            } else {
                sosStore.loadHistory().filter { record ->
                    if (!record.id.startsWith(legacyPrefix)) return@filter false
                    val remainder = record.id.removePrefix(legacyPrefix)
                    if (!remainder.contains('_')) return@filter false
                    val candidateOrigin = remainder.substringBeforeLast('_')
                    val candidatePacketId = remainder.substringAfterLast('_')
                    candidateOrigin.isNotBlank() &&
                        candidatePacketId.isNotBlank() &&
                        candidateOrigin == event.originId &&
                        record.startedAtMs == candidateStartMs
                }
            }

            if (candidates.size == 1) {
                val candidate = candidates.first()
                if (!candidate.isActive) {
                    // Pre-P0 record is already completed/terminal.
                    // Terminal protection: delayed ACTIVE cannot resurrect it.
                    // Repeated completion is also safely ignored.
                    return null
                }
                migratedOldId = candidate.id
                candidate
            } else {
                // Ambiguous or zero candidates: do not automatically migrate
                null
            }
        }

        val emergency = correlate(event, existing) ?: return null
        if (migratedOldId != null) {
            val migrated = sosStore.replaceInHistory(migratedOldId, emergency)
            if (!migrated) {
                // Migration persistence failed: fail closed, do not report success, preserve old record
                return null
            }
        } else {
            val saved = sosStore.addToHistory(emergency)
            if (!saved) {
                // Ordinary persistence failed: fail closed, do not report success
                return null
            }
        }
        return emergency
    }
}

