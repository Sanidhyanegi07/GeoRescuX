package com.example.georescux.data.incident

import android.content.Context
import org.json.JSONArray

/**
 * SharedPreferences-backed local incident report store.
 * Fully offline-capable and thread-safe.
 */
class SharedPreferencesIncidentStore(context: Context) : IncidentStore {

    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    @Synchronized
    override fun saveIncident(incident: IncidentReport) {
        val current = getAllIncidents().toMutableList()
        val index = current.indexOfFirst { it.id == incident.id }
        if (index >= 0) {
            current[index] = incident
        } else {
            current.add(0, incident) // Newest first
        }

        val jsonArray = JSONArray()
        current.forEach { jsonArray.put(it.toJson()) }
        prefs.edit().putString(KEY_INCIDENTS, jsonArray.toString()).apply()
    }

    @Synchronized
    override fun getAllIncidents(): List<IncidentReport> {
        val raw = prefs.getString(KEY_INCIDENTS, null) ?: return emptyList()
        return try {
            val array = JSONArray(raw)
            (0 until array.length()).map { idx ->
                IncidentReport.fromJson(array.getJSONObject(idx))
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    @Synchronized
    override fun getIncidentById(id: String): IncidentReport? {
        return getAllIncidents().firstOrNull { it.id == id }
    }

    @Synchronized
    override fun deleteIncident(id: String) {
        val filtered = getAllIncidents().filterNot { it.id == id }
        val jsonArray = JSONArray()
        filtered.forEach { jsonArray.put(it.toJson()) }
        prefs.edit().putString(KEY_INCIDENTS, jsonArray.toString()).apply()
    }

    @Synchronized
    override fun clearAll() {
        prefs.edit().remove(KEY_INCIDENTS).apply()
    }

    private companion object {
        const val PREFS_NAME = "incident_reports_store"
        const val KEY_INCIDENTS = "incidents_list"
    }
}
