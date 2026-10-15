package com.example.georescux.core.nativebridge

import com.example.georescux.data.routing.GeoRescueNativeRouting
import com.example.georescux.domain.routing.EvacuationRoute
import com.example.georescux.domain.routing.RouteGraph
import org.json.JSONObject

/**
 * Enterprise Native C++ Facade for GeoRescuX.
 * Provides unified, thread-safe access to the high-performance C++20 subsystems:
 * - Spatial Indexing & Geodesics (Vincenty / Haversine / Elevation Cost)
 * - Low-Latency Evacuation Pathfinding (Bidirectional A* & Dynamic Graph Updates)
 * - BLE Mesh Networking (Bit-packed Codec & Rolling Bloom Filter Deduplication)
 * - Emergency Triage Severity Scoring (START Protocol)
 * - Cryptographic Distress Beacon Authentication (HMAC-SHA256)
 */
object GeoRescueNativeEngine {

    val isAvailable: Boolean
        get() = GeoRescueNativeRouting.isAvailable

    /**
     * Finds an optimal evacuation route using Bidirectional A* in native C++.
     */
    fun findFastEvacuationRoute(
        graph: RouteGraph,
        startNodeId: String,
        destinationNodeId: String
    ): EvacuationRoute? {
        return GeoRescueNativeRouting.findBidirectionalRoute(graph, startNodeId, destinationNodeId)
            ?: GeoRescueNativeRouting.findRoute(graph, startNodeId, destinationNodeId)
    }

    /**
     * Calculates the true ellipsoidal geodesic distance in meters (WGS-84).
     */
    fun getPreciseDistanceMeters(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        return GeoRescueNativeRouting.computeGeodesicDistance(lat1, lon1, lat2, lon2)
    }

    /**
     * Evaluates victim triage status and returns structured severity assessment.
     */
    fun assessVictimTriage(
        canWalk: Boolean,
        breathing: Boolean,
        breathingAfterAirway: Boolean,
        respirationRate: Int,
        capillaryRefillSec: Double,
        radialPulse: Boolean,
        followsCommands: Boolean,
        hasHemorrhage: Boolean,
        isTrapped: Boolean
    ): TriageAssessmentResult? {
        val jsonStr = GeoRescueNativeRouting.evaluateTriage(
            canWalk, breathing, breathingAfterAirway,
            respirationRate, capillaryRefillSec, radialPulse,
            followsCommands, hasHemorrhage, isTrapped
        ) ?: return null

        return try {
            val obj = JSONObject(jsonStr)
            TriageAssessmentResult(
                category = obj.optString("category", "YELLOW"),
                priorityScore = obj.optInt("priorityScore", 50),
                primaryReason = obj.optString("primaryReason", ""),
                recommendedIntervention = obj.optString("recommendedIntervention", "")
            )
        } catch (_: Throwable) {
            null
        }
    }

    /**
     * Evaluates packet uniqueness via native rolling Bloom filter to prevent BLE flood storms.
     */
    fun isMeshPacketDuplicate(senderId: String, sequenceNumber: Int): Boolean {
        // FNV-1a hash matching native codec
        var hash = 2166136261L
        for (b in senderId.toByteArray(Charsets.UTF_8)) {
            hash = hash xor (b.toLong() and 0xFF)
            hash = (hash * 16777619L) and 0xFFFFFFFFL
        }
        return GeoRescueNativeRouting.isDuplicatePacket(hash.toInt(), sequenceNumber)
    }

    /**
     * Signs distress payload with HMAC-SHA256 for tamper-proof verification.
     */
    fun signEmergencyPayload(secretKey: String, payload: String): String? {
        return GeoRescueNativeRouting.computeHmacSha256(secretKey, payload)
    }

    data class TriageAssessmentResult(
        val category: String,
        val priorityScore: Int,
        val primaryReason: String,
        val recommendedIntervention: String
    )
}
