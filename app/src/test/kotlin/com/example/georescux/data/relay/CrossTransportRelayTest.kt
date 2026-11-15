package com.example.georescux.data.relay

import com.example.georescux.data.ble.BleSosBridge
import com.example.georescux.data.ble.GeoRescueBleManager
import com.example.georescux.data.ble.GeoRescueBleRepository
import com.example.georescux.data.sos.LocalSosStore
import com.example.georescux.data.sos.SosIncomingEventCoordinator
import com.example.georescux.domain.ble.GeoRescueBleMeshNode
import com.example.georescux.domain.ble.GeoRescueBlePacket
import com.example.georescux.domain.ble.GeoRescueBlePacketSink
import com.example.georescux.domain.ble.GeoRescueBlePacketType
import com.example.georescux.domain.ble.GeoRescueBleRelayDecision
import com.example.georescux.domain.ble.QueuedPacket
import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.incident.IncidentSeverity
import com.example.georescux.domain.incident.IncidentType
import com.example.georescux.domain.relay.CrossTransportDeduplicator
import com.example.georescux.domain.relay.RelayDecision
import com.example.georescux.domain.relay.RelayEngine
import com.example.georescux.domain.relay.RelayEnvelope
import com.example.georescux.domain.relay.RelayTransport
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import java.util.Collections
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

class CrossTransportRelayTest {

    private val selfDeviceId = "DEVICE_RELAY_B"
    private val victimDeviceId = "DEVICE_VICTIM_A"

    private lateinit var nearbyTransport: FakeRelayTransport
    private lateinit var seenEventStore: FakeSeenEventStore
    private lateinit var outboundQueue: FakeRelayOutboundQueue
    private lateinit var nearbyEngine: DurableRelayEngine

    private lateinit var bleSink: FakeBlePacketSink
    private lateinit var bleRepo: FakeBleRepository
    private lateinit var bleMeshNode: GeoRescueBleMeshNode
    private lateinit var bleSosBridge: BleSosBridge

    private lateinit var deduplicator: CrossTransportDeduplicator
    private lateinit var bridge: CrossTransportRelayBridge

    @Before
    fun setUp() {
        nearbyTransport = FakeRelayTransport(mutableListOf("nearby_peer_C"))
        seenEventStore = FakeSeenEventStore()
        outboundQueue = FakeRelayOutboundQueue()

        val rawRelayEngine = RelayEngine(selfOriginId = selfDeviceId, transport = nearbyTransport)
        nearbyEngine = DurableRelayEngine(rawRelayEngine, seenEventStore, outboundQueue)

        bleSink = FakeBlePacketSink(mutableListOf("ble_peer_C"))
        bleRepo = FakeBleRepository()
        bleMeshNode = GeoRescueBleMeshNode(
            selfDeviceId = selfDeviceId,
            sink = bleSink,
            seenStore = bleRepo,
            outboundQueue = bleRepo,
        )
        bleSosBridge = BleSosBridge(manager = null, originDeviceId = selfDeviceId)

        deduplicator = CrossTransportDeduplicator()
        bridge = CrossTransportRelayBridge(
            blePublisher = { bleMeshNode.publish(it) },
            relayEngine = nearbyEngine,
            selfOriginId = { selfDeviceId },
            deduplicator = deduplicator,
        )
    }

    // --- Helper builders ---

    private fun createBleSosPacket(
        emergencyId: String,
        sequence: Long = 1L,
        status: String = BleSosBridge.SOS_STATUS_ACTIVE,
        ttl: Int = 5,
        startedAtMs: Long = System.currentTimeMillis() - 5_000L,
        stoppedAtMs: Long? = null,
        note: String? = "Trapped on roof",
    ): GeoRescueBlePacket = GeoRescueBlePacket(
        packetId = "GRX-${emergencyId}-seq$sequence",
        originDeviceId = victimDeviceId,
        originEmergencyId = emergencyId,
        timestampMs = startedAtMs,
        ttl = ttl,
        type = GeoRescueBlePacketType.SOS,
        latitude = 30.1234,
        longitude = 78.5678,
        status = status,
        payload = buildMap {
            put("emergencyId", emergencyId)
            put("status", status)
            put("startedAtMs", startedAtMs.toString())
            if (stoppedAtMs != null) put("stoppedAtMs", stoppedAtMs.toString())
            put("sequence", sequence.toString())
            if (note != null) put("note", note)
        },
        noteText = note,
    )

