package com.example.georescux.data.maps

import android.content.Context
import com.example.georescux.domain.routing.MapRegion
import java.io.File
import java.io.InputStream
import java.util.concurrent.ConcurrentHashMap

/**
 * High-precision State Boundary Classifier for GeoRescuX.
 * Replaces coarse rectangular bounding boxes with authentic geographical polygon
 * ray-casting geometry across the four official states:
 * 1. Uttarakhand
 * 2. Himachal Pradesh
 * 3. Haryana
 * 4. Uttar Pradesh
 *
 * Performance optimization:
 * Uses an initial bounding-box prefilter (O(1)), followed by a ray-casting point-in-polygon
 * test against the official boundary geometry. Locations in Delhi, Rajasthan, Bihar, Punjab,
 * or abroad outside the four states return null (unsupported).
 */
object StateBoundaryClassifier {

    private data class BoundingBox(
        val minLat: Double,
        val maxLat: Double,
        val minLon: Double,
        val maxLon: Double
    ) {
        fun contains(lat: Double, lon: Double): Boolean =
            lat in minLat..maxLat && lon in minLon..maxLon
    }

    private val BBOXES = mapOf(
        "uttarakhand" to BoundingBox(28.71, 31.48, 77.56, 81.03),
        "himachal_pradesh" to BoundingBox(30.38, 33.26, 75.57, 79.00),
        "haryana" to BoundingBox(27.65, 30.94, 74.46, 77.60),
        "uttar_pradesh" to BoundingBox(23.87, 30.42, 77.08, 84.64)
    )

    // Cached polygon rings: regionId -> list of polygons, where each polygon is an outer ring (list of [lon, lat])
    private val polygonCache = ConcurrentHashMap<String, List<List<DoubleArray>>>()

    /**
     * Classifies a GPS latitude and longitude into one of the four supported MapRegions,
     * or null if outside all four states.
     */
    fun classify(latitude: Double, longitude: Double, context: Context? = null): MapRegion? {
        // Evaluate in official priority order: Uttarakhand (primary), HP, Haryana, UP
        val candidates = listOf(
            MapRegionCatalog.uttarakhand,
            MapRegionCatalog.himachalPradesh,
            MapRegionCatalog.haryana,
            MapRegionCatalog.uttarPradesh
        )

        for (region in candidates) {
            val bbox = BBOXES[region.id] ?: continue
            // 1. Fast Bounding-Box Prefilter (O(1))
            if (!bbox.contains(latitude, longitude)) continue

            // 2. High-Precision Polygon Geometry Ray-Casting
            val polygons = getPolygons(region.id, context)
            if (polygons.isNotEmpty()) {
                if (isInsideAnyPolygon(latitude, longitude, polygons)) {
                    return region
                }
            } else {
                // If boundary file not accessible, fall back to region bounding box
                if (latitude in region.minLatitude..region.maxLatitude &&
                    longitude in region.minLongitude..region.maxLongitude
                ) {
                    return region
                }
            }
        }
        return null
    }

    fun isPointInRegion(regionId: String, latitude: Double, longitude: Double, context: Context? = null): Boolean {
        val bbox = BBOXES[regionId] ?: return false
        if (!bbox.contains(latitude, longitude)) return false
        val polygons = getPolygons(regionId, context)
        return if (polygons.isNotEmpty()) {
            isInsideAnyPolygon(latitude, longitude, polygons)
        } else {
            val region = MapRegionCatalog.byId(regionId) ?: return false
            latitude in region.minLatitude..region.maxLatitude && longitude in region.minLongitude..region.maxLongitude
        }
    }

    private fun isInsideAnyPolygon(lat: Double, lon: Double, polygons: List<List<DoubleArray>>): Boolean {
        for (poly in polygons) {
            if (rayCast(lat, lon, poly)) return true
        }
        return false
    }

