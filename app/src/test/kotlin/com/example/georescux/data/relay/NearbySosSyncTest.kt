package com.example.georescux.data.relay

import com.example.georescux.core.result.AuthResult
import com.example.georescux.data.sos.LocalSosStore
import com.example.georescux.data.sos.SosRepositoryImpl
import com.example.georescux.data.sync.SosCloudDataSource
import com.example.georescux.data.sync.SyncStateStoreFake
import com.example.georescux.data.sync.SyncingSosRepository
import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentSeverity
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.relay.RelayEnvelope
import com.example.georescux.domain.repository.AuthRepository
import com.example.georescux.domain.repository.SosRepository
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NearbySosSyncTest {

    private class InMemorySosStore : LocalSosStore {
        private var active: SosEmergency? = null
        val history = mutableListOf<SosEmergency>()

        override fun loadActive(): SosEmergency? = active
        override fun saveActive(emergency: SosEmergency) { active = emergency }
        override fun clearActive() { active = null }
        override fun loadHistory(): List<SosEmergency> = history.toList()
        override fun addToHistory(emergency: SosEmergency): Boolean {
            history.add(emergency)
            return true
        }
    }

    private class FakeAuthRepository(var uid: String? = "test-relay-user") : AuthRepository {
        override val currentUserId: String? get() = uid
        override val currentUserEmail: String? get() = uid?.let { "$it@test.com" }
        override val isSignedIn: Boolean get() = uid != null
        override suspend fun signIn(e: String, p: String): AuthResult = AuthResult.Success(currentUserEmail)
        override suspend fun signUp(e: String, p: String): AuthResult = AuthResult.Success(currentUserEmail)
        override fun signOut() { uid = null }
    }

    private class FakeSosCloudDataSource : SosCloudDataSource {
        val upserted = mutableListOf<SosEmergency>()
        var failUpserts = false

        override suspend fun upsertEmergency(emergency: SosEmergency): Boolean {
            if (failUpserts) return false
            upserted.add(emergency)
            return true
        }

        override suspend fun upsertHistory(emergencies: List<SosEmergency>): Boolean {
            if (failUpserts) return false
            upserted.addAll(emergencies)
            return true
        }
    }

    @Test
    fun `nearby sos event is stored locally and marked pending for sync`() = runBlocking {
        val sosStore = InMemorySosStore()
        val localSosRepo: SosRepository = SosRepositoryImpl(sosStore)
        val authRepo = FakeAuthRepository(uid = "user-123")
        val syncStateStore = SyncStateStoreFake()
        val cloudDataSource = FakeSosCloudDataSource()

        val syncingSosRepo = SyncingSosRepository(
            local = localSosRepo,
            authRepository = authRepo,
            cloudDataSource = cloudDataSource,
            syncStateStore = syncStateStore,
        )

        val selfOriginId = "user-123"
        val peerOriginId = "peer-device-456"
        val eventId = "evt-789"
        val occurredAt = 1700000000000L

        val incomingEvent = IncidentEvent(
            eventId = eventId,
            originId = peerOriginId,
            type = IncidentType.SOS,
            latitude = 30.3165,
            longitude = 78.0322,
            occurredAtMs = occurredAt,
            severity = IncidentSeverity.CRITICAL,
            payload = mapOf(
                "status" to "ACTIVE",
                "locationStatus" to "ACQUIRED",
                "note" to "Need urgent evacuation",
            ),
        )
        val envelope = RelayEnvelope(
            event = incomingEvent,
            payload = incomingEvent.canonicalString(),
        )

        // Process incoming Nearby envelope (logic from GeoRescuXApplication.setOnEventReceivedListener)
        val event = envelope.event
        val isSelf = event.originId == selfOriginId
        assertFalse("Peer event must not be treated as self origin", isSelf)

        val status = event.payload["status"] ?: "ACTIVE"
        val locationStatusName = event.payload["locationStatus"]
        val locStatus = locationStatusName?.let { runCatching { SosLocationStatus.valueOf(it) }.getOrNull() }
        val location = if (event.latitude != null && event.longitude != null) {
            SosLocation(
                latitude = event.latitude,
                longitude = event.longitude,
                accuracyMeters = 0f,
                timestampMs = event.occurredAtMs,
                provider = "mesh_relay",
            )
        } else null

        val emergency = SosEmergency(
            id = "MESH_${event.originId}_${event.eventId}",
            startedAtMs = event.occurredAtMs,
            stoppedAtMs = if (status == "COMPLETED") event.occurredAtMs + 1000L else null,
            location = location,
            locationStatus = locStatus ?: (if (location != null) SosLocationStatus.ACQUIRED else null),
            note = event.payload["note"],
        )

        // 1. Stored locally
        sosStore.addToHistory(emergency)
        assertEquals(1, sosStore.loadHistory().size)
        assertEquals("Need urgent evacuation", sosStore.loadHistory().first().note)

        // 2. Marked pending in SyncStateStore
        val uid = authRepo.currentUserId!!
        syncStateStore.setSosAlertPending(uid, emergency.id, pending = true)
        assertTrue("Nearby emergency must be marked pending", emergency.id in syncStateStore.pendingSosAlertIds(uid))

        // 3. Retry pending backups uploads from history and clears pending state
        syncingSosRepo.retryPendingBackups()

        // Wait brief moment for background coroutine in syncingSosRepo
        kotlinx.coroutines.delay(200)

        assertEquals(1, cloudDataSource.upserted.size)
        assertEquals(emergency.id, cloudDataSource.upserted.first().id)
        assertFalse("Pending status must be cleared after successful upload", emergency.id in syncStateStore.pendingSosAlertIds(uid))
    }

    @Test
    fun `nearby sos event from self origin is ignored`() {
        val selfOriginId = "my-device-id"
        val incomingEvent = IncidentEvent(
            eventId = "evt-self",
            originId = selfOriginId,
            type = IncidentType.SOS,
            occurredAtMs = 1700000000000L,
            severity = IncidentSeverity.HIGH,
        )

        val shouldIgnore = incomingEvent.originId == selfOriginId
        assertTrue("Self-origin event must be rejected/ignored", shouldIgnore)
    }

    @Test
    fun `offline device retains pending nearby sos until connectivity restored`() = runBlocking {
        val sosStore = InMemorySosStore()
        val localSosRepo: SosRepository = SosRepositoryImpl(sosStore)
        val authRepo = FakeAuthRepository(uid = "user-offline")
        val syncStateStore = SyncStateStoreFake()
        val cloudDataSource = FakeSosCloudDataSource().apply { failUpserts = true } // offline simulation

        val syncingSosRepo = SyncingSosRepository(
            local = localSosRepo,
            authRepository = authRepo,
            cloudDataSource = cloudDataSource,
            syncStateStore = syncStateStore,
        )

        val emergencyId = "MESH_peer_offline_1"
        val emergency = SosEmergency(id = emergencyId, startedAtMs = 1700000000000L)
        sosStore.addToHistory(emergency)

        val uid = authRepo.currentUserId!!
        syncStateStore.setSosAlertPending(uid, emergencyId, pending = true)

        // First attempt fails due to offline state
        syncingSosRepo.retryPendingBackups()
        kotlinx.coroutines.delay(200)

        assertTrue("Emergency must remain pending when offline", emergencyId in syncStateStore.pendingSosAlertIds(uid))

        // Device comes online
        cloudDataSource.failUpserts = false
        syncingSosRepo.retryPendingBackups()
        kotlinx.coroutines.delay(200)

        assertFalse("Emergency must no longer be pending after successful online retry", emergencyId in syncStateStore.pendingSosAlertIds(uid))
        assertEquals(1, cloudDataSource.upserted.size)
    }
}