    private fun createNearbySosEnvelope(
        emergencyId: String,
        sequence: Long = 1L,
        status: String = "ACTIVE",
        startedAtMs: Long = System.currentTimeMillis() - 5_000L,
        stoppedAtMs: Long? = null,
        note: String? = "Trapped on roof",
        remainingHops: Int? = GeoRescueBlePacket.DEFAULT_TTL_HOPS,
    ): Pair<RelayEnvelope, SosEmergency> {
        val payload = buildMap {
            put("emergencyId", emergencyId)
            put("status", status)
            put("startedAtMs", startedAtMs.toString())
            if (stoppedAtMs != null) put("stoppedAtMs", stoppedAtMs.toString())
            put("sequence", sequence.toString())
            if (note != null) put("note", note)
            if (remainingHops != null) put("remainingHops", remainingHops.toString())
        }

        val event = IncidentEvent(
            eventId = if (status == "COMPLETED") "$emergencyId:COMPLETED" else emergencyId,
            originId = victimDeviceId,
            type = IncidentType.SOS,
            occurredAtMs = stoppedAtMs ?: startedAtMs,
            latitude = 30.1234,
            longitude = 78.5678,
            severity = IncidentSeverity.CRITICAL,
            sequence = sequence,
            payload = payload,
        )

        val emergency = SosEmergency(
            id = "MESH_${victimDeviceId}_$emergencyId",
            startedAtMs = startedAtMs,
            stoppedAtMs = stoppedAtMs,
            location = SosLocation(30.1234, 78.5678, 5f, startedAtMs, "nearby"),
            locationStatus = SosLocationStatus.ACQUIRED,
            note = note,
            sequence = sequence,
        )

        return Pair(RelayEnvelope(event = event, payload = event.canonicalString(), fromPeerId = "nearby_sender"), emergency)
    }

    // --- Scenario 1: BLE -> Nearby forwarding ---

    @Test
    fun `1 BLE SOS packet is converted and forwarded through Nearby transport`() {
        val blePacket = createBleSosPacket(emergencyId = "em-101", sequence = 1L)
        val event = bleSosBridge.toIncidentEvent(blePacket)!!

        val forwarded = bridge.forwardBleToNearby(blePacket, event)
        assertTrue("Bridge must forward valid BLE packet to Nearby", forwarded)

        // Verify sent over Nearby
        assertEquals(1, nearbyTransport.sentPayloads.size)
        val sentPayload = nearbyTransport.sentPayloads[0]
        assertTrue("Sent payload must contain victimDeviceId", sentPayload.contains(victimDeviceId))
        assertTrue("Sent payload must contain em-101", sentPayload.contains("em-101"))
    }

    // --- Scenario 2: Nearby -> BLE forwarding ---

    @Test
    fun `2 Nearby SOS event is converted and forwarded through BLE mesh`() {
        val (envelope, emergency) = createNearbySosEnvelope(emergencyId = "em-201", sequence = 1L)

        val forwarded = bridge.forwardNearbyToBle(envelope, emergency)
        assertTrue("Bridge must forward valid Nearby event to BLE", forwarded)

        // Verify sent over BLE
        assertEquals(1, bleSink.sentPackets.size)
        val sentBle = bleSink.sentPackets[0]
        assertEquals(victimDeviceId, sentBle.originDeviceId)
        assertEquals("em-201", sentBle.originEmergencyId)
        assertEquals(1L, sentBle.payload["sequence"]?.toLong())
        assertEquals(GeoRescueBlePacket.DEFAULT_TTL_HOPS - 1, sentBle.ttl)
        assertEquals(BleSosBridge.SOS_STATUS_ACTIVE, sentBle.status)
    }

    // --- Scenario 3: BLE -> Nearby -> BLE loop suppression ---

    @Test
    fun `3 BLE received packet forwarded to Nearby does not loop back to BLE`() {
        val blePacket = createBleSosPacket(emergencyId = "em-301", sequence = 1L)
        val event = bleSosBridge.toIncidentEvent(blePacket)!!

        // Forward BLE -> Nearby
        assertTrue(bridge.forwardBleToNearby(blePacket, event))

        // Same emergency returns from Nearby
        val (returnEnvelope, returnEmergency) = createNearbySosEnvelope(emergencyId = "em-301", sequence = 1L)
        val loopAttempt = bridge.forwardNearbyToBle(returnEnvelope, returnEmergency)

        assertFalse("Bridge must suppress loopback of the same emergency sequence back to BLE", loopAttempt)
        assertTrue("No BLE packet should have been sent during loopback", bleSink.sentPackets.isEmpty())
    }

