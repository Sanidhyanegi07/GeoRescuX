package com.example.georescux.data.ble

import com.example.georescux.data.sos.LocalSosStore
import com.example.georescux.data.sos.SharedPreferencesSosStore
import com.example.georescux.data.sos.SharedPreferencesSosStoreTest
import com.example.georescux.data.sos.SosEmergencyJson
import com.example.georescux.data.sos.SosIncomingEventCoordinator
import com.example.georescux.data.sos.SosRepositoryImpl
import com.example.georescux.domain.ble.GeoRescueBlePacket
import com.example.georescux.domain.ble.GeoRescueBlePacketType
import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentMapper
import com.example.georescux.domain.incident.IncidentSeverity
import com.example.georescux.domain.incident.IncidentSyncState
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

/**
 * Comprehensive P0 regression test suite for SOS Lifecycle and Sequence Integrity.
 *
 * Verifies all 14 required P0 scenarios and explicit edge-case regressions:
 * - Test 1: Stable originEmergencyId across full BLE lifecycle.
 * - Test 2: Unique packetId for every transmission.
 * - Test 3: Exact start timestamp preservation.
 * - Test 4: Exact real completion timestamp preservation without +1000L.
 * - Test 5: Legacy completion fallback (adds +1000L when stoppedAtMs is missing).
 * - Test 6: Duplicate ACTIVE event idempotency.
 * - Test 7: Duplicate COMPLETED event idempotency.
 * - Test 8: Stale active-to-active sequence rejection (seq 1 after seq 2).
 * - Test 9: Completion-before-active behavior.
 * - Test 10: Delayed ACTIVE after COMPLETED rejection (terminal protection).
 * - Test 11: Sequence persistence and restart recovery in SosRepositoryImpl.
 * - Test 12: BLE and Nearby canonical incident identity equivalence (computeMeshId).
 * - Test 13: Single history row after full incident lifecycle.
 * - Test 14: Legacy sequence-zero cannot overwrite newer sequenced state.
 * - Regression 15: Equal sequence with conflicting content rejection.
 * - Regression 16: Sequenced ACTIVE cannot resurrect legacy COMPLETED.
 * - Regression 17: Legacy COMPLETED cannot cancel sequenced ACTIVE.
 * - Regression 18: Concurrent mutations do not allocate duplicate sequences in SosRepositoryImpl.
 */
class BleSosLifecycleTest {

    private lateinit var fakePrefs: SharedPreferencesSosStoreTest.FakeSharedPreferences
    private lateinit var store: LocalSosStore
    private lateinit var bridge: BleSosBridge

    private val originId = "node-alpha"
    private val emergencyId = "alert-001"

    @Before
    fun setUp() {
        fakePrefs = SharedPreferencesSosStoreTest.FakeSharedPreferences()
        store = SharedPreferencesSosStore(fakePrefs)
        bridge = BleSosBridge(manager = null, originDeviceId = originId)
    }

    @Test
    fun `Test 1 - Stable originEmergencyId across full BLE lifecycle`() {
        val activeEmergency = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            stoppedAtMs = null,
            sequence = 1L,
        )
        val activePacket = bridge.buildSosPacket(activeEmergency, emergencyId)
        assertEquals(emergencyId, activePacket.originEmergencyId)

        val completedEmergency = activeEmergency.copy(
            stoppedAtMs = 25_000L,
            sequence = 2L,
        )
        val completedPacket = bridge.buildSosPacket(completedEmergency, emergencyId)
        assertEquals(emergencyId, completedPacket.originEmergencyId)

        // Convert both to IncidentEvents and verify emergencyId extraction and mesh ID
        val activeEvent = bridge.toIncidentEvent(activePacket)
        assertNotNull(activeEvent)
        assertEquals(emergencyId, IncidentMapper.emergencyIdFor(activeEvent!!))

        val completedEvent = bridge.toIncidentEvent(completedPacket)
        assertNotNull(completedEvent)
        assertEquals(emergencyId, IncidentMapper.emergencyIdFor(completedEvent!!))

