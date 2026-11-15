package com.example.georescux.data.relay

import com.example.georescux.domain.relay.RelayEnvelope

/**
 * Interface for durable outbound envelope store-and-forward queue surviving process restarts.
 */
interface RelayOutboundQueue {
    fun loadQueued(): List<RelayEnvelope>
    fun enqueue(envelope: RelayEnvelope, queuedAtMs: Long = System.currentTimeMillis())
    fun remove(eventId: String)
}