    // --- Scenario 4: Nearby -> BLE -> Nearby loop suppression ---

    @Test
    fun `4 Nearby received event forwarded to BLE does not loop back to Nearby`() {
        val (envelope, emergency) = createNearbySosEnvelope(emergencyId = "em-401", sequence = 1L)

        // Forward Nearby -> BLE
        assertTrue(bridge.forwardNearbyToBle(envelope, emergency))

        // Same emergency returns from BLE
        val returnBlePacket = createBleSosPacket(emergencyId = "em-401", sequence = 1L)
        val returnEvent = bleSosBridge.toIncidentEvent(returnBlePacket)!!
        val loopAttempt = bridge.forwardBleToNearby(returnBlePacket, returnEvent)

        assertFalse("Bridge must suppress loopback of the same emergency sequence back to Nearby", loopAttempt)
        assertTrue("No Nearby payload should have been sent during loopback", nearbyTransport.sentPayloads.isEmpty())
    }

    // --- Scenario 5: Duplicate delivery through both transports ---

    @Test
    fun `5 Duplicate delivery of identical emergency on same transport is rejected`() {
        val blePacket = createBleSosPacket(emergencyId = "em-501", sequence = 1L)
        val event = bleSosBridge.toIncidentEvent(blePacket)!!

        assertTrue("First forward must succeed", bridge.forwardBleToNearby(blePacket, event))
        assertFalse("Duplicate forward must be rejected", bridge.forwardBleToNearby(blePacket, event))
        assertEquals(1, nearbyTransport.sentPayloads.size)
    }

    // --- Scenario 6: Newer lifecycle sequence acceptance ---

    @Test
    fun `6 Newer lifecycle sequence update is accepted and forwarded`() {
        val blePacket1 = createBleSosPacket(emergencyId = "em-601", sequence = 1L, note = "Initial SOS")
        val event1 = bleSosBridge.toIncidentEvent(blePacket1)!!
        assertTrue(bridge.forwardBleToNearby(blePacket1, event1))

        // Newer sequence 2 arrives
        val blePacket2 = createBleSosPacket(emergencyId = "em-601", sequence = 2L, note = "Updated GPS location")
        val event2 = bleSosBridge.toIncidentEvent(blePacket2)!!
        assertTrue("Newer sequence must be accepted and forwarded", bridge.forwardBleToNearby(blePacket2, event2))

        assertEquals(2, nearbyTransport.sentPayloads.size)
    }

    // --- Scenario 7: Stale lifecycle sequence rejection ---

    @Test
    fun `7 Stale lifecycle sequence update is rejected`() {
        val blePacket2 = createBleSosPacket(emergencyId = "em-701", sequence = 5L)
        val event2 = bleSosBridge.toIncidentEvent(blePacket2)!!
        assertTrue(bridge.forwardBleToNearby(blePacket2, event2))

        // Delayed stale sequence 3 arrives
        val blePacketStale = createBleSosPacket(emergencyId = "em-701", sequence = 3L)
        val eventStale = bleSosBridge.toIncidentEvent(blePacketStale)!!
        assertFalse("Stale sequence must be rejected", bridge.forwardBleToNearby(blePacketStale, eventStale))

        assertEquals(1, nearbyTransport.sentPayloads.size)
    }

    // --- Scenario 8: Completion-before-active behavior ---

    @Test
    fun `8 Completion event arriving before active event is accepted and forwarded`() {
        val now = System.currentTimeMillis()
        // Active event was dropped, COMPLETED arrives first
        val (completedEnvelope, completedEmergency) = createNearbySosEnvelope(
            emergencyId = "em-801",
            sequence = 2L,
            status = "COMPLETED",
            startedAtMs = now - 10_000L,
            stoppedAtMs = now,
        )

        val forwarded = bridge.forwardNearbyToBle(completedEnvelope, completedEmergency)
        assertTrue("Completion arriving before active must be forwarded", forwarded)

        assertEquals(1, bleSink.sentPackets.size)
        assertEquals(BleSosBridge.SOS_STATUS_COMPLETED, bleSink.sentPackets[0].status)
        assertEquals(now.toString(), bleSink.sentPackets[0].payload["stoppedAtMs"])
    }

    // --- Scenario 9: Completed emergency cannot be resurrected ---

