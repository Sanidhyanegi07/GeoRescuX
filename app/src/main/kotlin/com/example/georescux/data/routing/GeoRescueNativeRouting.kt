package com.example.georescux.data.routing

import com.example.georescux.domain.routing.EvacuationRoute
import com.example.georescux.domain.routing.RoadHazard
import com.example.georescux.domain.routing.RouteGraph
import com.example.georescux.domain.routing.SafeHavenFallback
import org.json.JSONObject

/**
 * Interface and JNI loader for the C++ offline routing engine.
 *
 * Safe-fallback invariant:
 * - Attempts to load `libgeorescux_routing.so` via [System.loadLibrary].
 * - If the native library is missing, incompatible, or fails to link (e.g. on
 *   devices or test environments without NDK binaries), it gracefully catches
 *   [Throwable] and sets [isAvailable] to false.
 * - When [isAvailable] is false or any native error occurs, routing immediately
 *   and safely falls back to pure Kotlin [SafeHavenFallback], guaranteeing that
 *   Safe Route Pro never crashes.
 */
object GeoRescueNativeRouting {

    private val isNativeLoaded: Boolean = try {
        System.loadLibrary("georescux_routing")
        true
    } catch (_: Throwable) {
        false
    }

    val isAvailable: Boolean
        get() = isNativeLoaded && try {
            nativeIsAvailable()
        } catch (_: Throwable) {
            false
        }

    @JvmStatic
    private external fun nativeIsAvailable(): Boolean

    @JvmStatic
    private external fun nativeFindRoute(graphJson: String, startId: String, destId: String): String

    @JvmStatic
    private external fun nativeFindBidirectionalRoute(graphJson: String, startId: String, destId: String): String

    @JvmStatic
    private external fun nativeHaversineDistance(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double

    @JvmStatic
    private external fun nativeVincentyDistance(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double

    @JvmStatic
    private external fun nativeElevationImpedance(fromElev: Double, toElev: Double, distanceMeters: Double, injured: Boolean): Double

    @JvmStatic
    private external fun nativeEvaluateTriage(
        canWalk: Boolean,
        breathingSpontaneously: Boolean,
        breathingAfterAirway: Boolean,
        respirationRate: Int,
        capillaryRefill: Double,
        radialPulse: Boolean,
        followsCommands: Boolean,
        hasHemorrhage: Boolean,
        isTrapped: Boolean
    ): String

    @JvmStatic
    private external fun nativeDeduplicatePacket(senderHash: Int, seqNumber: Int): Boolean

    @JvmStatic
    private external fun nativeEncodeSosPacket(
        priority: Int,
        ttl: Int,
        seqNum: Int,
        senderId: String,
        lat: Double,
        lon: Double,
        alt: Double,
        batteryPct: Int,
        urgency: Int,
        peopleCount: Int,
        timestampSec: Long
    ): ByteArray

    @JvmStatic
    private external fun nativeHmacSha256(key: String, message: String): String

    private var cachedGraphKey: Int? = null
    private var cachedGraphJson: String? = null
    private val graphCacheLock = Any()

    internal fun getSerializedGraphJson(graph: RouteGraph): String {
        val key = graph.hashCode()
        synchronized(graphCacheLock) {
            if (cachedGraphKey == key && cachedGraphJson != null) {
                return cachedGraphJson!!
            }
            val json = serializeGraphToJson(graph)
            cachedGraphKey = key
            cachedGraphJson = json
            return json
        }
    }

    /**
     * Executes route finding using the native C++ engine if available, or
     * falls back to the pure Kotlin [SafeHavenFallback] engine.
     */
    fun findRoute(graph: RouteGraph, startId: String, destinationId: String): EvacuationRoute? {
        if (!isAvailable) {
            return SafeHavenFallback.findRoute(graph, startId, destinationId)
        }

        return try {
            val json = getSerializedGraphJson(graph)
            val resultJson = nativeFindRoute(json, startId, destinationId)
            parseNativeResult(resultJson, graph) ?: SafeHavenFallback.findRoute(graph, startId, destinationId)
        } catch (_: Throwable) {
            SafeHavenFallback.findRoute(graph, startId, destinationId)
        }
    }

    /**
     * Executes high-speed bidirectional route finding using native C++ Bidirectional A*.
     */
    fun findBidirectionalRoute(graph: RouteGraph, startId: String, destinationId: String): EvacuationRoute? {
        if (!isAvailable) {
            return SafeHavenFallback.findRoute(graph, startId, destinationId)
        }

        return try {
            val json = getSerializedGraphJson(graph)
            val resultJson = nativeFindBidirectionalRoute(json, startId, destinationId)
            parseNativeResult(resultJson, graph) ?: SafeHavenFallback.findRoute(graph, startId, destinationId)
        } catch (_: Throwable) {
            SafeHavenFallback.findRoute(graph, startId, destinationId)
        }
    }

    /**
     * High-precision geodesic distance in meters (Vincenty / WGS84).
     */
    fun computeGeodesicDistance(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        return if (isAvailable) {
            try {
                nativeVincentyDistance(lat1, lon1, lat2, lon2)
            } catch (_: Throwable) {
                nativeHaversineDistance(lat1, lon1, lat2, lon2)
            }
        } else {
            val r = 6371000.0
            val dLat = Math.toRadians(lat2 - lat1)
            val dLon = Math.toRadians(lon2 - lon1)
            val a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
                    Math.cos(Math.toRadians(lat1)) * Math.cos(Math.toRadians(lat2)) *
                    Math.sin(dLon / 2) * Math.sin(dLon / 2)
            val c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))
            r * c
        }
    }

