package com.example.georescux.domain.relay

/**
 * Thread-safe, bounded coordinator for cross-transport deduplication and correlation
 * between BLE and Google Nearby Connections.
 *
 * Enforces unified emergency correlation based on:
 *   `originDeviceId + originEmergencyId + sequence`
 *
 * Guarantees:
 * - A BLE-received event forwarded to Nearby is never re-forwarded to BLE (ping-pong loop prevention).
 * - A Nearby-received event forwarded to BLE is never re-forwarded to Nearby.
 * - Stale sequence updates (sequence < maxForwardedSequence) are rejected.
 * - Newer lifecycle updates (sequence > maxForwardedSequence) are forwarded.
 * - Terminal state protection: once an emergency is marked COMPLETED, subsequent ACTIVE updates
 *   are rejected and can never resurrect it.
 * - Completed events are accepted and forwarded even if an earlier ACTIVE event was forwarded.
 * - Bounded memory retention ([MAX_ENTRIES] = 500) to prevent memory growth under mesh flooding.
 */
class CrossTransportDeduplicator(
    private val maxEntries: Int = MAX_ENTRIES,
    private val sosStoreProvider: (() -> com.example.georescux.data.sos.LocalSosStore?)? = null,
) {

    private val lock = Any()

    private data class CorrelationRecord(
        val originDeviceId: String,
        val originEmergencyId: String,
        var maxSequence: Long,
        var isCompleted: Boolean,
        val forwardedToNearbySequences: MutableSet<Long> = mutableSetOf(),
        val forwardedToBleSequences: MutableSet<Long> = mutableSetOf(),
        var hasCompletedForwardedToNearby: Boolean = false,
        var hasCompletedForwardedToBle: Boolean = false,
        var lastUpdatedMs: Long = System.currentTimeMillis(),
    )

    private val records = LinkedHashMap<String, CorrelationRecord>(16, 0.75f, true)

    private fun correlationKey(originDeviceId: String, originEmergencyId: String): String =
        "${originDeviceId}_$originEmergencyId"

    /**
     * Determines whether an emergency received from BLE is eligible for forwarding to Nearby.
     * If eligible, updates internal state and returns `true`.
     */
    fun shouldForwardToNearby(
        originDeviceId: String,
        originEmergencyId: String,
        sequence: Long,
        isCompleted: Boolean,
        nowMs: Long = System.currentTimeMillis(),
    ): Boolean = synchronized(lock) {
        if (originDeviceId.isBlank() || originEmergencyId.isBlank()) return false
        val key = correlationKey(originDeviceId, originEmergencyId)
        val record = getOrCreateRecord(key, originDeviceId, originEmergencyId, nowMs)

        if (record.isCompleted) {
            if (!isCompleted) {
                // Terminal state protection: cannot forward older/delayed ACTIVE after COMPLETED
                return false
            }
            if (record.hasCompletedForwardedToNearby || record.hasCompletedForwardedToBle) {
                return false // Already forwarded completion to Nearby or loopback from BLE
            }
            record.hasCompletedForwardedToNearby = true
            record.maxSequence = maxOf(record.maxSequence, sequence)
            record.lastUpdatedMs = nowMs
            return true
        }

        if (isCompleted) {
            record.isCompleted = true
            record.hasCompletedForwardedToNearby = true
            record.maxSequence = maxOf(record.maxSequence, sequence)
            record.lastUpdatedMs = nowMs
            return true
        }

        // Active lifecycle event
        if (sequence > 0L && sequence < record.maxSequence) {
            return false // Stale sequence rejected
        }

        if (record.forwardedToNearbySequences.contains(sequence)) {
            return false // Already forwarded this exact sequence to Nearby
        }

        if (record.forwardedToBleSequences.contains(sequence)) {
            return false // Loopback suppression: sequence was already forwarded to BLE (originated from Nearby)
        }

        record.forwardedToNearbySequences.add(sequence)
        record.maxSequence = maxOf(record.maxSequence, sequence)
        record.lastUpdatedMs = nowMs
        return true
    }

    /**
     * Determines whether an emergency received from Nearby is eligible for forwarding to BLE.
     * If eligible, updates internal state and returns `true`.
     */
    fun shouldForwardToBle(
        originDeviceId: String,
        originEmergencyId: String,
        sequence: Long,
        isCompleted: Boolean,
        nowMs: Long = System.currentTimeMillis(),
    ): Boolean = synchronized(lock) {
        if (originDeviceId.isBlank() || originEmergencyId.isBlank()) return false
        val key = correlationKey(originDeviceId, originEmergencyId)
        val record = getOrCreateRecord(key, originDeviceId, originEmergencyId, nowMs)

        if (record.isCompleted) {
            if (!isCompleted) {
                // Terminal state protection: cannot forward older/delayed ACTIVE after COMPLETED
                return false
            }
            if (record.hasCompletedForwardedToBle || record.hasCompletedForwardedToNearby) {
                return false // Already forwarded completion to BLE or loopback from Nearby
            }
            record.hasCompletedForwardedToBle = true
            record.maxSequence = maxOf(record.maxSequence, sequence)
            record.lastUpdatedMs = nowMs
            return true
        }

        if (isCompleted) {
            record.isCompleted = true
            record.hasCompletedForwardedToBle = true
            record.maxSequence = maxOf(record.maxSequence, sequence)
            record.lastUpdatedMs = nowMs
            return true
        }

        // Active lifecycle event
        if (sequence > 0L && sequence < record.maxSequence) {
            return false // Stale sequence rejected
        }

        if (record.forwardedToBleSequences.contains(sequence)) {
            return false // Already forwarded this exact sequence to BLE
        }

        if (record.forwardedToNearbySequences.contains(sequence)) {
            return false // Loopback suppression: sequence was already forwarded to Nearby (originated from BLE)
        }

        record.forwardedToBleSequences.add(sequence)
        record.maxSequence = maxOf(record.maxSequence, sequence)
        record.lastUpdatedMs = nowMs
        return true
    }

    private fun getOrCreateRecord(
        key: String,
        originDeviceId: String,
        originEmergencyId: String,
        nowMs: Long,
    ): CorrelationRecord {
        val existing = records[key]
        if (existing != null) return existing

        if (records.size >= maxEntries) {
            // Evict the oldest accessed entry to bound memory
            val oldestKey = records.keys.firstOrNull()
            if (oldestKey != null) records.remove(oldestKey)
        }

        // Check persistent store fallback on cold start or post-LRU eviction
        val durableEmergency = runCatching {
            sosStoreProvider?.invoke()?.loadHistory()?.firstOrNull {
                it.id == "MESH_${originDeviceId}_$originEmergencyId"
            }
        }.getOrNull()

        val initialMaxSeq = durableEmergency?.sequence ?: 0L
        val initialCompleted = durableEmergency?.let { !it.isActive } ?: false

        val newRecord = CorrelationRecord(
            originDeviceId = originDeviceId,
            originEmergencyId = originEmergencyId,
            maxSequence = initialMaxSeq,
            isCompleted = initialCompleted,
            hasCompletedForwardedToNearby = initialCompleted,
            hasCompletedForwardedToBle = initialCompleted,
            lastUpdatedMs = nowMs,
        )
        if (durableEmergency != null && durableEmergency.sequence > 0L) {
            newRecord.forwardedToNearbySequences.add(durableEmergency.sequence)
            newRecord.forwardedToBleSequences.add(durableEmergency.sequence)
        }
        records[key] = newRecord
        return newRecord
    }

    fun clear() = synchronized(lock) {
        records.clear()
    }

    companion object {
        const val MAX_ENTRIES = 500
    }
}

