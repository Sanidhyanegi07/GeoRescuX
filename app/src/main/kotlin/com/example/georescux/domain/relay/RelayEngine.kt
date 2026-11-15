package com.example.georescux.domain.relay

import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.security.AuthVerdict
import com.example.georescux.domain.security.EventAuthenticator
import com.example.georescux.domain.security.ReplayGuard
import java.util.Collections

/**
 * Phone-to-phone relay core — the pure, device-independent half of the
 * approved relay design: deduplication, TTL, sequence/regression handling,
 * malformed-event rejection and delivery bookkeeping.
 *
 * The TRANSPORT (BLE/Wi-Fi Direct) is intentionally an interface
 * ([RelayTransport]); no radio stack is implemented here, so this engine
 * is fully JVM-testable and never blocks or touches SOS activation.
 * Real peer discovery/connection requires device-level permission and
 * lifecycle validation and is NOT implemented in this pass.
 *
 * Guarantees (each covered by tests):
 * - an eventId is forwarded at most once (duplicate suppression);
 * - expired events (occurredAtMs + ttl) are never forwarded;
 * - malformed events are rejected;
 * - per-origin sequences are monotonic — stale/regressing sequences are
 *   not forwarded (out-of-order/replay protection);
 * - sends to unavailable peers are kept in an outbox and flushed exactly
 *   once when the peer returns;
 * - the engine never blocks: callers decide when to run it.
 */
enum class RelayDecision {
    ACCEPTED_FORWARDED, ACCEPTED_QUEUED, DUPLICATE, EXPIRED, MALFORMED, REJECTED_SIGNATURE, STALE_SEQUENCE, OWN_EVENT,
}

/** The transport boundary a real BLE/Wi-Fi-Direct implementation will fill. */
interface RelayTransport {
    fun availablePeers(): List<String>
    fun send(peerId: String, payload: String): Boolean
}

/** Wire form: the event plus its opaque (compact, pre-serialized) payload. */
data class RelayEnvelope(
    val event: IncidentEvent,
    val payload: String,
    val fromPeerId: String? = null,
)

