package com.example.georescux.data.incident

import android.util.Log
import com.google.firebase.database.DataSnapshot
import com.google.firebase.database.DatabaseError
import com.google.firebase.database.FirebaseDatabase
import com.google.firebase.database.ValueEventListener

/**
 * Cloud synchronizer for disaster/hazard incident reports with Firebase Realtime Database.
 * Works symmetrically:
 * 1. Pushes local reports to `/incidents/{id}`.
 * 2. Listens for live updates from other users or emergency responders and updates local store.
 */
class FirebaseIncidentSync(
    private val localStore: IncidentStore,
) {
    private val database by lazy { FirebaseDatabase.getInstance() }
    private var incidentsListener: ValueEventListener? = null

    /**
     * Publishes an incident report to Firebase Realtime Database.
     */
    fun publishIncident(
        incident: IncidentReport,
        onComplete: ((Boolean) -> Unit)? = null,
    ) {
        // Always persist locally first so it is available offline immediately
        localStore.saveIncident(incident)

        try {
            val ref = database.getReference("incidents").child(incident.id)
            ref.setValue(incident.toMap())
                .addOnSuccessListener {
                    Log.d(TAG, "Incident ${incident.id} synced to Firebase successfully.")
                    onComplete?.invoke(true)
                }
                .addOnFailureListener { e ->
                    Log.w(TAG, "Failed to sync incident to Firebase (offline or permission issue): ${e.message}")
                    onComplete?.invoke(false)
                }
        } catch (e: Exception) {
            Log.w(TAG, "Error initiating Firebase incident upload: ${e.message}")
            onComplete?.invoke(false)
        }
    }

    /**
     * Starts observing `/incidents` for real-time additions and status updates.
     */
    fun startObserving(onUpdated: (List<IncidentReport>) -> Unit) {
        if (incidentsListener != null) return

        val ref = database.getReference("incidents")
        incidentsListener = object : ValueEventListener {
            override fun onDataChange(snapshot: DataSnapshot) {
                try {
                    for (child in snapshot.children) {
                        val id = child.key ?: continue
                        @Suppress("UNCHECKED_CAST")
                        val map = child.value as? Map<String, Any?> ?: continue
                        val cloudIncident = IncidentReport.fromMap(id, map)
                        // Merge into local store
                        localStore.saveIncident(cloudIncident)
                    }
                } catch (e: Exception) {
                    Log.w(TAG, "Error parsing incidents from Firebase", e)
                }

                // Notify callback with full combined local + cloud list
                onUpdated(localStore.getAllIncidents())
            }

            override fun onCancelled(error: DatabaseError) {
                Log.w(TAG, "Incidents listener cancelled: ${error.message}")
            }
        }.also { ref.addValueEventListener(it) }
    }

    /**
     * Stops observing cloud updates.
     */
    fun stopObserving() {
        incidentsListener?.let {
            try {
                database.getReference("incidents").removeEventListener(it)
            } catch (_: Exception) {}
            incidentsListener = null
        }
    }

    companion object {
        private const val TAG = "FirebaseIncidentSync"
    }
}
