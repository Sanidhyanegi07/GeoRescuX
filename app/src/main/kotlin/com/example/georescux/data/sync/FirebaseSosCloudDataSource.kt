package com.example.georescux.data.sync

import android.util.Log
import com.example.georescux.domain.sos.SosCloudPayload
import com.example.georescux.domain.sos.SosEmergency
import com.google.firebase.auth.FirebaseAuth
import com.google.firebase.database.FirebaseDatabase
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.tasks.await

/**
 * The ONLY class in the app that touches Firebase Realtime Database for SOS.
 * Upserts emergency records under "sos_alerts/{uid}/{alertId}" — updating an
 * existing alert writes the same node, never a duplicate.
 *
 * Firebase is touched lazily, only when an upload is actually attempted —
 * never during Application construction. All failures are contained and
 * reported as `false`: a backup problem can never affect local SOS data.
 */
class FirebaseSosCloudDataSource : SosCloudDataSource {

    private val firebaseAuth by lazy { FirebaseAuth.getInstance() }
    private val database by lazy { FirebaseDatabase.getInstance() }

    override suspend fun upsertEmergency(emergency: SosEmergency): Boolean {
        val uid = firebaseAuth.currentUser?.uid ?: return false // signed out: skip
        return try {
            val alertRef = database.getReference("sos_alerts").child(uid).child(emergency.id)
            val currentSnap = try { alertRef.child("status").get().await() } catch (_: Exception) { null }
            val cloudStatus = currentSnap?.getValue(String::class.java)

            val payload = SosCloudPayload.fromEmergency(emergency)
            if (cloudStatus == "RESOLVED" && emergency.stoppedAtMs == null) {
                // Admin has resolved this emergency on the dashboard.
                // Keep the RESOLVED status intact and update only telemetry.
                @Suppress("UNCHECKED_CAST")
                val partialUpdate = payload.filterKeys { it != "status" && payload[it] != null } as Map<String, Any>
                alertRef.updateChildren(partialUpdate).await()
            } else {
                alertRef.setValue(payload).await()
            }
            true
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            // Offline, missing database, denied rules — always best-effort.
            // Still logged: a fully silent failure is undiagnosable.
            Log.w(TAG, "SOS backup of alert ${emergency.id} failed", e)
            false
        }
    }

    override suspend fun upsertHistory(emergencies: List<SosEmergency>): Boolean {
        val uid = firebaseAuth.currentUser?.uid ?: return false // signed out: skip
        if (emergencies.isEmpty()) return true
        return try {
            val payload = SosCloudPayload.fromHistory(emergencies)
            @Suppress("UNCHECKED_CAST")
            database.getReference("sos_alerts")
                .child(uid)
                .updateChildren(payload as Map<String, Any>)
                .await()
            true
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            Log.w(TAG, "SOS history backup (${emergencies.size} alerts) failed", e)
            false
        }
    }

    private companion object {
        const val TAG = "FirebaseSosSync"
    }
}