    @Test
    fun `9 Completed emergency cannot be resurrected by delayed active packet`() {
        val now = System.currentTimeMillis()
        // 1. Completion is forwarded
        val (completedEnvelope, completedEmergency) = createNearbySosEnvelope(
            emergencyId = "em-901",
            sequence = 2L,
            status = "COMPLETED",
            startedAtMs = now - 10_000L,
            stoppedAtMs = now,
        )
        assertTrue(bridge.forwardNearbyToBle(completedEnvelope, completedEmergency))

        // 2. Delayed active packet arrives on BLE
        val delayedActiveBle = createBleSosPacket(emergencyId = "em-901", sequence = 1L, status = BleSosBridge.SOS_STATUS_ACTIVE, startedAtMs = now - 15_000L)
        val delayedEvent = bleSosBridge.toIncidentEvent(delayedActiveBle)!!

        val resurrected = bridge.forwardBleToNearby(delayedActiveBle, delayedEvent)
        assertFalse("Delayed active update must never resurrect completed emergency", resurrected)
    }

    // --- Scenario 10: Durable outbox recovery after engine recreation ---

    @Test
    fun `10 Durable outbox envelopes survive engine recreation when no peers are reachable`() {
        // Setup engine with no reachable peers
        val disconnectedTransport = FakeRelayTransport(mutableListOf())
        val queue = FakeRelayOutboundQueue()
        val engine1 = DurableRelayEngine(
            delegate = RelayEngine(selfOriginId = selfDeviceId, transport = disconnectedTransport),
            store = FakeSeenEventStore(),
            outboundStore = queue,
        )

        val (envelope, _) = createNearbySosEnvelope(emergencyId = "em-1001", sequence = 1L)
        val decision = engine1.onEventReceived(envelope)
        assertEquals(RelayDecision.ACCEPTED_QUEUED, decision)
        assertEquals("Must be queued in outbound store", 1, queue.loadQueued().size)

        // Engine restart: recreate new engine over same queue
        val engine2 = DurableRelayEngine(
            delegate = RelayEngine(selfOriginId = selfDeviceId, transport = disconnectedTransport),
            store = FakeSeenEventStore(),
            outboundStore = queue,
        )

        // Active envelopes must be restored
        assertEquals(1, engine2.activeEnvelopes().size)
        assertEquals("em-1001", engine2.activeEnvelopes()[0].event.payload["emergencyId"])
    }

    // --- Scenario 11: Retry when a peer becomes available ---

    @Test
    fun `11 Queued envelopes are flushed to newly available peer and removed from queue`() {
        val disconnectedTransport = FakeRelayTransport(mutableListOf())
        val queue = FakeRelayOutboundQueue()
        val rawEngine = RelayEngine(selfOriginId = selfDeviceId, transport = disconnectedTransport)
        val engine = DurableRelayEngine(rawEngine, FakeSeenEventStore(), queue)

        val (envelope, _) = createNearbySosEnvelope(emergencyId = "em-1101", sequence = 1L)
        engine.onEventReceived(envelope)
        assertEquals(1, queue.loadQueued().size)

        // Peer becomes available
        disconnectedTransport.peers.add("new_peer_X")
        val delivered = engine.onPeerAvailable("new_peer_X")

        assertEquals(1, delivered)
        assertEquals(1, disconnectedTransport.sentPayloads.size)
        assertTrue("Delivered envelope must be removed from outbound store", queue.loadQueued().isEmpty())
    }

    // --- Scenario 12: Pure BLE and pure Nearby relay regression tests ---

