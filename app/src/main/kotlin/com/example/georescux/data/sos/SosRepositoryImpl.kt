package com.example.georescux.data.sos

import com.example.georescux.domain.repository.SosRepository
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus

/**
 * Local-first SOS repository. Everything is stored on the device, which is
 * why the flow works offline. Firebase sync will be added later behind the
 * same [SosRepository] interface.
 */
class SosRepositoryImpl(private val store: LocalSosStore) : SosRepository {

    private val lock = Any()

    override fun getActiveEmergency(): SosEmergency? = synchronized(lock) {
        store.loadActive()
    }

    override fun startEmergency(id: String, startedAtMs: Long): SosEmergency? = synchronized(lock) {
        // Safety rule: never allow two simultaneous emergencies.
        if (store.loadActive() != null) return null
        val emergency = SosEmergency(id = id, startedAtMs = startedAtMs, sequence = 1L)
        store.saveActive(emergency)
        emergency
    }

    override fun completeEmergency(stoppedAtMs: Long): SosEmergency? = synchronized(lock) {
        val active = store.loadActive() ?: return null
        if (active.sequence >= Long.MAX_VALUE) {
            // Sequence exhausted: reject mutation to prevent wrapping to negative
            return null
        }
        val completed = active.copy(stoppedAtMs = stoppedAtMs, sequence = active.sequence + 1L)
        val success = store.completeActive(completed)
        if (!success) return null
        completed
    }

    override fun updateActiveEmergencyLocation(location: SosLocation?, status: SosLocationStatus) = synchronized(lock) {
        // Updates the existing record in place — never creates a new
        // emergency or a history entry. Preserves last valid coordinates if current is null.
        val active = store.loadActive() ?: return
        if (active.sequence >= Long.MAX_VALUE) {
            // Sequence exhausted: reject mutation
            return
        }
        val resolvedLocation = location ?: active.location
        val nextSeq = active.sequence + 1L
        store.saveActive(active.copy(location = resolvedLocation, locationStatus = status, sequence = nextSeq))
    }

    override fun updateActiveEmergencyNote(note: String?) = synchronized(lock) {
        val active = store.loadActive() ?: return
        if (active.sequence >= Long.MAX_VALUE) {
            // Sequence exhausted: reject mutation
            return
        }
        val capNote = note?.trim()?.takeIf { it.isNotEmpty() && !it.equals("null", ignoreCase = true) }?.take(120)
        val nextSeq = active.sequence + 1L
        store.saveActive(active.copy(note = capNote, sequence = nextSeq))
    }

    override fun getHistory(): List<SosEmergency> = synchronized(lock) {
        store.loadHistory()
    }
}
