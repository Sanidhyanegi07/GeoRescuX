package com.example.georescux.data.incident

import org.json.JSONObject
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.UUID

/**
 * Represents a user-reported disaster or emergency incident (e.g. Flood, Earthquake, Landslide).
 * Designed for offline-first resilience (persisted locally) and synchronized with Firebase
 * Realtime Database and the BLE mesh relay.
 */
data class IncidentReport(
    val id: String = UUID.randomUUID().toString(),
    val type: String, // FLOOD, EARTHQUAKE, LANDSLIDE, FIRE, ROUTE_BLOCK, BUILDING_COLLAPSE, MEDICAL, OTHER
    val title: String,
    val description: String,
    val latitude: Double,
    val longitude: Double,
    val severity: String = "HIGH", // LOW, MEDIUM, HIGH, CRITICAL
    val status: String = "REPORTED", // REPORTED, VERIFIED, RESOLVED
    val timestampMs: Long = System.currentTimeMillis(),
    val imageUri: String? = null,
    val imageBase64: String? = null,
    val reportedBy: String = "Anonymous",
) {
    fun getTypeDisplayName(): String = when (type.uppercase()) {
        "FLOOD" -> "Flood"
        "EARTHQUAKE" -> "Earthquake"
        "LANDSLIDE" -> "Landslide"
        "FIRE" -> "Fire / Wildfire"
        "ROUTE_BLOCK", "ROAD_BLOCK" -> "Road Blockage"
        "BUILDING_COLLAPSE", "COLLAPSE" -> "Building Collapse"
        "MEDICAL" -> "Medical Emergency"
        else -> "Hazard / Incident"
    }

    fun getTypeEmoji(): String = when (type.uppercase()) {
        "FLOOD" -> "🌊"
        "EARTHQUAKE" -> "🌋"
        "LANDSLIDE" -> "🪨"
        "FIRE" -> "🔥"
        "ROUTE_BLOCK", "ROAD_BLOCK" -> "🚧"
        "BUILDING_COLLAPSE", "COLLAPSE" -> "🏚️"
        "MEDICAL" -> "🏥"
        else -> "⚠️"
    }

    fun getFormattedTime(): String {
        return SimpleDateFormat("dd MMM yyyy, hh:mm a", Locale.getDefault()).format(Date(timestampMs))
    }

    fun getRelativeTime(): String {
        val diffMs = System.currentTimeMillis() - timestampMs
        val minutes = diffMs / (1000 * 60)
        val hours = minutes / 60
        val days = hours / 24
        return when {
            minutes < 1 -> "Just now"
            minutes < 60 -> "${minutes}m ago"
            hours < 24 -> "${hours}h ago"
            else -> "${days}d ago"
        }
    }

    fun toJson(): JSONObject = JSONObject().apply {
        put("id", id)
        put("type", type)
        put("title", title)
        put("description", description)
        put("latitude", latitude)
        put("longitude", longitude)
        put("severity", severity)
        put("status", status)
        put("timestampMs", timestampMs)
        if (imageUri != null) put("imageUri", imageUri)
        if (imageBase64 != null) put("imageBase64", imageBase64)
        put("reportedBy", reportedBy)
    }

    fun toMap(): Map<String, Any?> = mapOf(
        "id" to id,
        "type" to type,
        "title" to title,
        "description" to description,
        "latitude" to latitude,
        "longitude" to longitude,
        "severity" to severity,
        "status" to status,
        "timestampMs" to timestampMs,
        "imageUri" to imageUri,
        "imageBase64" to imageBase64,
        "reportedBy" to reportedBy,
    )

    companion object {
        fun fromJson(json: JSONObject): IncidentReport = IncidentReport(
            id = json.optString("id", UUID.randomUUID().toString()),
            type = json.optString("type", "OTHER"),
            title = json.optString("title", "Incident"),
            description = json.optString("description", ""),
            latitude = json.optDouble("latitude", 0.0),
            longitude = json.optDouble("longitude", 0.0),
            severity = json.optString("severity", "HIGH"),
            status = json.optString("status", "REPORTED"),
            timestampMs = json.optLong("timestampMs", System.currentTimeMillis()),
            imageUri = if (json.has("imageUri") && !json.isNull("imageUri")) json.getString("imageUri") else null,
            imageBase64 = if (json.has("imageBase64") && !json.isNull("imageBase64")) json.getString("imageBase64") else null,
            reportedBy = json.optString("reportedBy", "Anonymous"),
        )

        fun fromMap(id: String, map: Map<String, Any?>): IncidentReport = IncidentReport(
            id = id,
            type = map["type"] as? String ?: "OTHER",
            title = map["title"] as? String ?: "Incident",
            description = map["description"] as? String ?: "",
            latitude = (map["latitude"] as? Number)?.toDouble() ?: 0.0,
            longitude = (map["longitude"] as? Number)?.toDouble() ?: 0.0,
            severity = map["severity"] as? String ?: "HIGH",
            status = map["status"] as? String ?: "REPORTED",
            timestampMs = (map["timestampMs"] as? Number)?.toLong() ?: System.currentTimeMillis(),
            imageUri = map["imageUri"] as? String,
            imageBase64 = map["imageBase64"] as? String,
            reportedBy = map["reportedBy"] as? String ?: "Anonymous",
        )
    }
}
