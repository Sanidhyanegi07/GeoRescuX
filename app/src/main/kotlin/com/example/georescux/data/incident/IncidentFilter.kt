package com.example.georescux.data.incident

/**
 * Filter criteria for displaying disaster/hazard incidents on the map.
 * Supports filtering independently or simultaneously by Incident Type (Flood, Earthquake, Fire...)
 * and Severity (Critical, High, Medium, Low).
 */
data class IncidentFilter(
    val type: String? = null, // null or "ALL" matches all types
    val severity: String? = null, // null or "ALL" matches all severities
) {
    val isActive: Boolean
        get() = (type != null && !type.equals("ALL", ignoreCase = true)) ||
                (severity != null && !severity.equals("ALL", ignoreCase = true))

    fun matches(incident: IncidentReport): Boolean {
        val matchesType = when {
            type == null || type.equals("ALL", ignoreCase = true) -> true
            type.equals("ROUTE_BLOCK", ignoreCase = true) || type.equals("ROAD_BLOCK", ignoreCase = true) ->
                incident.type.equals("ROUTE_BLOCK", ignoreCase = true) || incident.type.equals("ROAD_BLOCK", ignoreCase = true)
            type.equals("BUILDING_COLLAPSE", ignoreCase = true) || type.equals("COLLAPSE", ignoreCase = true) ->
                incident.type.equals("BUILDING_COLLAPSE", ignoreCase = true) || incident.type.equals("COLLAPSE", ignoreCase = true)
            else -> incident.type.equals(type, ignoreCase = true)
        }

        val matchesSeverity = when {
            severity == null || severity.equals("ALL", ignoreCase = true) -> true
            else -> incident.severity.equals(severity, ignoreCase = true)
        }

        return matchesType && matchesSeverity
    }

    fun describe(): String {
        if (!isActive) return "All Incidents"
        val parts = mutableListOf<String>()
        if (type != null && !type.equals("ALL", ignoreCase = true)) {
            parts.add(when (type.uppercase()) {
                "FLOOD" -> "🌊 Flood"
                "EARTHQUAKE" -> "🌋 Earthquake"
                "LANDSLIDE" -> "🪨 Landslide"
                "FIRE" -> "🔥 Fire"
                "ROUTE_BLOCK", "ROAD_BLOCK" -> "🚧 Road Block"
                "BUILDING_COLLAPSE", "COLLAPSE" -> "🏚️ Collapse"
                "MEDICAL" -> "🏥 Medical"
                else -> "⚠️ $type"
            })
        }
        if (severity != null && !severity.equals("ALL", ignoreCase = true)) {
            parts.add("${severity.uppercase()} Severity")
        }
        return parts.joinToString(" • ")
    }
}