    /**
     * Standard Ray-Casting point-in-polygon algorithm.
     * polygon is a ring of DoubleArray where [0] is longitude, [1] is latitude.
     */
    fun rayCast(lat: Double, lon: Double, polygon: List<DoubleArray>): Boolean {
        var inside = false
        val n = polygon.size
        if (n < 3) return false
        var p1 = polygon[0]
        for (i in 1..n) {
            val p2 = polygon[i % n]
            val p1Lon = p1[0]
            val p1Lat = p1[1]
            val p2Lon = p2[0]
            val p2Lat = p2[1]

            if (lat > minOf(p1Lat, p2Lat)) {
                if (lat <= maxOf(p1Lat, p2Lat)) {
                    if (lon <= maxOf(p1Lon, p2Lon)) {
                        val xinters = if (p1Lat != p2Lat) {
                            (lat - p1Lat) * (p2Lon - p1Lon) / (p2Lat - p1Lat) + p1Lon
                        } else {
                            p1Lon
                        }
                        if (p1Lon == p2Lon || lon <= xinters) {
                            inside = !inside
                        }
                    }
                }
            }
            p1 = p2
        }
        return inside
    }

    private fun getPolygons(regionId: String, context: Context?): List<List<DoubleArray>> {
        polygonCache[regionId]?.let { return it }

        val loaded = loadBoundary(regionId, context)
        if (loaded.isNotEmpty()) {
            polygonCache[regionId] = loaded
        }
        return loaded
    }

    private fun loadBoundary(regionId: String, context: Context?): List<List<DoubleArray>> {
        // Try loading from Android assets
        if (context != null) {
            try {
                context.assets.open("maps/boundaries/$regionId.geojson").use { stream ->
                    return parseGeoJsonPolygons(stream.bufferedReader().readText())
                }
            } catch (_: Exception) {}
        }

        // Try candidate file locations
        val candidateFiles = listOf(
            File("app/src/main/assets/maps/boundaries/$regionId.geojson"),
            File("src/main/assets/maps/boundaries/$regionId.geojson"),
            File("../app/src/main/assets/maps/boundaries/$regionId.geojson"),
            File("output/$regionId/boundary.geojson"),
            File("../output/$regionId/boundary.geojson"),
            context?.let { File(it.filesDir, "maps/$regionId/boundary.geojson") }
        ).filterNotNull()

        for (file in candidateFiles) {
            if (file.exists() && file.length() > 0L) {
                try {
                    val content = file.readText()
                    val parsed = parseGeoJsonPolygons(content)
                    if (parsed.isNotEmpty()) return parsed
                } catch (_: Exception) {}
            }
        }

        return emptyList()
    }

    @Suppress("UNCHECKED_CAST")
    fun parseGeoJsonPolygons(json: String): List<List<DoubleArray>> {
        val result = mutableListOf<List<DoubleArray>>()
        try {
            val root = MiniJson.parse(json) as? Map<*, *> ?: return emptyList()
            val features = root["features"] as? List<*> ?: return emptyList()

            for (feat in features) {
                val fMap = feat as? Map<*, *> ?: continue
                val geom = fMap["geometry"] as? Map<*, *> ?: continue
                val type = geom["type"] as? String ?: continue
                val coords = geom["coordinates"] as? List<*> ?: continue

                when (type) {
                    "Polygon" -> {
                        // Outer ring is coords[0]
                        val outerRing = coords.firstOrNull() as? List<*> ?: continue
                        val ringPts = parseRing(outerRing)
                        if (ringPts.isNotEmpty()) result.add(ringPts)
                    }
                    "MultiPolygon" -> {
                        for (poly in coords) {
                            val polyCoords = poly as? List<*> ?: continue
                            val outerRing = polyCoords.firstOrNull() as? List<*> ?: continue
                            val ringPts = parseRing(outerRing)
                            if (ringPts.isNotEmpty()) result.add(ringPts)
                        }
                    }
                }
            }
        } catch (_: Exception) {}
        return result
    }

    private fun parseRing(ring: List<*>): List<DoubleArray> {
        val pts = mutableListOf<DoubleArray>()
        for (item in ring) {
            val pt = item as? List<*> ?: continue
            if (pt.size >= 2) {
                val lon = (pt[0] as? Number)?.toDouble() ?: continue
                val lat = (pt[1] as? Number)?.toDouble() ?: continue
                pts.add(doubleArrayOf(lon, lat))
            }
        }
        return pts
    }
}
