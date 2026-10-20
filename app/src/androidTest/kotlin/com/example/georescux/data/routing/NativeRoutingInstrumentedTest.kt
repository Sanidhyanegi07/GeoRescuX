package com.example.georescux.data.routing

import androidx.test.ext.junit.runners.AndroidJUnit4
import com.example.georescux.domain.routing.RoadHazard
import com.example.georescux.domain.routing.RouteEdge
import com.example.georescux.domain.routing.RouteGraph
import com.example.georescux.domain.routing.RouteNode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Gate A: On-device / Emulator instrumentation test validating that the packaged
 * C++ native library (`libgeorescux_routing.so`) is successfully loaded and executed
 * by the Android runtime (ART), verifying route continuity, cost calculation,
 * safe-haven fallback, and error handling.
 */
@RunWith(AndroidJUnit4::class)
class NativeRoutingInstrumentedTest {

    private fun createDeterministicGraph(): RouteGraph {
        val nodes = listOf(
            RouteNode("N1", 30.0, 78.0, isSafeHaven = false),
            RouteNode("N2", 30.01, 78.01, isSafeHaven = false),
            RouteNode("N3", 30.02, 78.02, isSafeHaven = false),
            RouteNode("HAVEN1", 30.005, 78.005, isSafeHaven = true)
        )
        val edges = listOf(
            RouteEdge("N1", "N2", 100.0, blocked = false),
            RouteEdge("N2", "N3", 150.0, blocked = false),
            RouteEdge("N1", "HAVEN1", 60.0, blocked = false)
        )
        val hazards = listOf(
            RoadHazard("H1", "N1", "N2", 50.0)
        )
        return RouteGraph(nodes = nodes, edges = edges, hazards = hazards, version = 1)
    }

    @Test
    fun testNativeLibraryStatusAndBasicRouteExecution() {
        // In an Android runtime environment (device or emulator), System.loadLibrary
        // loads libgeorescux_routing.so packaged in the APK.
        val graph = createDeterministicGraph()

        val route = GeoRescueNativeRouting.findRoute(graph, "N1", "N3")
        assertNotNull("Expected route from N1 to N3", route)
        assertEquals(listOf("N1", "N2", "N3"), route!!.nodeIds)
        assertEquals(250.0, route.totalDistanceMeters, 0.01)

        // Cost includes the 50.0m penalty from hazard H1 on edge N1-N2
        assertEquals(300.0, route.totalCostMeters, 0.01)
        assertEquals(1, route.hazardWarnings.size)
        assertEquals("H1", route.hazardWarnings[0].id)
    }

    @Test
    fun testDirectSafeHavenFallbackWhenTargetIsBlocked() {
        val base = createDeterministicGraph()
        // Sever N2-N3 connection by blocking it
        val severed = base.copy(
            edges = base.edges.map {
                if (it.connects("N2", "N3")) it.copy(blocked = true) else it
            }
        )

        // When primary destination N3 is unreachable, routing must safely
        // fallback to the nearest reachable safe haven (HAVEN1)
        val fallbackRoute = GeoRescueNativeRouting.findRoute(severed, "N1", "N3")
        assertNotNull("Fallback route to safe haven must be found", fallbackRoute)
        assertEquals(listOf("N1", "HAVEN1"), fallbackRoute!!.nodeIds)
        assertEquals(60.0, fallbackRoute.totalDistanceMeters, 0.01)
    }

    @Test
    fun testPathContinuityAndGraphIntegrity() {
        val graph = createDeterministicGraph()
        val route = GeoRescueNativeRouting.findRoute(graph, "N1", "N3")
        assertNotNull(route)

        // Verify path continuity: every adjacent pair of nodeIds must have an unblocked edge
        for (i in 0 until route!!.nodeIds.size - 1) {
            val u = route.nodeIds[i]
            val v = route.nodeIds[i + 1]
            val edge = graph.edges.find { it.connects(u, v) }
            assertNotNull("Edge connecting $u and $v must exist", edge)
            assertTrue("Edge connecting $u and $v must not be blocked", !edge!!.blocked)
        }
    }

    @Test
    fun testSafeHandlingOfNonexistentNodes() {
        val graph = createDeterministicGraph()
        val route = GeoRescueNativeRouting.findRoute(graph, "NONEXISTENT", "N3")
        // Nonexistent start should safely return null or safe haven without crashing
        assertTrue(route == null || route.nodeIds.isEmpty())
    }
}