    @Test
    fun `12 Pure BLE-to-BLE and pure Nearby-to-Nearby relay continue functioning`() {
        // Test pure BLE-to-BLE mesh propagation
        val blePacket = createBleSosPacket(emergencyId = "em-1201", sequence = 1L, ttl = 3)
        val bleDecision = bleMeshNode.ingest(blePacket.serialize(), fromPeerId = "ble_peer_in")
        assertEquals(GeoRescueBleRelayDecision.ACCEPTED_FORWARDED, bleDecision)
        assertEquals(1, bleSink.sentPackets.size)
        assertEquals(2, bleSink.sentPackets[0].ttl) // TTL decremented by 1

        // Test pure Nearby-to-Nearby relay propagation
        val (envelope, _) = createNearbySosEnvelope(emergencyId = "em-1202", sequence = 1L)
        val nearbyDecision = nearbyEngine.onEventReceived(envelope)
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, nearbyDecision)
        assertEquals(1, nearbyTransport.sentPayloads.size)
    }

    // =========================================================================
    // --- Regression Tests for Phase P1.2 Safety Audit Corrections ---
    // =========================================================================

    // --- Fix 1: Hop Budget and Expiry ---

    @Test
    fun `Fix 1 - BLE to Nearby to BLE preserves and decrements remaining budget`() {
        val blePacket = createBleSosPacket(emergencyId = "em-fix1a", sequence = 1L, ttl = 4)
        val event = bleSosBridge.toIncidentEvent(blePacket)!!

        // Hop 1: BLE -> Nearby
        assertTrue(bridge.forwardBleToNearby(blePacket, event))
        val sentNearby = nearbyTransport.sentPayloads.last()
        assertTrue(sentNearby.contains("remainingHops=3"))

        // Hop 2: Nearby -> BLE
        val (envelope, emergency) = createNearbySosEnvelope(
            emergencyId = "em-fix1a-return",
            sequence = 1L,
            remainingHops = 3
        )
        assertTrue(bridge.forwardNearbyToBle(envelope, emergency))
        val sentBle = bleSink.sentPackets.last()
        assertEquals(2, sentBle.ttl)
        assertEquals("2", sentBle.payload[CrossTransportRelayBridge.KEY_REMAINING_HOPS])
    }

    @Test
    fun `Fix 1 - Repeated alternating transport conversions cannot increase budget`() {
        // Initial Nearby envelope with budget 2
        val (env1, em1) = createNearbySosEnvelope(emergencyId = "em-fix1b", sequence = 1L, remainingHops = 2)
        assertTrue(bridge.forwardNearbyToBle(env1, em1))
        val ble1 = bleSink.sentPackets.last()
        assertEquals(1, ble1.ttl)

        // BLE -> Nearby with ttl 1 cannot be forwarded (remainingHops would be 0)
        val ev1 = bleSosBridge.toIncidentEvent(ble1)!!
        assertFalse("Packet with ttl 1 cannot make another hop to Nearby", bridge.forwardBleToNearby(ble1, ev1))
    }

    @Test
    fun `Fix 1 - Zero or exhausted hop budget prevents forwarding`() {
        val exhaustedBle = createBleSosPacket(emergencyId = "em-fix1c", sequence = 1L, ttl = 1)
        val event = bleSosBridge.toIncidentEvent(exhaustedBle)!!
        assertFalse("Exhausted BLE packet must be rejected", bridge.forwardBleToNearby(exhaustedBle, event))

        val (exhaustedNearby, emergency) = createNearbySosEnvelope(emergencyId = "em-fix1c-nearby", sequence = 1L, remainingHops = 1)
        assertFalse("Nearby envelope with remainingHops=1 cannot bridge to BLE (next ttl=0)", bridge.forwardNearbyToBle(exhaustedNearby, emergency))
    }

    @Test
    fun `Fix 1 - Expired BLE and Nearby events are rejected before persistence`() {
        val inMemoryStore = InMemorySosStore()
        val expiredTime = System.currentTimeMillis() - 7_200_000L // 2 hours ago (TTL is 1 hour)

        val expiredBle = createBleSosPacket(emergencyId = "em-fix1d", sequence = 1L, startedAtMs = expiredTime)
        val bleEvent = bleSosBridge.toIncidentEvent(expiredBle)!!
        val processedBle = SosIncomingEventCoordinator.processIncomingSos(bleEvent, inMemoryStore, nowMs = System.currentTimeMillis())
        assertNull("Expired BLE event must be rejected before persistence", processedBle)
        assertTrue(inMemoryStore.history.isEmpty())

        val (expiredNearbyEnv, _) = createNearbySosEnvelope(emergencyId = "em-fix1d-nearby", sequence = 1L, startedAtMs = expiredTime)
        val processedNearby = SosIncomingEventCoordinator.processIncomingSos(expiredNearbyEnv.event, inMemoryStore, nowMs = System.currentTimeMillis())
        assertNull("Expired Nearby event must be rejected before persistence", processedNearby)
        assertTrue(inMemoryStore.history.isEmpty())
    }

    @Test
    fun `Fix 1 - Original expiry is not refreshed by transport conversion`() {
        val now = System.currentTimeMillis()
        val pastStart = now - 3_590_000L // 59 minutes and 50 seconds ago (10s remaining until 1h expiry)
        val (envelope, emergency) = createNearbySosEnvelope(
            emergencyId = "em-fix1e",
            sequence = 1L,
            startedAtMs = pastStart,
            remainingHops = 5
        )

        // Forward Nearby -> BLE
        assertTrue(bridge.forwardNearbyToBle(envelope, emergency))
        val sentBle = bleSink.sentPackets.last()
        assertEquals(pastStart, sentBle.timestampMs) // Original occurrence timestamp is preserved

        // Advance fake clock past the 1-hour cutoff
        val futureNow = now + 20_000L // now 1 hour and 10 seconds past startedAtMs
        val timedBridge = CrossTransportRelayBridge(
            blePublisher = { bleMeshNode.publish(it) },
            relayEngine = nearbyEngine,
            selfOriginId = { selfDeviceId },
            deduplicator = deduplicator,
            nowMs = { futureNow },
        )
        assertFalse("Event past absolute expiry must be rejected for forwarding", timedBridge.forwardNearbyToBle(envelope, emergency))
    }

    // --- Fix 2: Loop Suppression & Emergency Isolation ---

    @Test
    fun `Fix 2 - Separate emergencies from same origin do not interfere`() {
        val em1Packet = createBleSosPacket(emergencyId = "em-201a", sequence = 1L)
        val em1Event = bleSosBridge.toIncidentEvent(em1Packet)!!
        assertTrue(bridge.forwardBleToNearby(em1Packet, em1Event))

        // Same origin, different emergency ID
        val em2Packet = createBleSosPacket(emergencyId = "em-201b", sequence = 1L)
        val em2Event = bleSosBridge.toIncidentEvent(em2Packet)!!
        assertTrue("Separate emergency with same origin must be accepted independently", bridge.forwardBleToNearby(em2Packet, em2Event))
    }

    @Test
    fun `Fix 2 - Concurrent attempts to forward same sequence cannot both pass`() {
        val blePacket = createBleSosPacket(emergencyId = "em-202a", sequence = 1L)
        val event = bleSosBridge.toIncidentEvent(blePacket)!!
        val (nearbyEnv, emergency) = createNearbySosEnvelope(emergencyId = "em-202a", sequence = 1L)

        val executor = Executors.newFixedThreadPool(2)
        val passedCount = AtomicInteger(0)

        val task1 = Runnable {
            if (bridge.forwardBleToNearby(blePacket, event)) passedCount.incrementAndGet()
        }
        val task2 = Runnable {
            if (bridge.forwardNearbyToBle(nearbyEnv, emergency)) passedCount.incrementAndGet()
        }

        executor.execute(task1)
        executor.execute(task2)
        executor.shutdown()
        executor.awaitTermination(5, TimeUnit.SECONDS)

        assertEquals("Exactly one direction can pass during race for identical sequence", 1, passedCount.get())
    }

    // --- Fix 3: RelayEngine Lifecycle Deduplication ---

    @Test
    fun `Fix 3 - Same event ID identical sequence is rejected as duplicate`() {
        val (env1, _) = createNearbySosEnvelope(emergencyId = "em-301a", sequence = 1L)
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, nearbyEngine.onEventReceived(env1))
        val secondDecision = nearbyEngine.onEventReceived(env1)
        assertTrue("Duplicate identical sequence must be rejected",
            secondDecision == RelayDecision.DUPLICATE || secondDecision == RelayDecision.STALE_SEQUENCE)

        // For local events, seenEventIds directly enforces DUPLICATE
        val (localEnv, _) = createNearbySosEnvelope(emergencyId = "em-301b", sequence = 1L)
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, nearbyEngine.onLocalEvent(localEnv.event, localEnv.payload))
        assertEquals("Duplicate local event must be rejected as DUPLICATE",
            RelayDecision.DUPLICATE, nearbyEngine.onLocalEvent(localEnv.event, localEnv.payload))
    }

    @Test
    fun `Fix 3 - Separate emergencies with same origin remain independent in RelayEngine`() {
        val (envA, _) = createNearbySosEnvelope(emergencyId = "em-302a", sequence = 5L)
        assertEquals(RelayDecision.ACCEPTED_FORWARDED, nearbyEngine.onEventReceived(envA))

        // Emergency B from same victim device with sequence 1 (lower than 5)
        val (envB, _) = createNearbySosEnvelope(emergencyId = "em-302b", sequence = 1L)
        assertEquals("Independent emergency with sequence 1 must not be rejected by emergency A's sequence 5",
            RelayDecision.ACCEPTED_FORWARDED, nearbyEngine.onEventReceived(envB))
    }

    // --- Fix 4: Durable Outbound Queue Delivery ---

    @Test
    fun `Fix 4 - Failed delivery retains outbox entry for retry`() {
        val failingTransport = object : RelayTransport {
            override fun availablePeers(): List<String> = listOf("failing_peer")
            override fun send(peerId: String, payload: String): Boolean = false // Simulate transport failure
        }
        val queue = FakeRelayOutboundQueue()
        val raw = RelayEngine(selfOriginId = selfDeviceId, transport = failingTransport)
        val engine = DurableRelayEngine(raw, FakeSeenEventStore(), queue)

        val (env, _) = createNearbySosEnvelope(emergencyId = "em-401a", sequence = 1L)
        engine.onEventReceived(env)
        assertEquals(1, queue.loadQueued().size)

        // Peer available attempt fails send
        val delivered = engine.onPeerAvailable("failing_peer")
        assertEquals(0, delivered)
        assertEquals("Entry must remain in outbox after failed delivery", 1, queue.loadQueued().size)
    }

    @Test
    fun `Fix 4 - Newer sequence sharing event ID is not lost when queued`() {
        val disconnected = FakeRelayTransport(mutableListOf())
        val queue = FakeRelayOutboundQueue()
        val raw = RelayEngine(selfOriginId = selfDeviceId, transport = disconnected)
        val engine = DurableRelayEngine(raw, FakeSeenEventStore(), queue)

        val (env1, _) = createNearbySosEnvelope(emergencyId = "em-402a", sequence = 1L)
        val (env2, _) = createNearbySosEnvelope(emergencyId = "em-402a", sequence = 2L)

        engine.onEventReceived(env1)
        engine.onEventReceived(env2)

        val queued = queue.loadQueued()
        assertEquals("Both distinct sequences must be queued without overwriting", 2, queued.size)
        assertTrue(queued.any { it.event.sequence == 1L })
        assertTrue(queued.any { it.event.sequence == 2L })
    }

    @Test
    fun `Fix 4 - Recreating engine does not retransmit already successfully delivered queued entries`() {
        val transport = FakeRelayTransport(mutableListOf("peer_1"))
        val queue = FakeRelayOutboundQueue()
        val raw1 = RelayEngine(selfOriginId = selfDeviceId, transport = transport)
        val engine1 = DurableRelayEngine(raw1, FakeSeenEventStore(), queue)

        val (env, _) = createNearbySosEnvelope(emergencyId = "em-403a", sequence = 1L)
        engine1.onEventReceived(env) // Successfully delivered immediately
        assertTrue("Delivered entry must not be in outbox", queue.loadQueued().isEmpty())

        // Recreate engine with same stores
        val raw2 = RelayEngine(selfOriginId = selfDeviceId, transport = transport)
        val engine2 = DurableRelayEngine(raw2, FakeSeenEventStore(), queue)
        transport.sentPayloads.clear()

        // New peer connects
        engine2.onPeerAvailable("peer_2")
        assertTrue("Already delivered envelope should not retransmit on reboot when peer appears",
            transport.sentPayloads.isEmpty())
    }

    // --- Fix 5: Atomic Concurrent SOS Ingestion ---

    @Test
    fun `Fix 5 - Concurrent BLE and Nearby ingestion produces one accepted update one notification and one sync`() {
        val store = InMemorySosStore()
        val blePacket = createBleSosPacket(emergencyId = "em-501a", sequence = 1L)
        val bleEvent = bleSosBridge.toIncidentEvent(blePacket)!!
        val (nearbyEnv, _) = createNearbySosEnvelope(emergencyId = "em-501a", sequence = 1L)

        val notificationCount = AtomicInteger(0)
        val syncPendingCount = AtomicInteger(0)
        val acceptedCount = AtomicInteger(0)

        val executor = Executors.newFixedThreadPool(2)

        val taskBle = Runnable {
            val emergency = SosIncomingEventCoordinator.processIncomingSos(bleEvent, store)
            if (emergency != null) {
                acceptedCount.incrementAndGet()
                notificationCount.incrementAndGet()
                syncPendingCount.incrementAndGet()
            }
        }

        val taskNearby = Runnable {
            val emergency = SosIncomingEventCoordinator.processIncomingSos(nearbyEnv.event, store)
            if (emergency != null) {
                acceptedCount.incrementAndGet()
                notificationCount.incrementAndGet()
                syncPendingCount.incrementAndGet()
            }
        }

        executor.execute(taskBle)
        executor.execute(taskNearby)
        executor.shutdown()
        executor.awaitTermination(5, TimeUnit.SECONDS)

        assertEquals("Exactly one coordinator ingestion must succeed", 1, acceptedCount.get())
        assertEquals("Exactly one heads-up notification must be triggered", 1, notificationCount.get())
        assertEquals("Exactly one sync pending action must be triggered", 1, syncPendingCount.get())
    }

    @Test
    fun `Fix 5 - Persistence failure does not trigger notification sync or forwarding`() {
        val failingStore = object : LocalSosStore {
            override fun loadActive(): SosEmergency? = null
            override fun saveActive(emergency: SosEmergency) {}
            override fun clearActive() {}
            override fun loadHistory(): List<SosEmergency> = emptyList()
            override fun addToHistory(emergency: SosEmergency): Boolean = false // Simulate write failure
            override fun replaceInHistory(replacedId: String, emergency: SosEmergency): Boolean = false
        }

        val blePacket = createBleSosPacket(emergencyId = "em-502a", sequence = 1L)
        val event = bleSosBridge.toIncidentEvent(blePacket)!!
        val emergency = SosIncomingEventCoordinator.processIncomingSos(event, failingStore)

        assertNull("Persistence failure must return null from processIncomingSos", emergency)
    }

    // --- Fake Test Implementations ---

    private class FakeRelayTransport(val peers: MutableList<String>) : RelayTransport {
        val sentPayloads = mutableListOf<String>()
        override fun availablePeers(): List<String> = peers.toList()
        override fun send(peerId: String, payload: String): Boolean {
            sentPayloads.add(payload)
            return true
        }
    }

    private class FakeSeenEventStore : SeenEventStore {
        val set = mutableSetOf<String>()
        override fun loadAllSeenEventIds(): Set<String> = set.toSet()
        override fun saveSeenEventId(eventId: String, timestampMs: Long) { set.add(eventId) }
        override fun clearExpiredBefore(cutoffMs: Long) {}
    }

    private class FakeRelayOutboundQueue : RelayOutboundQueue {
        val rows = LinkedHashMap<String, RelayEnvelope>()
        override fun loadQueued(): List<RelayEnvelope> = rows.values.toList()
        override fun enqueue(envelope: RelayEnvelope, queuedAtMs: Long) {
            val key = if (envelope.event.sequence > 0L) "${envelope.event.eventId}:${envelope.event.sequence}" else envelope.event.eventId
            rows[key] = envelope
        }
        override fun remove(eventId: String) {
            rows.remove(eventId)
            val matching = rows.keys.filter { it == eventId || it.startsWith("$eventId:") }.toList()
            matching.forEach { rows.remove(it) }
        }
    }

    private class FakeBlePacketSink(val peers: MutableList<String>) : GeoRescueBlePacketSink {
        val sentPackets = mutableListOf<GeoRescueBlePacket>()
        override fun availablePeerIds(): List<String> = peers.toList()
        override fun send(peerId: String, packetJson: String): Boolean {
            com.example.georescux.domain.ble.GeoRescueBlePacketCodec.deserialize(packetJson)?.let {
                sentPackets.add(it)
            }
            return true
        }
    }

    private class FakeBleRepository : com.example.georescux.domain.ble.GeoRescueBleSeenPacketStore,
        com.example.georescux.domain.ble.GeoRescueBleOutboundPacketQueue {
        val seen = mutableSetOf<String>()
        val queued = LinkedHashMap<String, QueuedPacket>()
        override fun loadAllSeenPacketIds(): Set<String> = seen.toSet()
        override fun saveSeenPacketId(packetId: String, timestampMs: Long) { seen.add(packetId) }
        override fun clearExpiredBefore(cutoffMs: Long) {}
        override fun loadQueued(): List<QueuedPacket> = queued.values.toList()
        override fun enqueue(packetId: String, packetJson: String, queuedAtMs: Long) {
            queued[packetId] = QueuedPacket(packetId, packetJson, queuedAtMs, 0)
        }
        override fun remove(packetId: String) { queued.remove(packetId) }
    }

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
        override fun replaceInHistory(replacedId: String, emergency: SosEmergency): Boolean {
            val idx = history.indexOfFirst { it.id == replacedId }
            if (idx >= 0) {
                history[idx] = emergency
                return true
            }
            history.add(emergency)
            return true
        }
    }
}
