package com.example.georescux.data.relay

import com.example.georescux.data.sos.LocalSosStore
import com.example.georescux.data.sos.SharedPreferencesSosStore
import com.example.georescux.data.sos.SharedPreferencesSosStoreTest.FakeSharedPreferences
import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentSeverity
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.relay.RelayEnvelope
import com.example.georescux.domain.sos.SosEmergency
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.Collections

@OptIn(ExperimentalCoroutinesApi::class)
class NearbySosIngestionTest {

    private class FailingSosStore : LocalSosStore {
        override fun loadActive(): SosEmergency? = null
        override fun saveActive(emergency: SosEmergency) {}
        override fun clearActive() {}
        override fun loadHistory(): List<SosEmergency> = emptyList()
        override fun addToHistory(emergency: SosEmergency): Boolean = false
        override fun replaceInHistory(replacedId: String, emergency: SosEmergency): Boolean = false
    }

    private fun createSosEnvelope(
        eventId: String,
        originId: String,
        status: String = "ACTIVE",
        startedAtMs: Long = 10_000L,
        stoppedAtMs: Long? = null,
        sequence: Long = 1L,
        note: String? = "Trapped on roof",
    ): RelayEnvelope {
        val payload = mutableMapOf(
            "emergencyId" to eventId,
            "status" to status,
            "startedAtMs" to startedAtMs.toString(),
            "sequence" to sequence.toString(),
        )
        if (stoppedAtMs != null) {
            payload["stoppedAtMs"] = stoppedAtMs.toString()
        }
        if (note != null) {
            payload["note"] = note
        }

        val event = IncidentEvent(
            eventId = eventId,
            originId = originId,
            type = IncidentType.SOS,
            latitude = 30.3165,
            longitude = 78.0322,
            occurredAtMs = stoppedAtMs ?: startedAtMs,
            severity = IncidentSeverity.CRITICAL,
            sequence = sequence,
            payload = payload,
        )
        return RelayEnvelope(
            event = event,
            payload = event.canonicalString(),
        )
    }

    @Test
    fun `1 Nearby SOS is processed off the main thread and succeeds against store rejecting main thread`() = runTest {
        val fakePrefs = FakeSharedPreferences()
        // Thread provider simulates that the test runner thread is the "main" thread,
        // so calling store methods on the caller thread would be rejected.
        val mainThreadName = Thread.currentThread().name
        val store = SharedPreferencesSosStore(
            prefs = fakePrefs,
            isMainThreadProvider = { Thread.currentThread().name == mainThreadName },
        )

        var processedThreadName: String? = null
        val notifications = mutableListOf<Triple<String, Long, String?>>()
        val pendingSyncs = mutableListOf<String>()

        // Use a background dispatcher (IO with limitedParallelism(1))
        val backgroundDispatcher = Dispatchers.IO.limitedParallelism(1)
        val backgroundScope = CoroutineScope(SupervisorJob() + backgroundDispatcher)

        val handler = NearbySosIngestionHandler(
            sosStore = object : LocalSosStore by store {
                override fun addToHistory(emergency: SosEmergency): Boolean {
                    processedThreadName = Thread.currentThread().name
                    return store.addToHistory(emergency)
                }
            },
            selfOriginId = { "self-device-123" },
            onNotifyAlert = { originId, startedAtMs, note ->
                notifications.add(Triple(originId, startedAtMs, note))
            },
            onMarkPendingSync = { emergencyId ->
                pendingSyncs.add(emergencyId)
            },
            scope = backgroundScope,
            dispatcher = backgroundDispatcher,
        )

        val envelope = createSosEnvelope(eventId = "sos-peer-1", originId = "peer-device-456")
        val job = handler.handleEnvelope(envelope)
        assertNotNull("SOS envelope must produce an ingestion job", job)
        job!!.join()

        // Verify execution thread was NOT main thread
        assertNotNull("Ingestion must have executed", processedThreadName)
        assertTrue("Must execute on a background thread", processedThreadName != mainThreadName)

        // Verify persistence succeeded because it ran on the background thread
        val history = store.loadHistory()
        assertEquals(1, history.size)
        assertEquals("MESH_peer-device-456_sos-peer-1", history[0].id)
        assertEquals("Trapped on roof", history[0].note)

        // Verify notification and pending sync paths reached
        assertEquals(1, notifications.size)
        assertEquals("peer-device-456", notifications[0].first)
        assertEquals(10_000L, notifications[0].second)
        assertEquals("Trapped on roof", notifications[0].third)

        assertEquals(1, pendingSyncs.size)
        assertEquals("MESH_peer-device-456_sos-peer-1", pendingSyncs[0])
    }

