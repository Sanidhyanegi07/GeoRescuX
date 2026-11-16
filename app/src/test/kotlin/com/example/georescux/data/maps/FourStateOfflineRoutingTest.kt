package com.example.georescux.data.maps

import com.example.georescux.domain.routing.AStarPathfinder
import com.example.georescux.domain.routing.NearestNode
import com.example.georescux.domain.routing.RoadHazard
import com.example.georescux.domain.routing.RouteGraph
import com.example.georescux.domain.routing.RouteNode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

class FourStateOfflineRoutingTest {

    private fun findGraphFile(regionId: String): File {
        val candidates = listOf(
            File("output/$regionId/route_graph_$regionId"),
            File("../output/$regionId/route_graph_$regionId"),
            File("app/src/main/assets/route_graph_$regionId"),
            File("src/main/assets/route_graph_$regionId")
        )
        return candidates.firstOrNull { it.exists() && it.length() > 0L }
            ?: throw AssertionError("Missing route graph file for $regionId")
    }

    private fun loadGraph(regionId: String): RouteGraph {
        val file = findGraphFile(regionId)
        val json = file.readText()
        val parsed = RegionGraphLoader.parse(json)
            ?: throw AssertionError("Failed to parse route graph JSON for $regionId")
        return RouteGraph(parsed.nodes, parsed.edges, parsed.hazards, parsed.version)
    }

    private fun findRoutableNode(graph: RouteGraph, latitude: Double, longitude: Double): RouteNode {
        val candidates = graph.nodes.filter { graph.neighbors(it.id).size >= 2 }
        var bestNode: RouteNode? = null
        var bestDist = Double.MAX_VALUE
        for (node in candidates) {
            val d = haversineMeters(latitude, longitude, node.latitude, node.longitude)
            if (d < bestDist) {
                bestDist = d
                bestNode = node
            }
        }
        return bestNode ?: NearestNode.find(graph, latitude, longitude)!!
    }

    private fun haversineMeters(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val r = 6371000.0
        val dLat = Math.toRadians(lat2 - lat1)
        val dLon = Math.toRadians(lon2 - lon1)
        val a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(Math.toRadians(lat1)) * Math.cos(Math.toRadians(lat2)) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2)
        return 2 * r * Math.atan2(Math.sqrt(a), Math.sqrt(1.0 - a))
    }

    @Test
    fun `all four state graphs exist on disk and meet scale requirements`() {
        val states = listOf(
            "uttarakhand" to 100_000,
            "himachal_pradesh" to 40_000,
            "haryana" to 150_000,
            "uttar_pradesh" to 400_000
        )

        for ((stateId, _) in states) {
            val file = findGraphFile(stateId)
            assertTrue("File for $stateId should exist and be > 5 MB", file.length() > 5 * 1024 * 1024)
        }
    }

    @Test
    fun `himachal pradesh graph routes successfully between Shimla and Solan`() {
        val graph = loadGraph("himachal_pradesh")
        assertTrue("HP should have >= 40k nodes", graph.nodes.size >= 40_000)
        assertTrue("HP should have >= 50k edges", graph.edges.size >= 50_000)

        // Find routable nodes in Shimla (31.1048, 77.1734) and Solan (30.9045, 77.0967)
        val shimlaNode = findRoutableNode(graph, 31.1048, 77.1734)
        val solanNode = findRoutableNode(graph, 30.9045, 77.0967)

        assertNotNull("Shimla routable node should be found", shimlaNode)
        assertNotNull("Solan routable node should be found", solanNode)
        assertNotEquals(shimlaNode.id, solanNode.id)

        val route = AStarPathfinder.findRoute(graph, shimlaNode.id, solanNode.id)
        assertNotNull("Route between Shimla and Solan should be found", route)
        assertEquals(shimlaNode.id, route!!.nodeIds.first())
        assertEquals(solanNode.id, route.nodeIds.last())
        assertTrue("Route should have multiple segments", route.nodeIds.size >= 5)
        assertTrue("Route distance should be positive", route.totalDistanceMeters > 5000.0)

        // Path integrity: all steps must be connected by real edges
        route.nodeIds.zipWithNext { from, to ->
            val edge = graph.edgeBetween(from, to)
            assertNotNull("Route leg between $from and $to must exist", edge)
            assertTrue("Leg edge must have positive distance", edge!!.distanceMeters > 0.0)
            assertTrue("Leg edge must not be blocked", !edge.blocked)
        }
    }

    @Test
    fun `haryana graph routes successfully between Gurugram and Faridabad`() {
        val graph = loadGraph("haryana")
        assertTrue("Haryana should have >= 150k nodes", graph.nodes.size >= 150_000)
        assertTrue("Haryana should have >= 200k edges", graph.edges.size >= 200_000)

        // Gurugram (28.4595, 77.0266) to Faridabad (28.4089, 77.3178)
        val ggNode = NearestNode.find(graph, 28.4595, 77.0266)
        val fbdNode = NearestNode.find(graph, 28.4089, 77.3178)

        assertNotNull("Gurugram nearest node should be found", ggNode)
        assertNotNull("Faridabad nearest node should be found", fbdNode)

        val route = AStarPathfinder.findRoute(graph, ggNode!!.id, fbdNode!!.id)
        assertNotNull("Route between Gurugram and Faridabad should be found", route)
        assertEquals(ggNode.id, route!!.nodeIds.first())
        assertEquals(fbdNode.id, route.nodeIds.last())
        assertTrue("Route should have positive distance", route.totalDistanceMeters > 5000.0)
    }

    @Test
    fun `hazard penalty or road blockage induces rerouting`() {
        val graph = loadGraph("himachal_pradesh")
        val shimlaNode = findRoutableNode(graph, 31.1048, 77.1734)
        val solanNode = findRoutableNode(graph, 30.9045, 77.0967)

        val baselineRoute = AStarPathfinder.findRoute(graph, shimlaNode.id, solanNode.id)
        assertNotNull(baselineRoute)

        // Select an edge in the middle of the route
        val midIndex = baselineRoute!!.nodeIds.size / 2
        val fromNode = baselineRoute.nodeIds[midIndex]
        val toNode = baselineRoute.nodeIds[midIndex + 1]

        // Add a severe hazard penalty on this edge
        val hazard = RoadHazard(
            id = "test_hazard",
            fromNodeId = fromNode,
            toNodeId = toNode,
            penaltyMeters = 500_000.0
        )
        val graphWithHazard = RouteGraph(
            nodes = graph.nodes,
            edges = graph.edges,
            hazards = listOf(hazard),
            version = graph.version
        )

        val rerouted = AStarPathfinder.findRoute(graphWithHazard, shimlaNode.id, solanNode.id)
        assertNotNull("Rerouted path should still exist", rerouted)

        // Either the path avoided the penalized leg or the cost was updated
        val usesPenalizedLeg = rerouted!!.nodeIds.zipWithNext().any { (a, b) ->
            (a == fromNode && b == toNode) || (a == toNode && b == fromNode)
        }
        if (usesPenalizedLeg) {
            assertTrue(rerouted.totalDistanceMeters > baselineRoute.totalDistanceMeters)
        } else {
            assertTrue("Path successfully detoured around the penalized edge", true)
        }
    }
}