open class RelayEngine(
    open val selfOriginId: String,
    open val transport: RelayTransport,
    private val replayGuard: ReplayGuard = ReplayGuard(),
    private val authenticator: com.example.georescux.domain.security.EventAuthenticator? = null,
    private val nowMs: () -> Long = System::currentTimeMillis,
) {

    private val lock = Any()
    private val seenEventIds = HashSet<String>()
    private val activeEnvelopes = Collections.synchronizedList(mutableListOf<RelayEnvelope>())
    private val outbox = HashMap<String, MutableList<RelayEnvelope>>()
    private val sentPeersMap = HashMap<String, MutableSet<String>>()

    /** A locally produced event enters the relay (e.g. SOS, injected hazard). */
    open fun onLocalEvent(event: IncidentEvent, payload: String = event.canonicalString()): RelayDecision =
        ingest(RelayEnvelope(event, payload), fromLocal = true)

    /** An event received from a peer. */
    open fun onEventReceived(envelope: RelayEnvelope): RelayDecision =
        ingest(envelope, fromLocal = false)

    /** Flush pending sends to a peer that became reachable again. */
    open fun onPeerAvailable(peerId: String): Int = synchronized(lock) {
        var delivered = 0
        val peerPending = outbox.remove(peerId) ?: emptyList()
        peerPending.forEach { envelope ->
            val key = dedupKey(envelope.event)
            val sentSet = sentPeersMap.getOrPut(key) { HashSet() }
            if (peerId !in sentSet) {
                if (transport.send(peerId, envelope.payload)) {
                    sentSet.add(peerId)
                    delivered++
                } else {
                    outbox.getOrPut(peerId) { mutableListOf() }.add(envelope)
                }
            }
        }

        val unexpired = activeEnvelopes.filter { !isExpired(it.event) && it.fromPeerId != peerId }
        unexpired.forEach { envelope ->
            val key = dedupKey(envelope.event)
            val sentSet = sentPeersMap.getOrPut(key) { HashSet() }
            if (peerId !in sentSet) {
                if (transport.send(peerId, envelope.payload)) {
                    sentSet.add(peerId)
                    delivered++
                } else {
                    outbox.getOrPut(peerId) { mutableListOf() }.add(envelope)
                }
            }
        }
        delivered
    }

    open fun pendingCountFor(peerId: String): Int = synchronized(lock) {
        outbox[peerId]?.size ?: 0
    }

    /** Returns true if the given event/sequence key has been confirmed sent to at least one peer. */
    open fun isDelivered(eventKey: String): Boolean = synchronized(lock) {
        sentPeersMap[eventKey]?.isNotEmpty() == true
    }

    /**
     * Directly marks [eventId] as seen without running it through the full ingest pipeline.
     * Used by [com.example.georescux.data.relay.DurableRelayEngine] to restore persisted
     * seen-event state on startup without triggering the [ReplayGuard] sequence check
     * (which would produce [RelayDecision.STALE_SEQUENCE] for any event with sequence ≤ 0).
     */
    open fun seedSeenEventId(eventId: String) {
        synchronized(lock) {
            seenEventIds.add(eventId)
        }
    }

    /**
     * Restores an active unexpired envelope into the relay engine queue across restarts.
     */
    open fun seedActiveEnvelope(envelope: RelayEnvelope) {
        synchronized(lock) {
            val key = dedupKey(envelope.event)
            if (!isExpired(envelope.event) && activeEnvelopes.none { dedupKey(it.event) == key }) {
                activeEnvelopes.add(envelope)
            }
        }
    }

    open fun activeEnvelopes(): List<RelayEnvelope> = synchronized(lock) {
        activeEnvelopes.toList()
    }

    fun dedupKey(event: IncidentEvent): String =
        if (event.sequence > 0L) "${event.eventId}:${event.sequence}" else event.eventId

    private fun ingest(envelope: RelayEnvelope, fromLocal: Boolean): RelayDecision = synchronized(lock) {
        val event = envelope.event
        if (event.eventId.isBlank() || event.originId.isBlank() || event.occurredAtMs <= 0 || event.ttlSeconds < 0) {
            return RelayDecision.MALFORMED
        }
        if (!fromLocal && event.originId == selfOriginId) return RelayDecision.OWN_EVENT
        if (isExpired(event)) return RelayDecision.EXPIRED
        authenticator?.let { auth ->
            if (auth.authenticate(event, nowMs()) != AuthVerdict.OK) {
                return RelayDecision.REJECTED_SIGNATURE
            }
        }
        val emergencyId = event.payload["emergencyId"]
        if (!fromLocal && !replayGuard.accept(event.originId, event.sequence, streamId = emergencyId)) {
            return RelayDecision.STALE_SEQUENCE
        }
        val key = dedupKey(event)
        if (!seenEventIds.add(key)) return RelayDecision.DUPLICATE
        
        envelope.fromPeerId?.let { sender ->
            sentPeersMap.getOrPut(key) { HashSet() }.add(sender)
        }
        
        forward(envelope)
    }

    private fun forward(envelope: RelayEnvelope): RelayDecision {
        activeEnvelopes.add(envelope)
        var anySuccess = false
        val key = dedupKey(envelope.event)
        val sentSet = sentPeersMap.getOrPut(key) { HashSet() }
        val peers = transport.availablePeers().filter { it != envelope.fromPeerId && it !in sentSet }
        if (peers.isNotEmpty()) {
            peers.forEach { peer ->
                if (transport.send(peer, envelope.payload)) {
                    sentSet.add(peer)
                    anySuccess = true
                } else {
                    outbox.getOrPut(peer) { mutableListOf() }.add(envelope)
                }
            }
        }
        return if (anySuccess) RelayDecision.ACCEPTED_FORWARDED else RelayDecision.ACCEPTED_QUEUED
    }

    private fun isExpired(event: IncidentEvent): Boolean =
        com.example.georescux.data.relay.CrossTransportRelayBridge.isEventExpired(
            event.occurredAtMs,
            event.ttlSeconds,
            nowMs()
        )
}
