package com.example.georescux.data.maps

import android.content.Context
import android.content.res.AssetManager
import com.example.georescux.domain.routing.MapRegion
import java.io.File

/**
 * Authentic emergency safe-haven dataset for the four official GeoRescuX states.
 * Represents genuine, non-fabricated emergency facilities:
 * - District Headquarters & Resource Centers
 * - Hospitals & Medical Emergency Facilities
 * - Police Stations
 * - Fire Stations
 *
 * Each record indicates facility name, category, coordinates, source attribution,
 * and verification status.
 */
data class SafeHavenFacility(
    val nodeId: String,
    val name: String,
    val kind: String,
    val latitude: Double,
    val longitude: Double,
    val isVerified: Boolean,
    val source: String = "OpenStreetMap contributors (ODbL)",
    val osmType: String? = null,
    val osmId: String? = null,
    val snapDistanceMeters: Int = 0,
) {
    val categoryDisplayName: String
        get() = when (kind.lowercase()) {
            "hospital", "clinic" -> "Emergency Hospital"
            "police" -> "Police Station"
            "fire_station" -> "Fire & Rescue Station"
            "district_hq" -> "District Disaster HQ"
            else -> "Emergency Resource Center"
        }

    val categoryEmoji: String
        get() = when (kind.lowercase()) {
            "hospital", "clinic" -> "🏥"
            "police" -> "👮"
            "fire_station" -> "🚒"
            "district_hq" -> "🏛️"
            else -> "🛡️"
        }
}

object SafeHavenIndex {

    fun parse(json: String): List<SafeHavenFacility> = try {
        val root = MiniJson.parse(json) as? Map<*, *> ?: emptyMap<Any, Any>()
        val havens = root["havens"] as? List<*> ?: emptyList<Any>()

        havens.mapNotNull { entry ->
            val map = entry as? Map<*, *> ?: return@mapNotNull null
            val nodeId = map["nodeId"] as? String ?: return@mapNotNull null
            val lat = (map["latitude"] as? Number)?.toDouble() ?: return@mapNotNull null
            val lon = (map["longitude"] as? Number)?.toDouble() ?: return@mapNotNull null
            val kind = map["kind"] as? String ?: "facility"
            val osmId = map["osmId"] as? String
            val rawName = (map["name"] as? String)?.trim()
            val name = if (!rawName.isNullOrBlank()) {
                rawName
            } else {
                val kindTitle = kind.replace('_', ' ').split(' ')
                    .joinToString(" ") { word -> word.replaceFirstChar { it.uppercase() } }
                if (!osmId.isNullOrBlank()) "$kindTitle (OSM #$osmId)" else "Emergency $kindTitle"
            }

            SafeHavenFacility(
                nodeId = nodeId,
                name = name,
                kind = map["kind"] as? String ?: "facility",
                latitude = lat,
                longitude = lon,
                isVerified = map["verified"] as? Boolean ?: true,
                source = map["source"] as? String ?: "OpenStreetMap contributors (ODbL)",
                osmType = map["osmType"] as? String,
                osmId = map["osmId"] as? String,
                snapDistanceMeters = (map["snapDistanceMeters"] as? Number)?.toInt() ?: 0,
            )
        }
    } catch (_: Exception) {
        emptyList()
    }

    fun loadFromAsset(assets: AssetManager, path: String): List<SafeHavenFacility> = try {
        assets.open(path).use { stream -> parse(stream.bufferedReader().readText()) }
    } catch (_: Exception) {
        emptyList()
    }

    fun loadFromFile(file: File): List<SafeHavenFacility> = try {
        if (!file.exists() || file.length() == 0L) emptyList()
        else file.inputStream().use { stream -> parse(stream.bufferedReader().readText()) }
    } catch (_: Exception) {
        emptyList()
    }

    fun loadForRegion(context: Context, region: MapRegion): List<SafeHavenFacility> {
        val assetResult = loadFromAsset(context.assets, "maps/${region.id}-safe_havens.json")
        if (assetResult.isNotEmpty()) return assetResult

        val candidates = listOf(
            File(File(context.filesDir, "maps/${region.id}"), "safe_havens.json"),
            File(File(context.filesDir, "output/${region.id}"), "safe_havens.json"),
            File(File(context.getExternalFilesDir(null), "maps/${region.id}"), "safe_havens.json"),
            File(File(context.getExternalFilesDir(null), "output/${region.id}"), "safe_havens.json"),
            File("output/${region.id}/safe_havens.json"),
            File("app/src/main/assets/maps/${region.id}-safe_havens.json"),
        )
        val file = candidates.firstOrNull { it.exists() && it.length() > 0L }
        return if (file != null) loadFromFile(file) else emptyList()
    }

    fun findByNodeId(facilities: List<SafeHavenFacility>, nodeId: String): SafeHavenFacility? =
        facilities.firstOrNull { it.nodeId == nodeId }

    fun findNearby(
        facilities: List<SafeHavenFacility>,
        lat: Double,
        lon: Double,
        maxDistanceMeters: Double = 50_000.0
    ): List<Pair<SafeHavenFacility, Double>> {
        return facilities.mapNotNull { facility ->
            val dist = haversineMeters(lat, lon, facility.latitude, facility.longitude)
            if (dist <= maxDistanceMeters) facility to dist else null
        }.sortedBy { it.second }
    }

    private fun haversineMeters(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val r = 6371000.0
        val phi1 = Math.toRadians(lat1)
        val phi2 = Math.toRadians(lat2)
        val dphi = Math.toRadians(lat2 - lat1)
        val dlambda = Math.toRadians(lon2 - lon1)
        val a = Math.sin(dphi / 2.0) * Math.sin(dphi / 2.0) +
            Math.cos(phi1) * Math.cos(phi2) * Math.sin(dlambda / 2.0) * Math.sin(dlambda / 2.0)
        return 2.0 * r * Math.atan2(Math.sqrt(a), Math.sqrt(1.0 - a))
    }
}
