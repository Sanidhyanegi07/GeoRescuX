package com.example.georescux.data.sos

import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentMapper
import com.example.georescux.domain.incident.IncidentSeverity
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.relay.RelayDecision
import com.example.georescux.domain.relay.RelayEngine
import com.example.georescux.domain.relay.RelayEnvelope
import com.example.georescux.domain.relay.RelayTransport
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosHistoryPolicy
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

/**
 * Focused regression tests for SOS relay lifecycle and completion deduplication:
 * - Test A: ACTIVE event is accepted and forwarded.
 * - Test B: COMPLETED event for the same emergency is accepted and forwarded as a distinct lifecycle event.
 * - Test C: Repeated completion is rejected as a duplicate.
 * - Test D: Receiver associates ACTIVE and COMPLETED events with the same stable emergency ID.
 * - Test E: Independent emergencies do not collide.
 * - Test F: Duplicate ACTIVE events remain rejected.
 * - Test G: Completion-before-start and delayed ACTIVE-after-completion do not create an incorrect active emergency.
 * - Test H: New-format completion timestamps remain exact.
 * - Test I: Legacy completion events follow the documented compatibility fallback.
 * - Test J: Actual SharedPreferences history replacement works.
 */
class SosRelayCompletionTest {

    private class TestTransport : RelayTransport {
        val sent = mutableListOf<Pair<String, String>>()
        var peers = listOf("peer-1", "peer-2")

        override fun availablePeers(): List<String> = peers
        override fun send(peerId: String, payload: String): Boolean {
            sent.add(peerId to payload)
            return true
        }
    }

    private lateinit var transport: TestTransport
    private lateinit var relayEngine: RelayEngine
    private lateinit var fakePrefs: SharedPreferencesSosStoreTest.FakeSharedPreferences
    private lateinit var productionStore: SharedPreferencesSosStore
    private var fakeNowMs = 20_000L

    @Before
    fun setUp() {
        fakeNowMs = 20_000L
        transport = TestTransport()
        relayEngine = RelayEngine(selfOriginId = "self-device", transport = transport, nowMs = { fakeNowMs })
        fakePrefs = SharedPreferencesSosStoreTest.FakeSharedPreferences()
        productionStore = SharedPreferencesSosStore(fakePrefs)
    }

