package com.example.georescux.data.relay

import com.example.georescux.domain.incident.IncidentEvent
import com.example.georescux.domain.relay.RelayDecision
import com.example.georescux.domain.relay.RelayEngine
import com.example.georescux.domain.relay.RelayEnvelope

/**
 * Decorator around [RelayEngine] that integrates durable seen-event persistence ([SeenEventStore])
 * and optional durable outbound store-and-forward queueing ([RelayOutboundQueue]).
 * Restores seen event state and pending outbound envelopes on startup so deduplication and
 * delivery retry survive process death, and persists accepted events to disk.
 */
class DurableRelayEngine(
    private val delegate: RelayEngine,
    private val store: SeenEventStore,
    private val outboundStore: RelayOutboundQueue? = null,
) : RelayEngine(
    selfOriginId = delegate.selfOriginId,
    transport = delegate.transport,
) {

    init {
        restoreState()
    }

    private fun eventKey(event: IncidentEvent): String =
        if (event.sequence > 0L) "${event.eventId}:${event.sequence}" else event.eventId

    private fun restoreState() {
        // FIX Bug 5: call seedSeenEventId() directly instead of routing through onEventReceived
        // with a dummy IncidentEvent.  The old approach ran the ReplayGuard sequence check,
        // which returned STALE_SEQUENCE for any restored event after the first (they all had
        // sequence=0), so those event IDs were never added to seenEventIds and could be
        // re-broadcast to peers after every process restart.
        store.loadAllSeenEventIds().forEach { eventId ->
            delegate.seedSeenEventId(eventId)
        }
        outboundStore?.loadQueued()?.forEach { envelope ->
            val key = eventKey(envelope.event)
            if (!delegate.isDelivered(key)) {
                delegate.seedActiveEnvelope(envelope)
            }
        }
    }

    override fun onLocalEvent(event: IncidentEvent, payload: String): RelayDecision {
        val decision = delegate.onLocalEvent(event, payload)
        if (decision == RelayDecision.ACCEPTED_FORWARDED || decision == RelayDecision.ACCEPTED_QUEUED) {
            val key = eventKey(event)
            store.saveSeenEventId(key)
            if (decision == RelayDecision.ACCEPTED_QUEUED) {
                outboundStore?.enqueue(RelayEnvelope(event, payload))
            }
        }
        return decision
    }

    override fun onEventReceived(envelope: RelayEnvelope): RelayDecision {
        val decision = delegate.onEventReceived(envelope)
        if (decision == RelayDecision.ACCEPTED_FORWARDED || decision == RelayDecision.ACCEPTED_QUEUED) {
            val key = eventKey(envelope.event)
            store.saveSeenEventId(key)
            if (decision == RelayDecision.ACCEPTED_QUEUED) {
                outboundStore?.enqueue(envelope)
            }
        }
        return decision
    }

    override fun onPeerAvailable(peerId: String): Int {
        val count = delegate.onPeerAvailable(peerId)
        if (count > 0 && outboundStore != null) {
            outboundStore.loadQueued().forEach { queued ->
                val key = eventKey(queued.event)
                if (delegate.isDelivered(key)) {
                    outboundStore.remove(key)
                }
            }
        }
        return count
    }

    override fun pendingCountFor(peerId: String): Int = delegate.pendingCountFor(peerId)

    override fun seedSeenEventId(eventId: String) {
        delegate.seedSeenEventId(eventId)
    }

    override fun seedActiveEnvelope(envelope: RelayEnvelope) {
        delegate.seedActiveEnvelope(envelope)
    }

    override fun activeEnvelopes(): List<RelayEnvelope> = delegate.activeEnvelopes()
}
