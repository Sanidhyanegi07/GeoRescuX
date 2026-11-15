package com.example.georescux.data.sos

import com.example.georescux.domain.sos.SosEmergency

/**
 * Local on-device storage for SOS records — deliberately not Firebase,
 * so Phase 1 works completely offline. One active emergency is kept,
 * plus a small history of finished emergencies.
 */
interface LocalSosStore {
    fun loadActive(): SosEmergency?
    fun saveActive(emergency: SosEmergency)
    fun clearActive()
    fun loadHistory(): List<SosEmergency>

    /**
     * Appends or replaces an emergency record in history.
     * Returns true if persistence succeeded, false otherwise.
     */
    fun addToHistory(emergency: SosEmergency): Boolean

    /**
     * Replaces an existing history record identified by [replacedId] with [emergency].
     * If [replacedId] matches [emergency.id], this acts as an upsert.
     * If [replacedId] differs (e.g. migrating an old legacy ID to canonical mesh ID),
     * the record with [replacedId] is removed and [emergency] is inserted.
     * Returns true if persistence succeeded, false otherwise.
     */
    fun replaceInHistory(replacedId: String, emergency: SosEmergency): Boolean {
        return addToHistory(emergency)
    }

    /**
     * Atomically transitions an emergency from active storage to completed history.
     * Returns true if persistence succeeded, false otherwise.
     * Default implementation ensures addToHistory completes before clearActive,
     * preventing incident loss on crash. Stores can override with a single atomic batch.
     */
    fun completeActive(completed: SosEmergency): Boolean {
        val success = addToHistory(completed)
        if (!success) return false
        clearActive()
        return true
    }
}