        val meshIdActive = SosIncomingEventCoordinator.computeMeshId(activeEvent.originId, emergencyId)
        val meshIdCompleted = SosIncomingEventCoordinator.computeMeshId(completedEvent.originId, emergencyId)
        assertEquals("MESH_${originId}_$emergencyId", meshIdActive)
        assertEquals(meshIdActive, meshIdCompleted)
    }

    @Test
    fun `Test 2 - Unique packetId for every transmission`() {
        val emergency = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            sequence = 1L,
        )
        val packet1 = bridge.buildSosPacket(emergency, emergencyId)
        val packet2 = bridge.buildSosPacket(emergency, emergencyId)
        val packet3 = bridge.buildSosPacket(emergency.copy(stoppedAtMs = 25_000L, sequence = 2L), emergencyId)

        assertNotNull(packet1.packetId)
        assertNotNull(packet2.packetId)
        assertNotNull(packet3.packetId)

        assertTrue(packet1.packetId.isNotBlank())
        assertNotEquals(packet1.packetId, packet2.packetId)
        assertNotEquals(packet2.packetId, packet3.packetId)
        assertNotEquals(packet1.packetId, packet3.packetId)
    }

    @Test
    fun `Test 3 - Exact start timestamp preservation`() {
        val completedEmergency = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            stoppedAtMs = 25_000L,
            sequence = 2L,
        )
        val packet = bridge.buildSosPacket(completedEmergency, emergencyId)
        val event = bridge.toIncidentEvent(packet)
        assertNotNull(event)

        // Receiving side with no prior active record
        val reconciled = SosIncomingEventCoordinator.correlate(event!!, existing = null)
        assertNotNull(reconciled)
        assertEquals(10_000L, reconciled!!.startedAtMs)
        assertEquals(25_000L, reconciled.stoppedAtMs)

        // Receiving side with existing active record
        val existingActive = SosEmergency(
            id = SosIncomingEventCoordinator.computeMeshId(originId, emergencyId),
            startedAtMs = 10_000L,
            sequence = 1L,
        )
        val reconciledWithExisting = SosIncomingEventCoordinator.correlate(event, existing = existingActive)
        assertNotNull(reconciledWithExisting)
        assertEquals(10_000L, reconciledWithExisting!!.startedAtMs)
    }

    @Test
    fun `Test 4 - Exact real completion timestamp preservation without +1000L`() {
        val completedEmergency = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            stoppedAtMs = 25_000L,
            sequence = 2L,
        )
        val packet = bridge.buildSosPacket(completedEmergency, emergencyId)
        val event = bridge.toIncidentEvent(packet)
        assertNotNull(event)

        assertFalse("New-format completion must not be marked as legacy", SosIncomingEventCoordinator.isLegacyCompletion(event!!))
        assertEquals("25000", event.payload["stoppedAtMs"])
        assertEquals("10000", event.payload["startedAtMs"])

        val reconciled = SosIncomingEventCoordinator.correlate(event, existing = null)
        assertNotNull(reconciled)
        // Must be exactly 25_000L, never 26_000L
        assertEquals(25_000L, reconciled!!.stoppedAtMs)
    }

    @Test
    fun `Test 5 - Legacy completion fallback adds +1000L when stoppedAtMs is missing`() {
        val legacyPacket = GeoRescueBlePacket(
            packetId = "leg-001",
            originDeviceId = originId,
            originEmergencyId = null,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            latitude = 12.34,
            longitude = 56.78,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = emptyMap(),
        )

        val event = bridge.toIncidentEvent(legacyPacket)
        assertNotNull(event)
        assertTrue("Packet without startedAtMs/stoppedAtMs is legacy", SosIncomingEventCoordinator.isLegacyCompletion(event!!))

        val reconciled = SosIncomingEventCoordinator.correlate(event, existing = null)
        assertNotNull(reconciled)
        assertEquals(10_000L, reconciled!!.startedAtMs)
        // Legacy fallback adds +1000L
        assertEquals(11_000L, reconciled.stoppedAtMs)
    }

    @Test
    fun `Test 6 - Duplicate ACTIVE event idempotency`() {
        val active = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = 1L)
        val packet = bridge.buildSosPacket(active, emergencyId)
        val event = bridge.toIncidentEvent(packet)!!

        val first = SosIncomingEventCoordinator.processIncomingSos(event, store)
        assertNotNull(first)
        assertEquals(1, store.loadHistory().size)

        // Exact same event again
        val duplicate = SosIncomingEventCoordinator.processIncomingSos(event, store)
        assertNull("Duplicate active event must be ignored", duplicate)
        assertEquals(1, store.loadHistory().size)
    }

    @Test
    fun `Test 7 - Duplicate COMPLETED event idempotency`() {
        val completed = SosEmergency(id = emergencyId, startedAtMs = 10_000L, stoppedAtMs = 25_000L, sequence = 2L)
        val packet = bridge.buildSosPacket(completed, emergencyId)
        val event = bridge.toIncidentEvent(packet)!!

        val first = SosIncomingEventCoordinator.processIncomingSos(event, store)
        assertNotNull(first)
        assertEquals(1, store.loadHistory().size)

        // Duplicate completion event
        val duplicate = SosIncomingEventCoordinator.processIncomingSos(event, store)
        assertNull("Duplicate completion event must be ignored", duplicate)
        assertEquals(1, store.loadHistory().size)
    }

    @Test
    fun `Test 8 - Stale active-to-active sequence rejection`() {
        val meshId = SosIncomingEventCoordinator.computeMeshId(originId, emergencyId)
        val current = SosEmergency(
            id = meshId,
            startedAtMs = 10_000L,
            location = SosLocation(20.0, 30.0, 5f, 12_000L, "ble_relay"),
            sequence = 2L,
        )
        store.saveActive(current)
        store.addToHistory(current)

        // Stale event with sequence 1
        val staleEmergency = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            location = SosLocation(10.0, 15.0, 5f, 10_000L, "ble_relay"),
            sequence = 1L,
        )
        val stalePacket = bridge.buildSosPacket(staleEmergency, emergencyId)
        val staleEvent = bridge.toIncidentEvent(stalePacket)!!

        val result = SosIncomingEventCoordinator.processIncomingSos(staleEvent, store)
        assertNull("Stale sequence 1 must be rejected when current sequence is 2", result)

        val stored = store.loadHistory().first { it.id == meshId }
        assertEquals(2L, stored.sequence)
        assertEquals(20.0, stored.location?.latitude ?: 0.0, 0.0001)
    }

    @Test
    fun `Test 9 - Completion-before-active behavior`() {
        // No prior active record in store
        val completed = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            stoppedAtMs = 25_000L,
            sequence = 2L,
        )
        val packet = bridge.buildSosPacket(completed, emergencyId)
        val event = bridge.toIncidentEvent(packet)!!

        val result = SosIncomingEventCoordinator.processIncomingSos(event, store)
        assertNotNull(result)
        assertFalse(result!!.isActive)
        assertEquals(10_000L, result.startedAtMs)
        assertEquals(25_000L, result.stoppedAtMs)
        assertEquals(2L, result.sequence)

        val history = store.loadHistory()
        assertEquals(1, history.size)
        assertEquals(result.id, history[0].id)
        assertFalse(history[0].isActive)
    }

    @Test
    fun `Test 10 - Delayed ACTIVE after COMPLETED rejection`() {
        val completed = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            stoppedAtMs = 25_000L,
            sequence = 2L,
        )
        val completedPacket = bridge.buildSosPacket(completed, emergencyId)
        val completedEvent = bridge.toIncidentEvent(completedPacket)!!
        SosIncomingEventCoordinator.processIncomingSos(completedEvent, store)

        // 1. Delayed active with sequence 1
        val delayedActive1 = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = 1L)
        val delayedPacket1 = bridge.buildSosPacket(delayedActive1, emergencyId)
        val delayedEvent1 = bridge.toIncidentEvent(delayedPacket1)!!
        val result1 = SosIncomingEventCoordinator.processIncomingSos(delayedEvent1, store)
        assertNull("Delayed active (seq 1) must be rejected after completion", result1)

        // 2. Delayed active even with sequence 3
        val delayedActive3 = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = 3L)
        val delayedPacket3 = bridge.buildSosPacket(delayedActive3, emergencyId)
        val delayedEvent3 = bridge.toIncidentEvent(delayedPacket3)!!
        val result3 = SosIncomingEventCoordinator.processIncomingSos(delayedEvent3, store)
        assertNull("Delayed active (seq 3) must be rejected because emergency is already terminal", result3)

        // 3. Delayed legacy active (seq 0)
        val legacyActivePacket = GeoRescueBlePacket(
            packetId = "leg-act-1",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            latitude = 1.0,
            longitude = 2.0,
            status = BleSosBridge.SOS_STATUS_ACTIVE,
            payload = emptyMap(),
        )
        val legacyActiveEvent = bridge.toIncidentEvent(legacyActivePacket)!!
        val resultLegacy = SosIncomingEventCoordinator.processIncomingSos(legacyActiveEvent, store)
        assertNull("Legacy active must be rejected after completion", resultLegacy)

        // Terminal emergency remains unchanged
        val stored = store.loadHistory().first()
        assertFalse(stored.isActive)
        assertEquals(25_000L, stored.stoppedAtMs)
    }

    @Test
    fun `Test 11 - Sequence persistence and restart recovery in SosRepositoryImpl`() {
        val repo1 = SosRepositoryImpl(store)
        val active1 = repo1.startEmergency("sos-local-1", 10_000L)
        assertNotNull(active1)
        assertEquals(1L, active1!!.sequence)

        repo1.updateActiveEmergencyLocation(
            SosLocation(10.0, 20.0, 5f, 11_000L, "fused"),
            SosLocationStatus.ACQUIRED
        )
        assertEquals(2L, store.loadActive()?.sequence)

        repo1.updateActiveEmergencyNote("Injured climber")
        assertEquals(3L, store.loadActive()?.sequence)
        assertEquals("Injured climber", store.loadActive()?.note)

        // Simulate app process restart: create a new repository instance pointing to the same persisted store
        val repo2 = SosRepositoryImpl(store)
        val recoveredActive = repo2.getActiveEmergency()
        assertNotNull(recoveredActive)
        assertEquals(3L, recoveredActive!!.sequence)
        assertEquals("Injured climber", recoveredActive.note)

        // Complete emergency in repo2
        val completed = repo2.completeEmergency(25_000L)
        assertNotNull(completed)
        assertEquals(4L, completed!!.sequence)
        assertEquals(25_000L, completed.stoppedAtMs)

        // Active must be cleared, history must contain the completed record with sequence 4
        assertNull(repo2.getActiveEmergency())
        val history = store.loadHistory()
        assertEquals(1, history.size)
        assertEquals(4L, history[0].sequence)
        assertEquals(25_000L, history[0].stoppedAtMs)
    }

    @Test
    fun `Test 12 - BLE and Nearby canonical incident identity equivalence`() {
        val emergency = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = 1L)

        // BLE pathway
        val blePacket = bridge.buildSosPacket(emergency, emergencyId)
        val bleEvent = bridge.toIncidentEvent(blePacket)!!
        val bleCanonicalId = SosIncomingEventCoordinator.computeMeshId(
            bleEvent.originId,
            IncidentMapper.emergencyIdFor(bleEvent)
        )

        // Nearby pathway
        val nearbyEvent = IncidentMapper.fromSos(emergency, originId = originId, pendingCloudBackup = false)
        val nearbyCanonicalId = SosIncomingEventCoordinator.computeMeshId(
            nearbyEvent.originId,
            IncidentMapper.emergencyIdFor(nearbyEvent)
        )

        assertEquals("MESH_${originId}_$emergencyId", bleCanonicalId)
        assertEquals("MESH_${originId}_$emergencyId", nearbyCanonicalId)
        assertEquals(bleCanonicalId, nearbyCanonicalId)

        // Both pathways update the same history row without duplicating
        SosIncomingEventCoordinator.processIncomingSos(bleEvent, store)
        assertEquals(1, store.loadHistory().size)

        // Nearby completion for the same emergency
        val completed = emergency.copy(stoppedAtMs = 25_000L, sequence = 2L)
        val nearbyCompletionEvent = IncidentMapper.fromSos(completed, originId = originId, pendingCloudBackup = false)
        SosIncomingEventCoordinator.processIncomingSos(nearbyCompletionEvent, store)

        val history = store.loadHistory()
        assertEquals(1, history.size)
        assertEquals("MESH_${originId}_$emergencyId", history[0].id)
        assertFalse(history[0].isActive)
        assertEquals(25_000L, history[0].stoppedAtMs)
        assertEquals(2L, history[0].sequence)
    }

    @Test
    fun `Test 13 - Single history row after full incident lifecycle`() {
        // Step 1: Ingest start event (seq 1)
        val e1 = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = 1L)
        val ev1 = bridge.toIncidentEvent(bridge.buildSosPacket(e1, emergencyId))!!
        SosIncomingEventCoordinator.processIncomingSos(ev1, store)

        // Step 2: Ingest location update (seq 2)
        val e2 = e1.copy(
            location = SosLocation(30.0, 40.0, 5f, 12_000L, "gps"),
            locationStatus = SosLocationStatus.ACQUIRED,
            sequence = 2L,
        )
        val ev2 = bridge.toIncidentEvent(bridge.buildSosPacket(e2, emergencyId))!!
        SosIncomingEventCoordinator.processIncomingSos(ev2, store)

        // Step 3: Ingest note update (seq 3)
        val e3 = e2.copy(note = "Water rising", sequence = 3L)
        val ev3 = bridge.toIncidentEvent(bridge.buildSosPacket(e3, emergencyId))!!
        SosIncomingEventCoordinator.processIncomingSos(ev3, store)

        // Step 4: Ingest completion (seq 4)
        val e4 = e3.copy(stoppedAtMs = 30_000L, sequence = 4L)
        val ev4 = bridge.toIncidentEvent(bridge.buildSosPacket(e4, emergencyId))!!
        SosIncomingEventCoordinator.processIncomingSos(ev4, store)

        val history = store.loadHistory()
        assertEquals(1, history.size)
        val row = history[0]
        assertEquals("MESH_${originId}_$emergencyId", row.id)
        assertEquals(10_000L, row.startedAtMs)
        assertEquals(30_000L, row.stoppedAtMs)
        assertEquals(4L, row.sequence)
        assertEquals("Water rising", row.note)
        assertEquals(30.0, row.location?.latitude ?: 0.0, 0.0001)
        assertFalse(row.isActive)
    }

    @Test
    fun `Test 14 - Legacy sequence-zero cannot overwrite newer sequenced state`() {
        // Sequenced active incident at sequence 2
        val sequencedActive = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            location = SosLocation(10.0, 20.0, 5f, 12_000L, "gps"),
            sequence = 2L,
        )
        val evSequenced = bridge.toIncidentEvent(bridge.buildSosPacket(sequencedActive, emergencyId))!!
        SosIncomingEventCoordinator.processIncomingSos(evSequenced, store)

        // Legacy active event (sequence 0) arrives
        val legacyActivePacket = GeoRescueBlePacket(
            packetId = "leg-act-0",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            latitude = 99.0,
            longitude = 99.0,
            status = BleSosBridge.SOS_STATUS_ACTIVE,
            payload = emptyMap(), // no sequence -> defaults to 0
        )
        val legacyActiveEvent = bridge.toIncidentEvent(legacyActivePacket)!!
        val resultActive = SosIncomingEventCoordinator.processIncomingSos(legacyActiveEvent, store)
        assertNull("Legacy active event (seq 0) must not overwrite sequenced incident (seq 2)", resultActive)

        // Legacy completion event (sequence 0) arrives
        val legacyCompletedPacket = GeoRescueBlePacket(
            packetId = "leg-comp-0",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            latitude = 99.0,
            longitude = 99.0,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = emptyMap(), // no sequence -> defaults to 0
        )
        val legacyCompletedEvent = bridge.toIncidentEvent(legacyCompletedPacket)!!
        val resultCompleted = SosIncomingEventCoordinator.processIncomingSos(legacyCompletedEvent, store)
        assertNull("Legacy completion event (seq 0) must not cancel sequenced incident (seq 2)", resultCompleted)

        val stored = store.loadHistory().first()
        assertEquals(2L, stored.sequence)
        assertEquals(10.0, stored.location?.latitude ?: 0.0, 0.0001)
        assertTrue(stored.isActive)
    }

    @Test
    fun `Regression 15 - Equal sequence with conflicting content rejection`() {
        val e1 = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            location = SosLocation(10.0, 20.0, 5f, 10_000L, "gps"),
            note = "Original note",
            sequence = 2L,
        )
        val ev1 = bridge.toIncidentEvent(bridge.buildSosPacket(e1, emergencyId))!!
        SosIncomingEventCoordinator.processIncomingSos(ev1, store)

        // Conflicting event at same sequence 2 with different location
        val conflictingEvent = IncidentEvent(
            eventId = emergencyId,
            originId = originId,
            type = IncidentType.SOS,
            occurredAtMs = 10_000L,
            latitude = 50.0,
            longitude = 60.0,
            severity = IncidentSeverity.CRITICAL,
            sequence = 2L,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to IncidentMapper.STATUS_ACTIVE,
                "sequence" to "2",
                "note" to "Original note",
            )
        )
        val resultConflictLoc = SosIncomingEventCoordinator.processIncomingSos(conflictingEvent, store)
        assertNull("Conflicting location at equal sequence must be rejected", resultConflictLoc)

        // Conflicting event at same sequence 2 with different note
        val conflictingEventNote = IncidentEvent(
            eventId = emergencyId,
            originId = originId,
            type = IncidentType.SOS,
            occurredAtMs = 10_000L,
            latitude = 10.0,
            longitude = 20.0,
            severity = IncidentSeverity.CRITICAL,
            sequence = 2L,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to IncidentMapper.STATUS_ACTIVE,
                "sequence" to "2",
                "note" to "Tampered note",
            )
        )
        val resultConflictNote = SosIncomingEventCoordinator.processIncomingSos(conflictingEventNote, store)
        assertNull("Conflicting note at equal sequence must be rejected", resultConflictNote)
    }

    @Test
    fun `Regression 16 - Sequenced ACTIVE cannot resurrect legacy COMPLETED`() {
        val legacyCompletedPacket = GeoRescueBlePacket(
            packetId = "leg-term-1",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            latitude = 1.0,
            longitude = 2.0,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = emptyMap(),
        )
        val legacyEvent = bridge.toIncidentEvent(legacyCompletedPacket)!!
        SosIncomingEventCoordinator.processIncomingSos(legacyEvent, store)

        val stored = store.loadHistory().first()
        assertFalse(stored.isActive)
        assertEquals(0L, stored.sequence)

        // Sequenced active event arrives later
        val sequencedActive = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = 1L)
        val seqActiveEvent = bridge.toIncidentEvent(bridge.buildSosPacket(sequencedActive, emergencyId))!!

        val result = SosIncomingEventCoordinator.processIncomingSos(seqActiveEvent, store)
        assertNull("Sequenced active must not resurrect a legacy completed emergency", result)

        val finalStored = store.loadHistory().first()
        assertFalse(finalStored.isActive)
    }

    @Test
    fun `Regression 17 - Legacy COMPLETED cannot cancel sequenced ACTIVE`() {
        val sequencedActive = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = 1L)
        val seqActiveEvent = bridge.toIncidentEvent(bridge.buildSosPacket(sequencedActive, emergencyId))!!
        SosIncomingEventCoordinator.processIncomingSos(seqActiveEvent, store)

        val legacyCompletedPacket = GeoRescueBlePacket(
            packetId = "leg-comp-seq",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = emptyMap(),
        )
        val legacyCompletedEvent = bridge.toIncidentEvent(legacyCompletedPacket)!!

        val result = SosIncomingEventCoordinator.processIncomingSos(legacyCompletedEvent, store)
        assertNull("Legacy completion cannot cancel sequenced active emergency", result)

        val stored = store.loadHistory().first()
        assertTrue(stored.isActive)
        assertEquals(1L, stored.sequence)
    }

    @Test
    fun `Regression 18 - Concurrent mutations do not allocate duplicate sequences in SosRepositoryImpl`() {
        val repo = SosRepositoryImpl(store)
        repo.startEmergency("concurrent-sos", 10_000L) // seq 1

        val threadCount = 20
        val executor = Executors.newFixedThreadPool(threadCount)
        val latch = CountDownLatch(threadCount)

        for (i in 0 until threadCount) {
            executor.submit {
                try {
                    if (i % 2 == 0) {
                        repo.updateActiveEmergencyLocation(
                            SosLocation(10.0 + i, 20.0 + i, 5f, 10_000L + i, "gps"),
                            SosLocationStatus.ACQUIRED
                        )
                    } else {
                        repo.updateActiveEmergencyNote("Update #$i")
                    }
                } finally {
                    latch.countDown()
                }
            }
        }

        assertTrue(latch.await(5, TimeUnit.SECONDS))
        executor.shutdown()

        val active = repo.getActiveEmergency()
        assertNotNull(active)
        // 1 initial + 20 concurrent updates = strictly sequence 21
        assertEquals(21L, active!!.sequence)
    }

    // =========================================================================
    // Phase P0.1 Integrity Corrections Tests
    // =========================================================================

    // --- Correction A: Strict Malformed Sequence Rejection ---

    @Test
    fun `Correction A - Missing sequence is accepted as legacy sequence zero in both conversion paths`() {
        val packet = GeoRescueBlePacket(
            packetId = "seq-test-missing",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            payload = emptyMap(), // no sequence key
        )
        val event = bridge.toIncidentEvent(packet)
        assertNotNull("Missing sequence must be accepted as legacy in toIncidentEvent", event)
        assertEquals(0L, event!!.sequence)

        val directEmergency = bridge.emergencyFromPacket(packet)
        assertNotNull("Missing sequence must be accepted as legacy in emergencyFromPacket", directEmergency)
        assertEquals(0L, directEmergency!!.sequence)
    }

    @Test
    fun `Correction A - Valid numeric sequences are accepted in both conversion paths`() {
        listOf("0" to 0L, "5" to 5L, "42" to 42L, "1000000" to 1000000L).forEach { (raw, expected) ->
            val packet = GeoRescueBlePacket(
                packetId = "seq-test-$raw",
                originDeviceId = originId,
                originEmergencyId = emergencyId,
                timestampMs = 10_000L,
                ttl = 3,
                type = GeoRescueBlePacketType.SOS,
                payload = mapOf("sequence" to raw),
            )
            val event = bridge.toIncidentEvent(packet)
            assertNotNull("Valid sequence '$raw' must be accepted in toIncidentEvent", event)
            assertEquals(expected, event!!.sequence)

            val emergency = bridge.emergencyFromPacket(packet)
            assertNotNull("Valid sequence '$raw' must be accepted in emergencyFromPacket", emergency)
            assertEquals(expected, emergency!!.sequence)
        }
    }

    @Test
    fun `Correction A - Malformed and invalid sequence strings are strictly rejected in both conversion paths`() {
        val malformedCases = listOf(
            "-1",
            "",
            " ",
            "   ",
            " 5 ",
            "5 ",
            " 5",
            "abc",
            "5a",
            "999999999999999999999999999999", // > Long.MAX_VALUE
            "1e3",
            "0x10",
            "05", // leading zero
            "+5", // explicit plus sign
        )

        malformedCases.forEach { malformed ->
            val packet = GeoRescueBlePacket(
                packetId = "seq-mal-$malformed",
                originDeviceId = originId,
                originEmergencyId = emergencyId,
                timestampMs = 10_000L,
                ttl = 3,
                type = GeoRescueBlePacketType.SOS,
                payload = mapOf("sequence" to malformed),
            )
            val event = bridge.toIncidentEvent(packet)
            assertNull("Malformed sequence '$malformed' must be rejected by toIncidentEvent", event)

            val emergency = bridge.emergencyFromPacket(packet)
            assertNull("Malformed sequence '$malformed' must be rejected by emergencyFromPacket", emergency)
        }
    }

    // --- Correction B: Pre-P0 BLE Incident ID Compatibility ---

    @Test
    fun `Correction B - Old pre-P0 active BLE record is migrated on valid matching completion`() {
        val oldRecordId = "MESH_BLE_${originId}_oldPkt001"
        val oldActive = SosEmergency(
            id = oldRecordId,
            startedAtMs = 10_000L,
            stoppedAtMs = null,
            sequence = 0L,
        )
        store.addToHistory(oldActive)
        assertEquals(1, store.loadHistory().size)
        assertEquals(oldRecordId, store.loadHistory().first().id)

        // Post-P0 completion event arrives
        val completionPacket = GeoRescueBlePacket(
            packetId = "new-comp-pkt",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val event = bridge.toIncidentEvent(completionPacket)!!
        val result = SosIncomingEventCoordinator.processIncomingSos(event, store)

        assertNotNull(result)
        assertFalse(result!!.isActive)
        val history = store.loadHistory()
        // Must migrate old record: exactly 1 history row, with canonical mesh ID
        assertEquals(1, history.size)
        val canonicalId = "MESH_${originId}_$emergencyId"
        assertEquals(canonicalId, history[0].id)
        assertEquals(25_000L, history[0].stoppedAtMs)
        assertEquals(2L, history[0].sequence)
    }

    @Test
    fun `Correction B - Old pre-P0 active record is migrated on valid matching update`() {
        val oldRecordId = "MESH_BLE_${originId}_oldPkt002"
        val oldActive = SosEmergency(
            id = oldRecordId,
            startedAtMs = 10_000L,
            stoppedAtMs = null,
            sequence = 0L,
        )
        store.addToHistory(oldActive)

        // Post-P0 location update arrives
        val updatePacket = GeoRescueBlePacket(
            packetId = "new-upd-pkt",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            latitude = 25.0,
            longitude = 35.0,
            status = BleSosBridge.SOS_STATUS_ACTIVE,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "ACTIVE",
                "startedAtMs" to "10000",
                "sequence" to "2",
            ),
        )
        val event = bridge.toIncidentEvent(updatePacket)!!
        val result = SosIncomingEventCoordinator.processIncomingSos(event, store)

        assertNotNull(result)
        assertTrue(result!!.isActive)
        val history = store.loadHistory()
        assertEquals(1, history.size)
        val canonicalId = "MESH_${originId}_$emergencyId"
        assertEquals(canonicalId, history[0].id)
        assertEquals(25.0, history[0].location?.latitude ?: 0.0, 0.0001)
        assertEquals(2L, history[0].sequence)
    }

    @Test
    fun `Correction B - Ambiguous multiple old records are not migrated`() {
        // Two old records for same origin and same start timestamp
        val old1 = SosEmergency(id = "MESH_BLE_${originId}_pktA", startedAtMs = 10_000L, stoppedAtMs = null)
        val old2 = SosEmergency(id = "MESH_BLE_${originId}_pktB", startedAtMs = 10_000L, stoppedAtMs = null)
        store.addToHistory(old1)
        store.addToHistory(old2)

        val completionPacket = GeoRescueBlePacket(
            packetId = "new-comp-ambig",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val event = bridge.toIncidentEvent(completionPacket)!!
        SosIncomingEventCoordinator.processIncomingSos(event, store)

        val history = store.loadHistory()
        // Ambiguity prevents arbitrary selection: old records must remain untouched
        assertTrue(history.any { it.id == "MESH_BLE_${originId}_pktA" && it.isActive })
        assertTrue(history.any { it.id == "MESH_BLE_${originId}_pktB" && it.isActive })
    }

    @Test
    fun `Correction B - Old record with different origin or different start time does not match`() {
        val oldDiffOrigin = SosEmergency(id = "MESH_BLE_otherNode_pkt1", startedAtMs = 10_000L, stoppedAtMs = null)
        val oldDiffTime = SosEmergency(id = "MESH_BLE_${originId}_pkt2", startedAtMs = 99_000L, stoppedAtMs = null)
        store.addToHistory(oldDiffOrigin)
        store.addToHistory(oldDiffTime)

        val completionPacket = GeoRescueBlePacket(
            packetId = "new-comp-mismatch",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val event = bridge.toIncidentEvent(completionPacket)!!
        SosIncomingEventCoordinator.processIncomingSos(event, store)

        val history = store.loadHistory()
        assertTrue(history.any { it.id == "MESH_BLE_otherNode_pkt1" })
        assertTrue(history.any { it.id == "MESH_BLE_${originId}_pkt2" })
    }

    @Test
    fun `Correction B - Already-completed old record is never reopened and delayed active cannot resurrect it`() {
        val oldCompletedId = "MESH_BLE_${originId}_oldCompleted"
        val oldCompleted = SosEmergency(
            id = oldCompletedId,
            startedAtMs = 10_000L,
            stoppedAtMs = 20_000L,
            sequence = 0L,
        )
        store.addToHistory(oldCompleted)

        // Delayed active arrives with same origin and start time
        val activePacket = GeoRescueBlePacket(
            packetId = "delayed-act-pkt",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_ACTIVE,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "ACTIVE",
                "startedAtMs" to "10000",
                "sequence" to "1",
            ),
        )
        val event = bridge.toIncidentEvent(activePacket)!!
        val result = SosIncomingEventCoordinator.processIncomingSos(event, store)

        assertNull("Delayed active must not resurrect an already completed old record", result)
        val history = store.loadHistory()
        assertEquals(1, history.size)
        assertEquals(oldCompletedId, history[0].id)
        assertFalse(history[0].isActive)
    }

    @Test
    fun `Correction B - Completion event with missing or malformed startedAtMs fails closed without migrating`() {
        val oldRecordId = "MESH_BLE_${originId}_oldPkt003"
        val oldActive = SosEmergency(id = oldRecordId, startedAtMs = 10_000L, stoppedAtMs = null)
        store.addToHistory(oldActive)

        // 1. Completion with missing startedAtMs in payload
        val packetMissingStart = GeoRescueBlePacket(
            packetId = "comp-missing-start",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 25_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "COMPLETED",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val eventMissingStart = bridge.toIncidentEvent(packetMissingStart)!!
        SosIncomingEventCoordinator.processIncomingSos(eventMissingStart, store)

        // Old record must remain intact and not migrated
        var history = store.loadHistory()
        assertTrue("Old record must not be migrated when startedAtMs is missing", history.any { it.id == oldRecordId && it.isActive })

        // 2. Completion with malformed startedAtMs
        val packetMalformedStart = GeoRescueBlePacket(
            packetId = "comp-malformed-start",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 25_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "COMPLETED",
                "startedAtMs" to "abc",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val eventMalformedStart = bridge.toIncidentEvent(packetMalformedStart)!!
        SosIncomingEventCoordinator.processIncomingSos(eventMalformedStart, store)

        history = store.loadHistory()
        assertTrue("Old record must not be migrated when startedAtMs is malformed", history.any { it.id == oldRecordId && it.isActive })
    }

    @Test
    fun `Correction B - Device IDs with underscores match strictly without prefix collision`() {
        // Device "node_1" has an active old record
        val oldRecordNode1 = SosEmergency(
            id = "MESH_BLE_node_1_pkt999",
            startedAtMs = 10_000L,
            stoppedAtMs = null,
        )
        store.addToHistory(oldRecordNode1)

        // Incoming completion from device "node" (prefix of "node_1")
        val bridgeNode = BleSosBridge(manager = null, originDeviceId = "node")
        val packetNode = GeoRescueBlePacket(
            packetId = "pkt-node",
            originDeviceId = "node",
            originEmergencyId = "em-node",
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to "em-node",
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val eventNode = bridgeNode.toIncidentEvent(packetNode)!!
        SosIncomingEventCoordinator.processIncomingSos(eventNode, store)

        // Old record belonging to "node_1" must NOT have been touched or migrated by "node"
        val history = store.loadHistory()
        assertTrue(history.any { it.id == "MESH_BLE_node_1_pkt999" && it.isActive })

        // Now incoming completion from exact origin "node_1"
        val bridgeNode1 = BleSosBridge(manager = null, originDeviceId = "node_1")
        val packetNode1 = GeoRescueBlePacket(
            packetId = "pkt-node1",
            originDeviceId = "node_1",
            originEmergencyId = "em-node1",
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to "em-node1",
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val eventNode1 = bridgeNode1.toIncidentEvent(packetNode1)!!
        val migratedResult = SosIncomingEventCoordinator.processIncomingSos(eventNode1, store)

        assertNotNull(migratedResult)
        // Now "node_1" record is migrated to canonical ID
        val updatedHistory = store.loadHistory()
        assertFalse(updatedHistory.any { it.id == "MESH_BLE_node_1_pkt999" })
        assertTrue(updatedHistory.any { it.id == "MESH_node_1_em-node1" && !it.isActive })
    }

    @Test
    fun `Correction B - Malformed legacy IDs are rejected from migration`() {
        // 1. Missing packet delimiter
        val malformed1 = SosEmergency(id = "MESH_BLE_nodeNoDelimiter", startedAtMs = 10_000L, stoppedAtMs = null)
        // 2. Empty origin
        val malformed2 = SosEmergency(id = "MESH_BLE__pktOnly", startedAtMs = 10_000L, stoppedAtMs = null)
        // 3. Empty packet ID
        val malformed3 = SosEmergency(id = "MESH_BLE_nodeEmptyPkt_", startedAtMs = 10_000L, stoppedAtMs = null)

        store.addToHistory(malformed1)
        store.addToHistory(malformed2)
        store.addToHistory(malformed3)

        val completionPacket = GeoRescueBlePacket(
            packetId = "pkt-comp-test",
            originDeviceId = "nodeNoDelimiter",
            originEmergencyId = "em-test",
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to "em-test",
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val bridgeTest = BleSosBridge(manager = null, originDeviceId = "nodeNoDelimiter")
        val event = bridgeTest.toIncidentEvent(completionPacket)!!
        SosIncomingEventCoordinator.processIncomingSos(event, store)

        val history = store.loadHistory()
        // Malformed records must not be migrated
        assertTrue(history.any { it.id == "MESH_BLE_nodeNoDelimiter" })
        assertTrue(history.any { it.id == "MESH_BLE__pktOnly" })
        assertTrue(history.any { it.id == "MESH_BLE_nodeEmptyPkt_" })
    }

    @Test
    fun `Correction C - Migration persistence failure preserves old record and returns null`() {
        val oldRecordId = "MESH_BLE_${originId}_persistFail"
        val oldActive = SosEmergency(id = oldRecordId, startedAtMs = 10_000L, stoppedAtMs = null)
        fakePrefs.map["sos_history"] = org.json.JSONArray().put(SosEmergencyJson.toJson(oldActive)).toString()

        val failingPrefs = FailingEditorSharedPreferences(fakePrefs)
        val failingStore = SharedPreferencesSosStore(failingPrefs)

        val completionPacket = GeoRescueBlePacket(
            packetId = "pkt-mig-fail",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val event = bridge.toIncidentEvent(completionPacket)!!

        val executor = Executors.newSingleThreadExecutor()
        val future = executor.submit(java.util.concurrent.Callable {
            SosIncomingEventCoordinator.processIncomingSos(event, failingStore)
        })
        val result = future.get(5, TimeUnit.SECONDS)
        executor.shutdown()

        // Migration must fail closed and return null
        assertNull("processIncomingSos must return null when migration persistence fails", result)

        // Old record must remain intact and recoverable
        val history = failingStore.loadHistory()
        assertTrue("Old record must be preserved when migration persistence fails", history.any { it.id == oldRecordId && it.isActive })
        assertFalse("New canonical record must not be present when persistence failed", history.any { it.id == "MESH_${originId}_$emergencyId" })
    }

    // --- Correction C: SharedPreferences Store-Level Synchronization ---

    @Test
    fun `Correction C - Concurrent addToHistory and completeActive calls maintain store consistency`() {
        val threadCount = 20
        val executor = Executors.newFixedThreadPool(threadCount)
        val latch = CountDownLatch(threadCount)

        // Seed an active emergency
        val initialActive = SosEmergency(id = "sos-local-active", startedAtMs = 10_000L, sequence = 1L)
        store.saveActive(initialActive)

        for (i in 0 until threadCount) {
            executor.submit {
                try {
                    if (i == 0) {
                        store.completeActive(initialActive.copy(stoppedAtMs = 20_000L, sequence = 2L))
                    } else {
                        val peerRecord = SosEmergency(
                            id = "MESH_peer_${i}_em",
                            startedAtMs = 10_000L + i,
                            stoppedAtMs = if (i % 2 == 0) 25_000L + i else null,
                            sequence = i.toLong(),
                        )
                        store.addToHistory(peerRecord)
                    }
                } finally {
                    latch.countDown()
                }
            }
        }

        assertTrue(latch.await(5, TimeUnit.SECONDS))
        executor.shutdown()

        // Active must be cleared by completeActive
        assertNull(store.loadActive())

        val history = store.loadHistory()
        // History must contain initial completed plus the 19 peer records = 20 items total
        assertEquals(threadCount, history.size)
        assertTrue(history.any { it.id == "sos-local-active" && !it.isActive })
    }

    // --- Correction D: Completion Persistence and Failure Handling ---

    private class FailingEditorSharedPreferences(
        private val delegate: SharedPreferencesSosStoreTest.FakeSharedPreferences
    ) : android.content.SharedPreferences by delegate {
        override fun edit(): android.content.SharedPreferences.Editor =
            FailingEditor(delegate.edit())

        class FailingEditor(
            private val editorDelegate: android.content.SharedPreferences.Editor
        ) : android.content.SharedPreferences.Editor by editorDelegate {
            override fun remove(key: String): android.content.SharedPreferences.Editor {
                editorDelegate.remove(key)
                return this
            }
            override fun putString(key: String, value: String?): android.content.SharedPreferences.Editor {
                editorDelegate.putString(key, value)
                return this
            }
            override fun commit(): Boolean = false // Simulate atomic disk write failure
        }
    }

    @Test
    fun `Correction D - Active state remains recoverable when commit fails`() {
        val failingPrefs = FailingEditorSharedPreferences(fakePrefs)
        val failingStore = SharedPreferencesSosStore(failingPrefs)
        val repo = SosRepositoryImpl(failingStore)

        val active = repo.startEmergency("failing-sos", 10_000L)
        assertNotNull(active)
        assertEquals(1L, active!!.sequence)

        // Attempt completion off main thread (where commit() is used for durability)
        val executor = Executors.newSingleThreadExecutor()
        val future = executor.submit(java.util.concurrent.Callable {
            repo.completeEmergency(25_000L)
        })
        val completed = future.get(5, TimeUnit.SECONDS)
        executor.shutdown()

        assertNull("completeEmergency must return null when persistence fails", completed)

        // Active emergency remains recoverable
        val recovered = repo.getActiveEmergency()
        assertNotNull("Active emergency must remain recoverable after persistence failure", recovered)
        assertEquals("failing-sos", recovered!!.id)
        assertTrue(recovered.isActive)

        // History must not falsely contain the completed record
        val history = failingStore.loadHistory()
        assertFalse(history.any { it.id == "failing-sos" })
    }

    // --- Correction E: Sequence Overflow and Exhaustion Protection ---

    @Test
    fun `Correction E - Sequence at Long MAX_VALUE minus one increments normally`() {
        val nearMax = SosEmergency(id = emergencyId, startedAtMs = 10_000L, sequence = Long.MAX_VALUE - 1L)
        store.saveActive(nearMax)
        val repo = SosRepositoryImpl(store)

        repo.updateActiveEmergencyLocation(
            SosLocation(1.0, 2.0, 5f, 11_000L, "gps"),
            SosLocationStatus.ACQUIRED
        )
        val active = repo.getActiveEmergency()
        assertNotNull(active)
        assertEquals(Long.MAX_VALUE, active!!.sequence)
    }

    @Test
    fun `Correction E - Mutations at Long MAX_VALUE are rejected and never wrap to negative or zero`() {
        val maxEmergency = SosEmergency(
            id = emergencyId,
            startedAtMs = 10_000L,
            location = SosLocation(1.0, 2.0, 5f, 10_000L, "gps"),
            note = "Original note",
            sequence = Long.MAX_VALUE
        )
        store.saveActive(maxEmergency)
        val repo = SosRepositoryImpl(store)

        // 1. Location update at MAX_VALUE must be rejected
        repo.updateActiveEmergencyLocation(
            SosLocation(10.0, 20.0, 5f, 12_000L, "gps"),
            SosLocationStatus.ACQUIRED
        )
        var current = repo.getActiveEmergency()
        assertNotNull(current)
        assertEquals(Long.MAX_VALUE, current!!.sequence)
        assertEquals(1.0, current.location?.latitude ?: 0.0, 0.0001)

        // 2. Note update at MAX_VALUE must be rejected
        repo.updateActiveEmergencyNote("New note")
        current = repo.getActiveEmergency()
        assertNotNull(current)
        assertEquals(Long.MAX_VALUE, current!!.sequence)
        assertEquals("Original note", current.note)

        // 3. completeEmergency at MAX_VALUE must be rejected (return null)
        val completed = repo.completeEmergency(25_000L)
        assertNull("completeEmergency at Long.MAX_VALUE must return null", completed)
        current = repo.getActiveEmergency()
        assertNotNull(current)
        assertEquals(Long.MAX_VALUE, current!!.sequence)
        assertTrue(current.isActive) // Still active, not wrapped
    }

    // --- Correction F: Realistic SharedPreferences Failure Simulation & Ordinary Incoming Failure ---

    class RealisticFailingSharedPreferences(
        initialData: Map<String, Any?> = emptyMap(),
        var simulateDiskFailure: Boolean = true,
        var throwOnApply: Boolean = false,
    ) : android.content.SharedPreferences {
        val inMemoryMap = HashMap<String, Any?>(initialData)
        val diskMap = HashMap<String, Any?>(initialData)

        override fun getAll(): MutableMap<String, *> = HashMap(inMemoryMap)
        override fun getString(key: String, defValue: String?): String? =
            inMemoryMap[key] as? String ?: defValue
        override fun getStringSet(key: String, defValues: MutableSet<String>?): MutableSet<String>? =
            (inMemoryMap[key] as? Set<*>)?.filterIsInstance<String>()?.toMutableSet() ?: defValues
        override fun getInt(key: String, defValue: Int): Int = inMemoryMap[key] as? Int ?: defValue
        override fun getLong(key: String, defValue: Long): Long = inMemoryMap[key] as? Long ?: defValue
        override fun getFloat(key: String, defValue: Float): Float = inMemoryMap[key] as? Float ?: defValue
        override fun getBoolean(key: String, defValue: Boolean): Boolean = inMemoryMap[key] as? Boolean ?: defValue
        override fun contains(key: String): Boolean = inMemoryMap.containsKey(key)
        override fun edit(): android.content.SharedPreferences.Editor = Editor()
        override fun registerOnSharedPreferenceChangeListener(listener: android.content.SharedPreferences.OnSharedPreferenceChangeListener?) {}
        override fun unregisterOnSharedPreferenceChangeListener(listener: android.content.SharedPreferences.OnSharedPreferenceChangeListener?) {}

        fun reloadFromDisk() {
            inMemoryMap.clear()
            inMemoryMap.putAll(diskMap)
        }

        inner class Editor : android.content.SharedPreferences.Editor {
            private val modifications = mutableMapOf<String, Any?>()
            private val removals = mutableSetOf<String>()
            private var clearRequested = false

            override fun putString(key: String, value: String?): android.content.SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putStringSet(key: String, values: MutableSet<String>?): android.content.SharedPreferences.Editor {
                modifications[key] = values
                removals.remove(key)
                return this
            }
            override fun putInt(key: String, value: Int): android.content.SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putLong(key: String, value: Long): android.content.SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putFloat(key: String, value: Float): android.content.SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun putBoolean(key: String, value: Boolean): android.content.SharedPreferences.Editor {
                modifications[key] = value
                removals.remove(key)
                return this
            }
            override fun remove(key: String): android.content.SharedPreferences.Editor {
                removals.add(key)
                modifications.remove(key)
                return this
            }
            override fun clear(): android.content.SharedPreferences.Editor {
                clearRequested = true
                return this
            }

            override fun apply() {
                if (throwOnApply) {
                    throw RuntimeException("Simulated apply failure during rollback")
                }
                applyToMap(inMemoryMap)
                if (!simulateDiskFailure) {
                    applyToMap(diskMap)
                }
            }

            override fun commit(): Boolean {
                // Stage 2: commitToMemory applies modifications to in-memory map first
                applyToMap(inMemoryMap)
                // Stage 3 & 4: Disk persistence fails, returns false without updating diskMap
                if (simulateDiskFailure) {
                    return false
                }
                applyToMap(diskMap)
                return true
            }

            private fun applyToMap(target: MutableMap<String, Any?>) {
                if (clearRequested) target.clear()
                for (key in removals) target.remove(key)
                for ((key, value) in modifications) {
                    if (value == null) target.remove(key)
                    else target[key] = value
                }
            }
        }
    }

    @Test
    fun `Correction B - Ordinary incoming SOS persistence failure returns null`() {
        val failingPrefs = RealisticFailingSharedPreferences(simulateDiskFailure = true)
        val failingStore = SharedPreferencesSosStore(failingPrefs)

        val packet = GeoRescueBlePacket(
            packetId = "pkt-ord-fail",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_ACTIVE,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "ACTIVE",
                "startedAtMs" to "10000",
                "sequence" to "1",
            ),
        )
        val event = bridge.toIncidentEvent(packet)!!

        val executor = Executors.newSingleThreadExecutor()
        val future = executor.submit(java.util.concurrent.Callable {
            SosIncomingEventCoordinator.processIncomingSos(event, failingStore)
        })
        val result = future.get(5, TimeUnit.SECONDS)
        executor.shutdown()

        // Must fail closed and return null when persistence fails
        assertNull("processIncomingSos must return null when ordinary history persistence fails", result)
    }

    @Test
    fun `Correction B - Ordinary incoming SOS persistence success returns accepted emergency`() {
        val successPrefs = RealisticFailingSharedPreferences(simulateDiskFailure = false)
        val successStore = SharedPreferencesSosStore(successPrefs)

        val packet = GeoRescueBlePacket(
            packetId = "pkt-ord-success",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_ACTIVE,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "ACTIVE",
                "startedAtMs" to "10000",
                "sequence" to "1",
            ),
        )
        val event = bridge.toIncidentEvent(packet)!!

        val executor = Executors.newSingleThreadExecutor()
        val future = executor.submit(java.util.concurrent.Callable {
            SosIncomingEventCoordinator.processIncomingSos(event, successStore)
        })
        val result = future.get(5, TimeUnit.SECONDS)
        executor.shutdown()

        assertNotNull("processIncomingSos must return accepted emergency on successful persistence", result)
        assertEquals("MESH_${originId}_$emergencyId", result!!.id)
        assertEquals(1L, result.sequence)
    }

    @Test
    fun `Priority E - Test 1 - Failed completion restores current-process active SOS`() {
        val initialActive = SosEmergency(id = "sos-active-1", startedAtMs = 10_000L, sequence = 1L, note = "Test trapped")
        val initialActiveJson = SosEmergencyJson.toJson(initialActive).toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("active_emergency" to initialActiveJson, "sos_history" to "[]"),
            simulateDiskFailure = true
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)
        val repo = SosRepositoryImpl(store1)

        val executor = Executors.newSingleThreadExecutor()
        val future = executor.submit(java.util.concurrent.Callable {
            repo.completeEmergency(25_000L)
        })
        val completed = future.get(5, TimeUnit.SECONDS)
        executor.shutdown()

        // 1. Completion returns failure (null)
        assertNull("completeEmergency must return null when commit fails", completed)

        // 2. loadActive() still returns the original active SOS in the same running process
        val restoredActive = store1.loadActive()
        assertNotNull("Active SOS must be restored in memory after failed completion", restoredActive)
        assertEquals("sos-active-1", restoredActive!!.id)
        assertEquals(10_000L, restoredActive.startedAtMs)
        assertEquals(1L, restoredActive.sequence)
        assertEquals("Test trapped", restoredActive.note)
        assertTrue(restoredActive.isActive)

        // 3. History remains restored to its previous state (empty)
        val history = store1.loadHistory()
        assertTrue("History must remain empty as before the failed completion", history.isEmpty())
    }

    @Test
    fun `Priority E - Test 2 - New store instance sees restored active SOS`() {
        val initialActive = SosEmergency(id = "sos-active-1", startedAtMs = 10_000L, sequence = 1L)
        val initialActiveJson = SosEmergencyJson.toJson(initialActive).toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("active_emergency" to initialActiveJson, "sos_history" to "[]"),
            simulateDiskFailure = true
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)
        val repo = SosRepositoryImpl(store1)

        val executor = Executors.newSingleThreadExecutor()
        executor.submit(java.util.concurrent.Callable {
            repo.completeEmergency(25_000L)
        }).get(5, TimeUnit.SECONDS)
        executor.shutdown()

        // Construct a new store instance over the same preferences instance
        val newStore = SharedPreferencesSosStore(realisticPrefs)
        val active = newStore.loadActive()
        assertNotNull("New store instance over same preferences sees restored active SOS", active)
        assertEquals("sos-active-1", active!!.id)
        assertTrue(active.isActive)
    }

    @Test
    fun `Priority E - Test 3 - Retry succeeds after storage recovers`() {
        val initialActive = SosEmergency(id = "sos-active-1", startedAtMs = 10_000L, sequence = 1L)
        val initialActiveJson = SosEmergencyJson.toJson(initialActive).toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("active_emergency" to initialActiveJson, "sos_history" to "[]"),
            simulateDiskFailure = true
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)
        val repo = SosRepositoryImpl(store1)

        // 1. Initial attempt fails
        val executor1 = Executors.newSingleThreadExecutor()
        val firstAttempt = executor1.submit(java.util.concurrent.Callable {
            repo.completeEmergency(25_000L)
        }).get(5, TimeUnit.SECONDS)
        executor1.shutdown()
        assertNull(firstAttempt)

        // 2. Verify active SOS is still accessible in memory
        val activeBeforeRetry = repo.getActiveEmergency()
        assertNotNull("Active emergency must be accessible for retry", activeBeforeRetry)
        assertEquals("sos-active-1", activeBeforeRetry!!.id)

        // 3. Storage recovers
        realisticPrefs.simulateDiskFailure = false

        // 4. Retry completion
        val executor2 = Executors.newSingleThreadExecutor()
        val retried = executor2.submit(java.util.concurrent.Callable {
            repo.completeEmergency(35_000L)
        }).get(5, TimeUnit.SECONDS)
        executor2.shutdown()

        // 5. Completion succeeds
        assertNotNull("Retry must succeed once storage recovers", retried)
        assertEquals("sos-active-1", retried!!.id)
        assertEquals(2L, retried.sequence)
        assertEquals(35_000L, retried.stoppedAtMs)
        assertFalse(retried.isActive)

        // 6. Active SOS is removed
        assertNull("Active emergency must now be null", repo.getActiveEmergency())
        assertNull(store1.loadActive())

        // 7. Exactly one history record exists
        val history = store1.loadHistory()
        assertEquals(1, history.size)
        assertEquals("sos-active-1", history[0].id)
        assertEquals(2L, history[0].sequence)
        assertEquals(35_000L, history[0].stoppedAtMs)
    }

    @Test
    fun `Priority E - Test 4 - Failed history replacement restores previous history`() {
        val hist1 = SosEmergency(id = "hist-1", startedAtMs = 1000L, stoppedAtMs = 2000L)
        val hist2 = SosEmergency(id = "hist-2", startedAtMs = 3000L, stoppedAtMs = 4000L)
        val initialHistoryJson = org.json.JSONArray()
            .put(SosEmergencyJson.toJson(hist1))
            .put(SosEmergencyJson.toJson(hist2))
            .toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("sos_history" to initialHistoryJson),
            simulateDiskFailure = true
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)

        val newRecord = SosEmergency(id = "hist-3", startedAtMs = 5000L, stoppedAtMs = 6000L)

        val executor = Executors.newSingleThreadExecutor()
        val success = executor.submit(java.util.concurrent.Callable {
            store1.replaceInHistory("hist-3", newRecord)
        }).get(5, TimeUnit.SECONDS)
        executor.shutdown()

        // 1. Must return false
        assertFalse("replaceInHistory must return false on failed commit", success)

        // 2. Previous history records must be restored and intact
        val history = store1.loadHistory()
        assertEquals(2, history.size)
        val ids = history.map { it.id }.toSet()
        assertTrue(ids.contains("hist-1"))
        assertTrue(ids.contains("hist-2"))
        assertFalse("New failed record must not be in history", ids.contains("hist-3"))
    }

    @Test
    fun `Priority E - Test 5 - Failed legacy migration preserves legacy record`() {
        val oldRecordId = "MESH_BLE_${originId}_oldPkt1"
        val oldActive = SosEmergency(id = oldRecordId, startedAtMs = 10_000L, stoppedAtMs = null)
        val initialHistoryJson = org.json.JSONArray().put(SosEmergencyJson.toJson(oldActive)).toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("sos_history" to initialHistoryJson),
            simulateDiskFailure = true
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)

        val completionPacket = GeoRescueBlePacket(
            packetId = "pkt-realistic-mig",
            originDeviceId = originId,
            originEmergencyId = emergencyId,
            timestampMs = 10_000L,
            ttl = 3,
            type = GeoRescueBlePacketType.SOS,
            status = BleSosBridge.SOS_STATUS_COMPLETED,
            payload = mapOf(
                "emergencyId" to emergencyId,
                "status" to "COMPLETED",
                "startedAtMs" to "10000",
                "stoppedAtMs" to "25000",
                "sequence" to "2",
            ),
        )
        val event = bridge.toIncidentEvent(completionPacket)!!

        val executor = Executors.newSingleThreadExecutor()
        val result = executor.submit(java.util.concurrent.Callable {
            SosIncomingEventCoordinator.processIncomingSos(event, store1)
        }).get(5, TimeUnit.SECONDS)
        executor.shutdown()

        // 1. Migration returns null
        assertNull("processIncomingSos must return null when migration fails", result)

        // 2. Legacy record remains restored in running process
        val inMemHistory = store1.loadHistory()
        assertTrue("Legacy record must be restored in running process memory", inMemHistory.any { it.id == oldRecordId && it.isActive })
        assertFalse("Canonical record must not remain in history", inMemHistory.any { it.id == "MESH_${originId}_$emergencyId" })

        // 3. New store instance over same preferences also sees restored legacy record
        val newStore = SharedPreferencesSosStore(realisticPrefs)
        assertTrue(newStore.loadHistory().any { it.id == oldRecordId && it.isActive })
    }

    @Test
    fun `Priority E - Test 7 - No duplicate emergency after failed completion`() {
        val initialActive = SosEmergency(id = "sos-real-active", startedAtMs = 10_000L, sequence = 1L)
        val initialActiveJson = SosEmergencyJson.toJson(initialActive).toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("active_emergency" to initialActiveJson, "sos_history" to "[]"),
            simulateDiskFailure = true
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)
        val repo = SosRepositoryImpl(store1)

        val executor = Executors.newSingleThreadExecutor()
        val completed = executor.submit(java.util.concurrent.Callable {
            repo.completeEmergency(25_000L)
        }).get(5, TimeUnit.SECONDS)
        executor.shutdown()

        assertNull(completed)

        // Attempt to start a second emergency while first is still active
        val second = repo.startEmergency("sos-second", 30_000L)
        assertNull("Second emergency must be rejected while first is still active following failure", second)
        assertEquals("sos-real-active", repo.getActiveEmergency()?.id)
    }

    @Test
    fun `Priority E - Test 8 - Successful completion remains unchanged`() {
        val initialActive = SosEmergency(id = "sos-success-path", startedAtMs = 10_000L, sequence = 1L)
        val initialActiveJson = SosEmergencyJson.toJson(initialActive).toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("active_emergency" to initialActiveJson, "sos_history" to "[]"),
            simulateDiskFailure = false
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)
        val repo = SosRepositoryImpl(store1)

        val executor = Executors.newSingleThreadExecutor()
        val completed = executor.submit(java.util.concurrent.Callable {
            repo.completeEmergency(25_000L)
        }).get(5, TimeUnit.SECONDS)
        executor.shutdown()

        assertNotNull("Successful completion must return completed record", completed)
        assertEquals("sos-success-path", completed!!.id)
        assertEquals(2L, completed.sequence)
        assertEquals(25_000L, completed.stoppedAtMs)
        assertFalse(completed.isActive)

        assertNull("Active emergency must be removed", store1.loadActive())
        val history = store1.loadHistory()
        assertEquals(1, history.size)
        assertEquals("sos-success-path", history[0].id)
        assertEquals(2L, history[0].sequence)
    }

    @Test
    fun `Priority E - Test 9 - Rollback failure contains exception without unhandled crash (exception containment only, not recovery)`() {
        // NOTE: In RealisticFailingSharedPreferences, throwOnApply throws at the beginning of apply()
        // BEFORE applyToMap(inMemoryMap) can execute. Because commit() previously mutated inMemoryMap
        // by removing active_emergency, and the thrown exception aborts the rollback editor before
        // inMemoryMap can be restored, in-memory state is NOT recovered in this scenario.
        // This test verifies EXCEPTION CONTAINMENT only: runCatching prevents the exception
        // from escaping, logs the diagnostic error, and ensures completeEmergency returns null
        // rather than crashing the caller or leaving an unhandled runtime failure.
        val initialActive = SosEmergency(id = "sos-rollback-fail", startedAtMs = 10_000L, sequence = 1L)
        val initialActiveJson = SosEmergencyJson.toJson(initialActive).toString()

        val realisticPrefs = RealisticFailingSharedPreferences(
            initialData = mapOf("active_emergency" to initialActiveJson, "sos_history" to "[]"),
            simulateDiskFailure = true,
            throwOnApply = true
        )
        val store1 = SharedPreferencesSosStore(realisticPrefs)
        val repo = SosRepositoryImpl(store1)

        val executor = Executors.newSingleThreadExecutor()
        val completed = executor.submit(java.util.concurrent.Callable {
            repo.completeEmergency(25_000L)
        }).get(5, TimeUnit.SECONDS)
        executor.shutdown()

        // 1. completeEmergency safely returns null without unhandled crash
        assertNull("completeEmergency must return null when rollback itself fails", completed)

        // 2. Explicitly demonstrate that in-memory state is NOT recovered (exception containment only)
        assertNull(
            "In this simulated failure, inMemoryMap was not restored because apply threw before mutating memory",
            store1.loadActive()
        )
    }
}
