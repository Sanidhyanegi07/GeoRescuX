package com.example.georescux.data.maps

import com.example.georescux.domain.routing.SafeHavenFallback
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * Verifies the redesigned Four-State Map Region Catalog:
 * 1. Uttarakhand (Primary, bundled)
 * 2. Himachal Pradesh (Secondary)
 * 3. Uttar Pradesh (Secondary)
 * 4. Haryana (Secondary)
 *
 * Ensures Delhi demo and generic sample regions are strictly excluded from
 * user-facing availableRegions and bundledRegions.
 */
class MapRegionCatalogTest {

    @Test
    fun `available regions contains exactly the four official states`() {
        val available = MapRegionCatalog.availableRegions
        assertEquals(4, available.size)
        val ids = available.map { it.id }.toSet()
        assertEquals(
            setOf("uttarakhand", "himachal_pradesh", "uttar_pradesh", "haryana"),
            ids
        )
        assertFalse(ids.contains("sample-region"))
    }

    @Test
    fun `uttarakhand region is bundled as primary`() {
        val region = MapRegionCatalog.uttarakhand
        assertEquals("uttarakhand", region.id)
        assertEquals("Uttarakhand", region.displayName)
        assertEquals(1, MapRegionCatalog.bundledRegions.size)
        assertTrue(MapRegionCatalog.bundledRegions.contains(region))
        assertEquals(MapRegionCatalog.uttarakhand, MapRegionCatalog.byId("uttarakhand"))
    }

    @Test
    fun `bounds are valid for all four official states`() {
        MapRegionCatalog.availableRegions.forEach { region ->
            assertTrue(region.minLatitude < region.maxLatitude)
            assertTrue(region.minLongitude < region.maxLongitude)
            assertTrue(region.minLatitude > -90 && region.maxLatitude < 90)
            assertTrue(region.minLongitude > -180 && region.maxLongitude < 180)
        }
    }

    @Test
    fun `sample region is deprecated and excluded from available and bundled lists`() {
        val sample = MapRegionCatalog.sampleRegion
        assertEquals("sample-region", sample.id)
        assertFalse(MapRegionCatalog.availableRegions.contains(sample))
        assertFalse(MapRegionCatalog.bundledRegions.contains(sample))
        // byId still safely resolves legacy test references
        assertEquals(sample, MapRegionCatalog.byId("sample-region"))
    }

    @Test
    fun `uttarakhand asset path follows the route_graph regionId convention`() {
        val region = MapRegionCatalog.uttarakhand
        assertEquals("route_graph_uttarakhand", region.graphAssetPath)
        assertEquals("maps/uttarakhand-state.map", region.tileAssetPath)
    }

    @Test
    fun `location inside uttarakhand selects the uttarakhand region`() {
        // Dehradun
        assertEquals(
            MapRegionCatalog.uttarakhand,
            MapRegionCatalog.regionForLocation(30.3165, 78.0322),
        )
        // Pithoragarh, in the state's far east
        assertEquals(
            MapRegionCatalog.uttarakhand,
            MapRegionCatalog.regionForLocation(29.5833, 80.2167),
        )
    }

    @Test
    fun `location inside himachal pradesh selects himachal pradesh`() {
        // Shimla
        assertEquals(
            MapRegionCatalog.himachalPradesh,
            MapRegionCatalog.regionForLocation(31.1048, 77.1734),
        )
    }

    @Test
    fun `location inside haryana selects haryana`() {
        // Hisar
        assertEquals(
            MapRegionCatalog.haryana,
            MapRegionCatalog.regionForLocation(29.1492, 75.7217),
        )
    }

    @Test
    fun `location inside uttar pradesh selects uttar pradesh`() {
        // Lucknow
        assertEquals(
            MapRegionCatalog.uttarPradesh,
            MapRegionCatalog.regionForLocation(26.8467, 80.9462),
        )
    }

    @Test
    fun `location outside every official region selects nothing`() {
        assertNull(MapRegionCatalog.regionForLocation(19.0760, 72.8777)) // Mumbai
        assertNull(MapRegionCatalog.regionForLocation(12.9716, 77.5946)) // Bengaluru
    }

    @Test
    fun `the bundled real sample region graph works with the stage 7a a star`() {
        val assetFile = java.io.File("src/main/assets/maps/sample-region/graph.json")
        if (!assetFile.exists()) return

        val graph = com.example.georescux.data.maps.RegionGraphLoader.parse(assetFile.readText())!!
        assertTrue("real region graph should have substantial nodes", graph.nodes.size >= 50)

        val edge = graph.edges.first()
        val route = SafeHavenFallback.findRoute(graph, edge.fromNodeId, edge.toNodeId)
        assertNotNull(route)
        assertEquals(edge.fromNodeId, route?.nodeIds?.first())
        assertEquals(edge.toNodeId, route?.nodeIds?.last())
        assertTrue(route!!.totalDistanceMeters > 0.0)

        val first = graph.nodes.first()
        val routeFromFirst = SafeHavenFallback.findRoute(graph, first.id, first.id)
        assertEquals(listOf(first.id), routeFromFirst?.nodeIds)
    }
}