    /**
     * Native packet deduplication using Bloom filter.
     */
    fun isDuplicatePacket(senderHash: Int, sequenceNumber: Int): Boolean {
        if (!isAvailable) return false
        return try {
            nativeDeduplicatePacket(senderHash, sequenceNumber)
        } catch (_: Throwable) {
            false
        }
    }

    /**
     * Encodes a compact 20-31 byte BLE mesh packet natively.
     */
    fun encodeSosPacket(
        priority: Int, ttl: Int, seqNum: Int, senderId: String,
        lat: Double, lon: Double, alt: Double,
        batteryPct: Int, urgency: Int, peopleCount: Int,
        timestampSec: Long
    ): ByteArray? {
        if (!isAvailable) return null
        return try {
            nativeEncodeSosPacket(
                priority, ttl, seqNum, senderId,
                lat, lon, alt, batteryPct, urgency, peopleCount, timestampSec
            )
        } catch (_: Throwable) {
            null
        }
    }

    /**
     * Evaluates victim triage using native START engine.
     */
    fun evaluateTriage(
        canWalk: Boolean,
        breathing: Boolean,
        breathingAfterAirway: Boolean,
        respRate: Int,
        capRefill: Double,
        radialPulse: Boolean,
        followsCommands: Boolean,
        hasHemorrhage: Boolean,
        isTrapped: Boolean
    ): String? {
        if (!isAvailable) return null
        return try {
            nativeEvaluateTriage(
                canWalk, breathing, breathingAfterAirway,
                respRate, capRefill, radialPulse, followsCommands,
                hasHemorrhage, isTrapped
            )
        } catch (_: Throwable) {
            null
        }
    }

    /**
     * Authenticates packet payload using HMAC-SHA256.
     */
    fun computeHmacSha256(key: String, message: String): String? {
        if (!isAvailable) return null
        return try {
            nativeHmacSha256(key, message)
        } catch (_: Throwable) {
            null
        }
    }

    internal fun serializeGraphToJson(graph: RouteGraph): String {
        val root = JSONObject()
        val region = JSONObject()
        region.put("version", graph.version)
        root.put("region", region)

        val nodesArr = org.json.JSONArray()
        for (node in graph.nodes) {
            val nObj = JSONObject()
            nObj.put("id", node.id)
            nObj.put("latitude", node.latitude)
            nObj.put("longitude", node.longitude)
            nObj.put("isSafeHaven", node.isSafeHaven)
            nodesArr.put(nObj)
        }
        root.put("nodes", nodesArr)

        val edgesArr = org.json.JSONArray()
        for (edge in graph.edges) {
            val eObj = JSONObject()
            eObj.put("fromNodeId", edge.fromNodeId)
            eObj.put("toNodeId", edge.toNodeId)
            eObj.put("distanceMeters", edge.distanceMeters)
            eObj.put("blocked", edge.blocked)
            edgesArr.put(eObj)
        }
        root.put("edges", edgesArr)

        val hazardsArr = org.json.JSONArray()
        for (hazard in graph.hazards) {
            val hObj = JSONObject()
            hObj.put("id", hazard.id)
            hObj.put("fromNodeId", hazard.fromNodeId)
            hObj.put("toNodeId", hazard.toNodeId)
            hObj.put("penaltyMeters", hazard.penaltyMeters)
            hazardsArr.put(hObj)
        }
        root.put("hazards", hazardsArr)

        return root.toString()
    }

    internal fun parseNativeResult(jsonStr: String, graph: RouteGraph): EvacuationRoute? {
        val obj = JSONObject(jsonStr)
        if (!obj.optBoolean("found", false)) return null

        val nodeIdsArray = obj.optJSONArray("nodeIds") ?: return null
        val nodeIds = mutableListOf<String>()
        for (i in 0 until nodeIdsArray.length()) {
            nodeIds.add(nodeIdsArray.getString(i))
        }

        val totalDist = obj.optDouble("totalDistanceMeters", 0.0)
        val totalCost = obj.optDouble("totalCostMeters", 0.0)
        val durationSec = obj.optLong("estimatedDurationSeconds", 0L)

        val hazards = mutableListOf<RoadHazard>()
        val hazardsArray = obj.optJSONArray("hazardWarnings")
        if (hazardsArray != null) {
            for (i in 0 until hazardsArray.length()) {
                val hObj = hazardsArray.getJSONObject(i)
                val hId = hObj.optString("id")
                graph.hazards.firstOrNull { it.id == hId }?.let { hazards.add(it) }
            }
        }

        return EvacuationRoute(
            nodeIds = nodeIds,
            totalDistanceMeters = totalDist,
            totalCostMeters = totalCost,
            hazardWarnings = hazards,
            estimatedDurationSeconds = durationSec,
        )
    }
}