    @Test
    fun `2 Valid incoming Nearby SOS is persisted and reaches notification path`() = runTest {
        val testDispatcher = StandardTestDispatcher(testScheduler)
        val testScope = TestScope(testDispatcher)
        val fakePrefs = FakeSharedPreferences()
        val store = SharedPreferencesSosStore(fakePrefs)

        val notifications = mutableListOf<Triple<String, Long, String?>>()
        val pendingSyncs = mutableListOf<String>()

        val handler = NearbySosIngestionHandler(
            sosStore = store,
            selfOriginId = { "self-device-123" },
            onNotifyAlert = { originId, startedAtMs, note ->
                notifications.add(Triple(originId, startedAtMs, note))
            },
            onMarkPendingSync = { emergencyId ->
                pendingSyncs.add(emergencyId)
            },
            scope = testScope,
            dispatcher = testDispatcher,
        )

        val envelope = createSosEnvelope(
            eventId = "alert-urgent-1",
            originId = "responder-99",
            note = "Evacuation required immediately",
        )

        val job = handler.handleEnvelope(envelope)
        assertNotNull(job)
        testScheduler.advanceUntilIdle()

        // 1. Persisted in store
        val history = store.loadHistory()
        assertEquals(1, history.size)
        assertEquals("MESH_responder-99_alert-urgent-1", history[0].id)
        assertEquals("Evacuation required immediately", history[0].note)
        assertTrue(history[0].isActive)

        // 2. Notification sent
        assertEquals(1, notifications.size)
        assertEquals("responder-99", notifications[0].first)
        assertEquals(10_000L, notifications[0].second)
        assertEquals("Evacuation required immediately", notifications[0].third)

        // 3. Marked pending sync
        assertEquals(1, pendingSyncs.size)
        assertEquals("MESH_responder-99_alert-urgent-1", pendingSyncs[0])
    }

    @Test
    fun `3 Failed persistence does not produce false accepted-event notification`() = runTest {
        val testDispatcher = StandardTestDispatcher(testScheduler)
        val testScope = TestScope(testDispatcher)
        val failingStore = FailingSosStore()

        val notifications = mutableListOf<Triple<String, Long, String?>>()
        val pendingSyncs = mutableListOf<String>()

        val handler = NearbySosIngestionHandler(
            sosStore = failingStore,
            selfOriginId = { "self-device-123" },
            onNotifyAlert = { originId, startedAtMs, note ->
                notifications.add(Triple(originId, startedAtMs, note))
            },
            onMarkPendingSync = { emergencyId ->
                pendingSyncs.add(emergencyId)
            },
            scope = testScope,
            dispatcher = testDispatcher,
        )

        val envelope = createSosEnvelope(eventId = "sos-fail-1", originId = "peer-fail-77")
        val job = handler.handleEnvelope(envelope)
        assertNotNull(job)
        testScheduler.advanceUntilIdle()

        // Crucial requirement: failed persistence must NEVER produce a notification or pending sync
        assertTrue("No notification must be posted when persistence fails", notifications.isEmpty())
        assertTrue("No sync must be marked when persistence fails", pendingSyncs.isEmpty())
    }

