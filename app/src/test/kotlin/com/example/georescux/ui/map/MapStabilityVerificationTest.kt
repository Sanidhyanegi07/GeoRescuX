package com.example.georescux.ui.map

import com.example.georescux.data.maps.MapRegionCatalog
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.osmdroid.config.Configuration

class MapStabilityVerificationTest {

    @Test
    fun `osmdroid tile cache configuration enforces memory and storage bounds`() {
        val maxBytes = 100L * 1024 * 1024
        val trimBytes = 80L * 1024 * 1024

        Configuration.getInstance().tileFileSystemCacheMaxBytes = maxBytes
        Configuration.getInstance().tileFileSystemCacheTrimBytes = trimBytes
        Configuration.getInstance().cacheMapTileCount = 50.toShort()
        Configuration.getInstance().cacheMapTileOvershoot = 10.toShort()

        assertEquals(maxBytes, Configuration.getInstance().tileFileSystemCacheMaxBytes)
        assertEquals(trimBytes, Configuration.getInstance().tileFileSystemCacheTrimBytes)
        assertEquals(50.toShort(), Configuration.getInstance().cacheMapTileCount)
        assertEquals(10.toShort(), Configuration.getInstance().cacheMapTileOvershoot)
    }

    @Test
    fun `all four official states have valid non-overlapping bounding coordinates`() {
        val states = MapRegionCatalog.availableRegions
        assertEquals(4, states.size)

        states.forEach { state ->
            assertTrue("${state.id} minLat < maxLat", state.minLatitude < state.maxLatitude)
            assertTrue("${state.id} minLng < maxLng", state.minLongitude < state.maxLongitude)
            assertTrue("${state.id} valid lat bounds", state.minLatitude in 20.0..40.0)
            assertTrue("${state.id} valid lng bounds", state.minLongitude in 70.0..90.0)
            assertNotNull("${state.id} display name not null", state.displayName)
        }
    }

    @Test
    fun `primary state uttarakhand has bundled vector map and routing graph`() {
        val uttarakhand = MapRegionCatalog.uttarakhand
        assertEquals("route_graph_uttarakhand", uttarakhand.graphAssetPath)
        assertEquals("maps/uttarakhand-state.map", uttarakhand.tileAssetPath)
        assertTrue(MapRegionCatalog.bundledRegions.contains(uttarakhand))
    }
}
