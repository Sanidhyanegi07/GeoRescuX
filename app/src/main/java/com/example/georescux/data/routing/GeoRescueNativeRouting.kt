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

    /**
     * Executes route finding using the native C++ engine if available, or
     * falls back to the pure Kotlin [SafeHavenFallback] engine.
     */
    fun findRoute(graph: RouteGraph, startId: String, destinationId: String): EvacuationRoute? {
        if (!isAvailable) {
            return SafeHavenFallback.findRoute(graph, startId, destinationId)
        }

        return try {
            val json = serializeGraphToJson(graph)
            val resultJson = nativeFindRoute(json, startId, destinationId)
            parseNativeResult(resultJson, graph) ?: SafeHavenFallback.findRoute(graph, startId, destinationId)
        } catch (_: Throwable) {
            SafeHavenFallback.findRoute(graph, startId, destinationId)
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
