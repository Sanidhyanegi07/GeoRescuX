package com.example.georescux.domain.routing

import kotlin.math.asin
import kotlin.math.cos
import kotlin.math.pow
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * Finds the graph node nearest to a geographic position (Haversine).
 * Pure Kotlin; deterministic — equal distances resolve by node id.
 */
object NearestNode {

    fun find(graph: RouteGraph, latitude: Double, longitude: Double): RouteNode? {
        if (graph.nodes.isEmpty()) return null
        var bestNode: RouteNode? = null
        var bestDist = Double.MAX_VALUE
        for (node in graph.nodes) {
            val d = haversineMeters(latitude, longitude, node.latitude, node.longitude)
            if (d < bestDist || (d == bestDist && (bestNode == null || node.id < bestNode.id))) {
                bestDist = d
                bestNode = node
            }
        }
        return bestNode
    }

    private fun haversineMeters(lat1: Double, lng1: Double, lat2: Double, lng2: Double): Double {
        val earthRadiusMeters = 6_371_000.0
        val degToRad = Math.PI / 180.0
        val dLat = (lat2 - lat1) * degToRad
        val dLng = (lng2 - lng1) * degToRad
        val sLat = sin(dLat / 2)
        val sLng = sin(dLng / 2)
        val halfChord = sLat * sLat +
            cos(lat1 * degToRad) * cos(lat2 * degToRad) * (sLng * sLng)
        return 2 * earthRadiusMeters * asin(sqrt(halfChord.coerceIn(0.0, 1.0)))
    }
}