    @Test
    fun `Test A - SOS start event is accepted and relayed`() {
        fakeNowMs = 20_000L
        val emergency = SosEmergency(id = "sos-001", startedAtMs = 10_000L, stoppedAtMs = null)
        val startEvent = IncidentMapper.fromSos(emergency, originId = "self-device", pendingCloudBackup = true)

        assertEquals("sos-001", startEvent.eventId)
        assertEquals("sos-001", IncidentMapper.emergencyIdFor(startEvent))
        assertEquals(IncidentMapper.STATUS_ACTIVE, startEvent.payload["status"])
        assertEquals("10000", startEvent.payload["startedAtMs"])

        val decision = relayEngine.onLocalEvent(startEvent)
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, decision)
        assertEquals(2, transport.sent.size) // Sent to peer-1 and peer-2
    }

    @Test
    fun `Test B - SOS completion event is accepted and relayed without duplicate suppression`() {
        fakeNowMs = 20_000L
        val active = SosEmergency(id = "sos-002", startedAtMs = 10_000L, stoppedAtMs = null)
        val startEvent = IncidentMapper.fromSos(active, originId = "self-device", pendingCloudBackup = true)
        val startDecision = relayEngine.onLocalEvent(startEvent)
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, startDecision)

        transport.sent.clear()

        // Complete emergency and relay stop event
        fakeNowMs = 26_000L
        val completed = active.copy(stoppedAtMs = 25_000L)
        val stopEvent = IncidentMapper.fromSos(completed, originId = "self-device", pendingCloudBackup = false)

        assertEquals("sos-002:COMPLETED", stopEvent.eventId)
        assertEquals("sos-002", IncidentMapper.emergencyIdFor(stopEvent))
        assertEquals(IncidentMapper.STATUS_COMPLETED, stopEvent.payload["status"])
        assertEquals(25_000L, stopEvent.occurredAtMs)
        assertEquals("10000", stopEvent.payload["startedAtMs"])
        assertEquals("25000", stopEvent.payload["stoppedAtMs"])

        val stopDecision = relayEngine.onLocalEvent(stopEvent)
        assertEquals("Completion event must be accepted for forwarding, not dropped as DUPLICATE",
            RelayDecision.ACCEPTED_FORWARDED, stopDecision)
        assertEquals(2, transport.sent.size) // Successfully forwarded to peers
    }

    @Test
    fun `Test C - Duplicate completion event is rejected as duplicate`() {
        fakeNowMs = 20_000L
        val active = SosEmergency(id = "sos-003", startedAtMs = 10_000L)
        relayEngine.onLocalEvent(IncidentMapper.fromSos(active, originId = "peer-device", pendingCloudBackup = true))

        fakeNowMs = 31_000L
        val completed = active.copy(stoppedAtMs = 30_000L)
        val stopEvent = IncidentMapper.fromSos(completed, originId = "peer-device", pendingCloudBackup = false)

        val firstStop = relayEngine.onLocalEvent(stopEvent)
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, firstStop)

        // Retransmit same completion event
        val secondStop = relayEngine.onLocalEvent(stopEvent)
        assertEquals("Retransmitted completion event must be suppressed as DUPLICATE",
            RelayDecision.DUPLICATE, secondStop)

        // Peer retransmission
        val peerDuplicate = relayEngine.onEventReceived(RelayEnvelope(stopEvent, stopEvent.canonicalString(), fromPeerId = "peer-1"))
        assertEquals(RelayDecision.DUPLICATE, peerDuplicate)
    }

    @Test
    fun `Test D - Emergency correlation resolves original emergency on receiver`() {
        val peerOrigin = "victim-phone"

        // 1. Peer broadcasts start event
        val startEmergency = SosEmergency(id = "sos-004", startedAtMs = 12_000L, stoppedAtMs = null, note = "Trapped in building")
        val startEvent = IncidentMapper.fromSos(startEmergency, originId = peerOrigin, pendingCloudBackup = true)

        val processedActive = SosIncomingEventCoordinator.processIncomingSos(startEvent, productionStore)
        assertNotNull("Production coordinator must accept active SOS", processedActive)
        assertEquals("MESH_victim-phone_sos-004", processedActive!!.id)
        assertTrue("Processed emergency must be active", processedActive.isActive)
        assertEquals(12_000L, processedActive.startedAtMs)
        assertEquals("Trapped in building", processedActive.note)

        val historyAfterActive = productionStore.loadHistory()
        assertEquals(1, historyAfterActive.size)
        assertTrue(historyAfterActive[0].isActive)

        // 2. Peer broadcasts stop event
        val completedEmergency = startEmergency.copy(stoppedAtMs = 45_000L)
        val stopEvent = IncidentMapper.fromSos(completedEmergency, originId = peerOrigin, pendingCloudBackup = false)

        val processedCompleted = SosIncomingEventCoordinator.processIncomingSos(stopEvent, productionStore)
        assertNotNull("Production coordinator must correlate and process stop event", processedCompleted)
        assertEquals("MESH_victim-phone_sos-004", processedCompleted!!.id)
        assertFalse("Processed emergency must now be inactive/completed", processedCompleted.isActive)
        assertEquals(12_000L, processedCompleted.startedAtMs)
        assertEquals(45_000L, processedCompleted.stoppedAtMs)
        assertEquals("Trapped in building", processedCompleted.note)

        // Verification in actual production SharedPreferences store:
        val finalHistory = productionStore.loadHistory()
        assertEquals(1, finalHistory.size) // No duplicate records! Updated in-place
        assertEquals("MESH_victim-phone_sos-004", finalHistory[0].id)
        assertFalse(finalHistory[0].isActive)
        assertEquals(45_000L, finalHistory[0].stoppedAtMs)
    }

    @Test
    fun `Test E - Independent emergencies do not collide in event deduplication`() {
        fakeNowMs = 10_000L
        val emergency1 = SosEmergency(id = "sos-alpha", startedAtMs = 1_000L)
        val emergency2 = SosEmergency(id = "sos-beta", startedAtMs = 2_000L)

        val start1 = IncidentMapper.fromSos(emergency1, "dev-1", true)
        val start2 = IncidentMapper.fromSos(emergency2, "dev-2", true)

        assertEquals(RelayDecision.ACCEPTED_FORWARDED, relayEngine.onLocalEvent(start1))
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, relayEngine.onLocalEvent(start2))

        val stop1 = IncidentMapper.fromSos(emergency1.copy(stoppedAtMs = 5_000L), "dev-1", false)
        val stop2 = IncidentMapper.fromSos(emergency2.copy(stoppedAtMs = 6_000L), "dev-2", false)

        assertEquals(RelayDecision.ACCEPTED_FORWARDED, relayEngine.onLocalEvent(stop1))
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, relayEngine.onLocalEvent(stop2))
    }

    @Test
    fun `Test F - Existing duplicate protection rejects duplicate start events`() {
        fakeNowMs = 10_000L
        val emergency = SosEmergency(id = "sos-start-dup", startedAtMs = 5_000L)
        val startEvent = IncidentMapper.fromSos(emergency, "device-1", true)

        assertEquals(RelayDecision.ACCEPTED_FORWARDED, relayEngine.onLocalEvent(startEvent))
        assertEquals(RelayDecision.DUPLICATE, relayEngine.onLocalEvent(startEvent))

        val peerStart = RelayEnvelope(startEvent, startEvent.canonicalString(), fromPeerId = "peer-1")
        assertEquals(RelayDecision.DUPLICATE, relayEngine.onEventReceived(peerStart))
    }

    @Test
    fun `Test G - Event ordering prevents completion from creating active emergency and prevents downgrade`() {
        val peerOrigin = "origin-victim"

        // Case 1: Completion event arrives directly before start event (completion-before-start)
        val completed = SosEmergency(id = "sos-late", startedAtMs = 10_000L, stoppedAtMs = 18_000L, note = "Needs ambulance")
        val stopEvent = IncidentMapper.fromSos(completed, originId = peerOrigin, pendingCloudBackup = false)

        val processedDirectStop = SosIncomingEventCoordinator.processIncomingSos(stopEvent, productionStore)
        assertNotNull(processedDirectStop)
        assertEquals("MESH_origin-victim_sos-late", processedDirectStop!!.id)
        assertFalse("Direct completion event must not create active emergency", processedDirectStop.isActive)
        assertEquals(10_000L, processedDirectStop.startedAtMs)
        assertEquals(18_000L, processedDirectStop.stoppedAtMs)
        assertEquals("Needs ambulance", processedDirectStop.note)

        val storedCompleted = productionStore.loadHistory().firstOrNull { it.id == "MESH_origin-victim_sos-late" }
        assertNotNull(storedCompleted)
        assertFalse(storedCompleted!!.isActive)

        // Case 2: Out-of-order delayed start event arrives AFTER emergency was already completed locally
        val delayedStart = SosEmergency(id = "sos-late", startedAtMs = 10_000L, stoppedAtMs = null, note = "Needs ambulance")
        val startEvent = IncidentMapper.fromSos(delayedStart, originId = peerOrigin, pendingCloudBackup = true)

        val delayedResult = SosIncomingEventCoordinator.processIncomingSos(startEvent, productionStore)
        assertNull("Delayed start event must be rejected/ignored if emergency is already completed", delayedResult)

        // State in production store remains strictly completed
        val unchanged = productionStore.loadHistory().firstOrNull { it.id == "MESH_origin-victim_sos-late" }
        assertNotNull(unchanged)
        assertFalse("Emergency state must remain completed and not regress to active", unchanged!!.isActive)
        assertEquals(10_000L, unchanged.startedAtMs)
        assertEquals(18_000L, unchanged.stoppedAtMs)
        assertEquals(1, productionStore.loadHistory().size)
    }

    @Test
    fun `Test H - New-format completion timestamps remain exact with zero artificial distortion`() {
        val peerOrigin = "exact-peer"
        val emergency = SosEmergency(id = "sos-exact-1", startedAtMs = 10_000L, stoppedAtMs = 25_450L)
        val stopEvent = IncidentMapper.fromSos(emergency, originId = peerOrigin, pendingCloudBackup = false)

        assertEquals("10000", stopEvent.payload["startedAtMs"])
        assertEquals("25450", stopEvent.payload["stoppedAtMs"])
        assertEquals(25_450L, stopEvent.occurredAtMs)

        val processed = SosIncomingEventCoordinator.processIncomingSos(stopEvent, productionStore)
        assertNotNull(processed)
        assertEquals("Start timestamp must match transmitted start time exactly", 10_000L, processed!!.startedAtMs)
        assertEquals("Stop timestamp must match actual completion time exactly without artificial +1000ms offset",
            25_450L, processed.stoppedAtMs)
        assertEquals(25_450L - 10_000L, processed.stoppedAtMs!! - processed.startedAtMs)
    }

    @Test
    fun `Test I - Legacy completion events follow documented compatibility fallback`() {
        val peerOrigin = "legacy-peer"
        // In legacy versions, occurredAtMs was emergency.startedAtMs and no startedAtMs/stoppedAtMs were in payload
        val legacyEvent = IncidentEvent(
            eventId = "legacy-sos-100:COMPLETED",
            originId = peerOrigin,
            type = IncidentType.SOS,
            occurredAtMs = 15_000L, // Legacy occurredAtMs was start time
            severity = IncidentSeverity.CRITICAL,
            payload = mapOf(
                "status" to "COMPLETED",
                "emergencyId" to "legacy-sos-100",
                "note" to "Legacy flood rescue",
            ),
        )

        assertTrue("Event must be identified as legacy completion", SosIncomingEventCoordinator.isLegacyCompletion(legacyEvent))

        val processed = SosIncomingEventCoordinator.processIncomingSos(legacyEvent, productionStore)
        assertNotNull(processed)
        assertEquals("MESH_legacy-peer_legacy-sos-100", processed!!.id)
        assertFalse(processed.isActive)
        assertEquals("Legacy start time falls back to occurredAtMs", 15_000L, processed.startedAtMs)
        assertEquals("Legacy fallback adds +1000ms offset to maintain positive duration for legacy timestamps",
            16_000L, processed.stoppedAtMs)
    }

    @Test
    fun `Test J - Actual SharedPreferences history replacement works`() {
        val emergencyId = "MESH_dev_sos-test-j"

        // 1. Adding an emergency with a new ID adds it to history
        val active = SosEmergency(id = emergencyId, startedAtMs = 10_000L, stoppedAtMs = null, note = "Trapped on roof")
        productionStore.addToHistory(active)

        var history = productionStore.loadHistory()
        assertEquals(1, history.size)
        assertEquals(emergencyId, history[0].id)
        assertTrue(history[0].isActive)

        // 2. Adding an updated emergency with the same ID replaces the old entry
        val completed = active.copy(stoppedAtMs = 28_000L, note = "Rescued by boat")
        productionStore.addToHistory(completed)

        history = productionStore.loadHistory()
        // 3. The history contains only one entry for that ID (no duplicate)
        assertEquals("History must contain exactly 1 entry for the same emergency ID", 1, history.size)
        assertEquals(emergencyId, history[0].id)
        // 4. The updated completion state and timestamps are preserved
        assertFalse(history[0].isActive)
        assertEquals(10_000L, history[0].startedAtMs)
        assertEquals(28_000L, history[0].stoppedAtMs)
        assertEquals("Rescued by boat", history[0].note)

        // 5. The maximum-history retention policy still works
        for (i in 1..60) {
            productionStore.addToHistory(SosEmergency(id = "hist-$i", startedAtMs = i * 1000L))
        }
        val cappedHistory = productionStore.loadHistory()
        assertEquals(SosHistoryPolicy.MAX_HISTORY, cappedHistory.size)
        assertEquals(50, cappedHistory.size)

        // 6. Existing active emergency preservation behavior remains correct
        val activeStandalone = SosEmergency(id = "standalone-active", startedAtMs = 999_000L)
        productionStore.saveActive(activeStandalone)
        assertEquals("standalone-active", productionStore.loadActive()?.id)
        productionStore.addToHistory(SosEmergency(id = "another-hist", startedAtMs = 70_000L))
        assertEquals("standalone-active", productionStore.loadActive()?.id)
    }
}
