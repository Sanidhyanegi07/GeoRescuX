package com.example.georescux.data.incident

interface IncidentStore {
    fun saveIncident(incident: IncidentReport)
    fun getAllIncidents(): List<IncidentReport>
    fun getIncidentById(id: String): IncidentReport?
    fun deleteIncident(id: String)
    fun clearAll()
}