    @Test
    fun `4 Multiple incoming lifecycle events are processed in strict sequential order`() = runTest {
        val testDispatcher = StandardTestDispatcher(testScheduler)
        val testScope = TestScope(testDispatcher)
        val fakePrefs = FakeSharedPreferences()
        val store = SharedPreferencesSosStore(fakePrefs)

        val notifications = mutableListOf<Triple<String, Long, String?>>()
        val pendingSyncs = mutableListOf<String>()

        val handler = NearbySosIngestionHandler(
            sosStore = store,
            selfOriginId = { "self-device-123" },
            onNotifyAlert = { originId, startedAtMs, note ->
                notifications.add(Triple(originId, startedAtMs, note))
            },
            onMarkPendingSync = { emergencyId ->
                pendingSyncs.add(emergencyId)
            },
            scope = testScope,
            dispatcher = testDispatcher,
        )

        // Event 1: ACTIVE (sequence 1)
        val activeEnvelope = createSosEnvelope(
            eventId = "sos-multi-1",
            originId = "peer-multi-99",
            status = "ACTIVE",
            startedAtMs = 10_000L,
            sequence = 1L,
            note = "Need boat",
        )

        // Event 2: COMPLETED (sequence 2)
        val completedEnvelope = createSosEnvelope(
            eventId = "sos-multi-1",
            originId = "peer-multi-99",
            status = "COMPLETED",
            startedAtMs = 10_000L,
            stoppedAtMs = 25_000L,
            sequence = 2L,
            note = "Rescued by boat",
        )

        // Submit both in rapid succession
        val job1 = handler.handleEnvelope(activeEnvelope)
        val job2 = handler.handleEnvelope(completedEnvelope)

        assertNotNull(job1)
        assertNotNull(job2)
        testScheduler.advanceUntilIdle()

        // Final state in history must be the completed record with sequence 2
        val history = store.loadHistory()
        assertEquals(1, history.size)
        val record = history[0]
        assertEquals("MESH_peer-multi-99_sos-multi-1", record.id)
        assertEquals(2L, record.sequence)
        assertEquals(10_000L, record.startedAtMs)
        assertEquals(25_000L, record.stoppedAtMs)
        assertFalse("Must be marked completed", record.isActive)
        assertEquals("Rescued by boat", record.note)

        // Only the initial ACTIVE event triggers heads-up notification (COMPLETED does not notify)
        assertEquals(1, notifications.size)
        assertEquals("peer-multi-99", notifications[0].first)
    }

    @Test
    fun `5 Non-SOS events and own echoes return null without ingestion`() = runTest {
        val testDispatcher = StandardTestDispatcher(testScheduler)
        val testScope = TestScope(testDispatcher)
        val fakePrefs = FakeSharedPreferences()
        val store = SharedPreferencesSosStore(fakePrefs)

        val notifications = mutableListOf<Triple<String, Long, String?>>()
        val handler = NearbySosIngestionHandler(
            sosStore = store,
            selfOriginId = { "self-device-123" },
            onNotifyAlert = { originId, startedAtMs, note -> notifications.add(Triple(originId, startedAtMs, note)) },
            onMarkPendingSync = {},
            scope = testScope,
            dispatcher = testDispatcher,
        )

        // Case A: Hazard event (non-SOS)
        val hazardEvent = IncidentEvent(
            eventId = "haz-1",
            originId = "peer-1",
            type = IncidentType.HAZARD,
            latitude = 30.0,
            longitude = 78.0,
            occurredAtMs = 1000L,
        )
        val hazardEnvelope = RelayEnvelope(event = hazardEvent, payload = hazardEvent.canonicalString())
        val jobHazard = handler.handleEnvelope(hazardEnvelope)
        assertNull("Non-SOS event must return null job immediately", jobHazard)

        // Case B: Own-device SOS echo
        val ownSosEnvelope = createSosEnvelope(eventId = "own-sos-1", originId = "self-device-123")
        val jobOwn = handler.handleEnvelope(ownSosEnvelope)
        assertNotNull(jobOwn)
        testScheduler.advanceUntilIdle()

        // Coordinator drops own echo
        assertTrue("Own echo must not be persisted in history", store.loadHistory().isEmpty())
        assertTrue("Own echo must not produce notification", notifications.isEmpty())
    }
}
