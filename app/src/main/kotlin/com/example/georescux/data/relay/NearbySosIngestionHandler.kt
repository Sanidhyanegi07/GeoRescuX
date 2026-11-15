package com.example.georescux.data.relay

import android.util.Log
import com.example.georescux.data.sos.LocalSosStore
import com.example.georescux.data.sos.SosIncomingEventCoordinator
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.relay.RelayEnvelope
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch

/**
 * Handles background ingestion for incoming Nearby Connections SOS events.
 *
 * Incoming Google Nearby Connections payloads are delivered by Google Play Services
 * on the Android main looper. In-memory and on-device persistence (e.g. SharedPreferences)
 * strictly rejects main-thread persistence to ensure persistence durability contracts.
 *
 * This handler offloads Nearby SOS coordinator reconciliation, persistence, heads-up
 * notification, and Firebase sync staging to a background [CoroutineScope] with a
 * single-threaded sequential [dispatcher] (e.g. `Dispatchers.IO.limitedParallelism(1)`),
 * ensuring strict FIFO event ordering across rapid lifecycle updates.
 */
class NearbySosIngestionHandler(
    private val sosStore: LocalSosStore,
    private val selfOriginId: () -> String,
    private val onNotifyAlert: (originId: String, startedAtMs: Long, note: String?) -> Unit,
    private val onMarkPendingSync: (emergencyId: String) -> Unit,
    private val scope: CoroutineScope,
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO.limitedParallelism(1),
    private val onForwardToBle: ((envelope: RelayEnvelope, emergency: com.example.georescux.domain.sos.SosEmergency) -> Unit)? = null,
    private val nowMs: (() -> Long)? = null,
) {

    fun handleEnvelope(envelope: RelayEnvelope): Job? {
        val event = envelope.event
        if (event.type != IncidentType.SOS) return null

        return scope.launch(dispatcher) {
            try {
                val emergency = SosIncomingEventCoordinator.processIncomingSos(
                    event = event,
                    sosStore = sosStore,
                    selfOriginId = selfOriginId(),
                    nowMs = nowMs?.invoke(),
                ) ?: return@launch

                if (emergency.isActive) {
                    onNotifyAlert(event.originId, emergency.startedAtMs, emergency.note)
                }

                onMarkPendingSync(emergency.id)

                onForwardToBle?.invoke(envelope, emergency)
            } catch (e: Exception) {
                runCatching {
                    Log.e(TAG, "Failed to process incoming Nearby SOS event", e)
                }
            }
        }
    }

    private companion object {
        const val TAG = "NearbySosIngestion"
    }
}
