package com.example.georescux.data.maps

import android.content.Context
import android.content.res.AssetManager
import com.example.georescux.domain.routing.MapRegion
import java.io.File
import java.io.IOException

/**
 * Official Map Region Catalog for GeoRescuX.
 * Redesigned exclusively around the four officially supported states:
 *   1. Uttarakhand (Primary - bundled vector basemap & offline routing graph)
 *   2. Himachal Pradesh (Secondary - downloadable regional package)
 *   3. Uttar Pradesh (Secondary - downloadable regional package)
 *   4. Haryana (Secondary - downloadable regional package)
 */
object MapRegionCatalog {
    val uttarakhand = MapRegion(
        id = "uttarakhand",
        displayName = "Uttarakhand",
        minLatitude = 28.7, maxLatitude = 31.5,
        minLongitude = 77.5, maxLongitude = 81.1,
        version = 1,
        graphAssetPath = "route_graph_uttarakhand",
        tileAssetPath = "maps/uttarakhand-state.map"
    )

    val himachalPradesh = MapRegion(
        id = "himachal_pradesh",
        displayName = "Himachal Pradesh",
        minLatitude = 30.3, maxLatitude = 33.3,
        minLongitude = 75.5, maxLongitude = 79.0,
        version = 1,
        graphAssetPath = "route_graph_himachal_pradesh",
        tileAssetPath = ""
    )

    val haryana = MapRegion(
        id = "haryana",
        displayName = "Haryana",
        minLatitude = 27.6, maxLatitude = 30.9,
        minLongitude = 74.4, maxLongitude = 77.6,
        version = 1,
        graphAssetPath = "route_graph_haryana",
        tileAssetPath = ""
    )

    val uttarPradesh = MapRegion(
        id = "uttar_pradesh",
        displayName = "Uttar Pradesh",
        minLatitude = 23.8, maxLatitude = 30.4,
        minLongitude = 77.0, maxLongitude = 84.6,
        version = 1,
        graphAssetPath = "route_graph_uttar_pradesh",
        tileAssetPath = ""
    )

    @Deprecated("Retired Delhi sample region. Retained only for legacy test fixtures.")
    val sampleRegion = MapRegion(
        id = "sample-region",
        displayName = "Connaught Place, New Delhi (sample)",
        minLatitude = 28.6290,
        maxLatitude = 28.6365,
        minLongitude = 77.2130,
        maxLongitude = 77.2270,
        version = 1,
        graphAssetPath = "maps/sample-region/graph.json",
        tileAssetPath = "maps/sample-region/tiles.sqlite",
    )

    /** Only the primary state ships bundled vector basemap and graph inside the APK. */
    val bundledRegions: List<MapRegion> = listOf(uttarakhand)

    /** The four official supported states for the GeoRescuX mission. */
    val availableRegions: List<MapRegion> = listOf(uttarakhand, himachalPradesh, uttarPradesh, haryana)

    @Suppress("DEPRECATION")
    fun byId(id: String): MapRegion? =
        availableRegions.firstOrNull { it.id == id }
            ?: bundledRegions.firstOrNull { it.id == id }
            ?: if (id == sampleRegion.id) sampleRegion else null

    /**
     * Resolves the local file containing the offline routing graph for a downloaded package,
     * or null if not installed in app private/external storage.
     */
    fun getGraphFile(context: Context, region: MapRegion): File? {
        val candidates = listOf(
            File(File(context.filesDir, "maps/${region.id}"), "route_graph_${region.id}"),
            File(File(context.filesDir, "maps/${region.id}"), "graph.json"),
            File(File(context.filesDir, "output/${region.id}"), "route_graph_${region.id}"),
            File(context.filesDir, "routing/${region.id}.json"),
            File(File(context.getExternalFilesDir(null), "maps/${region.id}"), "route_graph_${region.id}"),
            File(File(context.getExternalFilesDir(null), "maps/${region.id}"), "graph.json"),
            File(File(context.getExternalFilesDir(null), "output/${region.id}"), "route_graph_${region.id}"),
            File(context.getExternalFilesDir(null), "routing/${region.id}.json"),
        )
        return candidates.firstOrNull { it.exists() && it.length() > 0L }
    }

    /**
     * True when this region's offline routing graph is actually present on
     * this device. Bundled regions ship inside the APK; downloaded packages
     * are located in local app storage.
     */
    fun isGraphInstalled(assets: AssetManager, region: MapRegion): Boolean = try {
        assets.open(region.graphAssetPath).close(); true
    } catch (_: IOException) {
        false
    }

    fun isGraphInstalled(context: Context, region: MapRegion): Boolean {
        if (isGraphInstalled(context.assets, region)) return true
        return getGraphFile(context, region) != null
    }

    /**
     * True when offline *rendering* data (vector basemap or raster tile
     * archive) is present either in APK assets or downloaded in app storage.
     */
    fun isOfflineMapInstalled(assets: AssetManager, region: MapRegion): Boolean {
        if (region.tileAssetPath.isBlank()) return false
        return try {
            assets.open(region.tileAssetPath).close(); true
        } catch (_: IOException) {
            false
        }
    }

    fun isOfflineMapInstalled(context: Context, region: MapRegion): Boolean {
        if (isOfflineMapInstalled(context.assets, region)) return true
        val candidates = listOf(
            File(File(context.filesDir, "maps/${region.id}"), "state.map"),
            File(File(context.filesDir, "maps"), "${region.id}.map"),
            File(File(context.filesDir, "output/${region.id}"), "state.map"),
            File(File(context.filesDir, "osmdroid"), "${region.id}-tiles.map"),
            File(File(context.filesDir, "osmdroid"), "${region.id}-tiles.sqlite"),
            File(File(context.getExternalFilesDir(null), "maps/${region.id}"), "state.map"),
            File(File(context.getExternalFilesDir(null), "maps"), "${region.id}.map"),
            File(File(context.getExternalFilesDir(null), "output/${region.id}"), "state.map"),
        )
        return candidates.any { it.exists() && it.length() > 0L }
    }

    /**
     * The region containing the given position, or null when the position
     * lies outside the 4 officially supported states.
     */
    fun regionForLocation(latitude: Double, longitude: Double): MapRegion? =
        availableRegions.firstOrNull { region ->
            latitude in region.minLatitude..region.maxLatitude &&
                longitude in region.minLongitude..region.maxLongitude
        }
}
