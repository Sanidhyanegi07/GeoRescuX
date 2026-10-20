package com.example.georescux.data.routing

import com.example.georescux.domain.routing.RoadHazard
import com.example.georescux.domain.routing.RouteEdge
import com.example.georescux.domain.routing.RouteGraph
import com.example.georescux.domain.routing.RouteNode
import com.example.georescux.domain.routing.SafeHavenFallback
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Tests for [GeoRescueNativeRouting]:
 * - Safe fallback to Kotlin engine when native .so is absent
 * - Graph JSON serialization format matching RegionGraphLoader schema
 * - Parsing of native result format
 * - Consistency with SafeHavenFallback semantics
 */
class GeoRescueNativeRoutingTest {

    private fun createTestGraph(): RouteGraph {
        val nodes = listOf(
            RouteNode("A", 28.6300, 77.2150),
            RouteNode("B", 28.6320, 77.2180),
            RouteNode("C", 28.6340, 77.2200, isSafeHaven = true),
            RouteNode("D", 28.6350, 77.2250, isSafeHaven = true)
        )
        val edges = listOf(
            RouteEdge("A", "C", distanceMeters = 100.0),
            RouteEdge("A", "B", distanceMeters = 60.0),
            RouteEdge("B", "C", distanceMeters = 60.0),
            RouteEdge("C", "D", distanceMeters = 50.0)
        )
        val hazards = listOf(
            RoadHazard("h1", "A", "C", penaltyMeters = 20.0)
        )
        return RouteGraph(nodes = nodes, edges = edges, hazards = hazards, version = 1)
    }

    @Test
    fun `fallback safely executes and produces valid route when native is unavailable`() {
        val graph = createTestGraph()
        val route = GeoRescueNativeRouting.findRoute(graph, "A", "D")

        assertNotNull(route)
        assertEquals(listOf("A", "C", "D"), route!!.nodeIds)
        assertEquals(150.0, route.totalDistanceMeters, 0.001)
    }

    @Test
    fun `graph serialization produces valid JSON schema with nodes edges and hazards`() {
        val graph = createTestGraph()
        val json = GeoRescueNativeRouting.serializeGraphToJson(graph)

        assertTrue(json.contains("\"nodes\":"))
        assertTrue(json.contains("\"edges\":"))
        assertTrue(json.contains("\"hazards\":"))
        assertTrue(json.contains("\"version\":1"))
        assertTrue(json.contains("\"id\":\"A\""))
        assertTrue(json.contains("\"fromNodeId\":\"A\""))
        assertTrue(json.contains("\"toNodeId\":\"C\""))
        assertTrue(json.contains("\"penaltyMeters\":20"))
    }

    @Test
    fun `parseNativeResult correctly reconstructs EvacuationRoute from JSON`() {
        val graph = createTestGraph()
        val nativeJson = """
            {
                "found": true,
                "nodeIds": ["A", "B", "C"],
                "totalDistanceMeters": 120.0,
                "totalCostMeters": 120.0,
                "estimatedDurationSeconds": 60,
                "hazardWarnings": [{"id": "h1", "penaltyMeters": 20.0}]
            }
        """.trimIndent()

        val parsed = GeoRescueNativeRouting.parseNativeResult(nativeJson, graph)
        assertNotNull(parsed)
        assertEquals(listOf("A", "B", "C"), parsed!!.nodeIds)
        assertEquals(120.0, parsed.totalDistanceMeters, 0.001)
        assertEquals(120.0, parsed.totalCostMeters, 0.001)
        assertEquals(60L, parsed.estimatedDurationSeconds)
        assertEquals(1, parsed.hazardWarnings.size)
        assertEquals("h1", parsed.hazardWarnings[0].id)
    }

    @Test
    fun `parseNativeResult returns null when found is false`() {
        val graph = createTestGraph()
        val notFoundJson = """{"found": false}"""
        val parsed = GeoRescueNativeRouting.parseNativeResult(notFoundJson, graph)
        assertNull(parsed)
    }

    @Test
    fun `unreachable destinations return null without throwing`() {
        val graph = RouteGraph(
            nodes = listOf(RouteNode("A", 0.0, 0.0), RouteNode("Z", 1.0, 1.0)),
            edges = emptyList(),
            hazards = emptyList()
        )
        val route = GeoRescueNativeRouting.findRoute(graph, "A", "Z")
        assertNull(route)
    }

    @Test
    fun `safe haven fallback is engaged when destination is unreachable`() {
        // A -> B (blocked), C is safe haven reachable from A
        val nodes = listOf(
            RouteNode("A", 0.0, 0.0),
            RouteNode("B", 0.001, 0.0),
            RouteNode("C", 0.0, 0.001, isSafeHaven = true)
        )
        val edges = listOf(
            RouteEdge("A", "B", 100.0, blocked = true),
            RouteEdge("A", "C", 80.0)
        )
        val graph = RouteGraph(nodes = nodes, edges = edges)

        val route = GeoRescueNativeRouting.findRoute(graph, "A", "B")
        assertNotNull("Must fallback to reachable safe haven C", route)
        assertEquals(listOf("A", "C"), route!!.nodeIds)
    }
}
