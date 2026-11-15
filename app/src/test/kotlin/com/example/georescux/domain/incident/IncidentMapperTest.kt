package com.example.georescux.domain.incident

import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus
import org.junit.Assert.assertEquals
import org.junit.Test

/** Verifies the honest SOS → incident derivation, especially the sync state. */
class IncidentMapperTest {

    private fun emergency(location: SosLocation? = null) = SosEmergency(
        id = "alert-1",
        startedAtMs = 5_000L,
        stoppedAtMs = null,
        location = location,
        locationStatus = location?.let { SosLocationStatus.ACQUIRED },
    )

    @Test
    fun `a pending cloud backup maps to QUEUED, not SYNCED`() {
        val event = IncidentMapper.fromSos(emergency(), originId = "uid-1", pendingCloudBackup = true)
        assertEquals(IncidentSyncState.QUEUED, event.syncState)
    }

    @Test
    fun `a confirmed cloud backup maps to SYNCED`() {
        val event = IncidentMapper.fromSos(emergency(), originId = "uid-1", pendingCloudBackup = false)
        assertEquals(IncidentSyncState.SYNCED, event.syncState)
    }

    @Test
    fun `sos events are critical and carry the emergency identity`() {
        val event = IncidentMapper.fromSos(
            emergency(location = SosLocation(28.63, 77.22, 10f, 7_000L, "gps")),
            originId = "uid-1",
            pendingCloudBackup = true,
        )
        assertEquals("alert-1", event.eventId)
        assertEquals("alert-1", IncidentMapper.emergencyIdFor(event))
        assertEquals("alert-1", event.payload["emergencyId"])
        assertEquals("uid-1", event.originId)
        assertEquals(IncidentType.SOS, event.type)
        assertEquals(IncidentSeverity.CRITICAL, event.severity)
        assertEquals(5_000L, event.occurredAtMs)
        assertEquals(28.63, event.latitude!!, 0.0000001)
        assertEquals("ACTIVE", event.payload["status"])
    }

    @Test
    fun `an active emergency reports its status without a stopped timestamp`() {
        val active = emergency().copy(stoppedAtMs = null)
        val event = IncidentMapper.fromSos(active, originId = "uid-1", pendingCloudBackup = true)
        assertEquals("ACTIVE", event.payload["status"])
        assertEquals("5000", event.payload["startedAtMs"])
        assertEquals(null, event.payload["stoppedAtMs"])
        assertEquals("alert-1", event.eventId)
    }

    @Test
    fun `a completed emergency generates distinct transition eventId and preserves emergency identity`() {
        val completed = emergency().copy(stoppedAtMs = 9_000L)
        val event = IncidentMapper.fromSos(completed, originId = "uid-1", pendingCloudBackup = false)
        assertEquals("alert-1:COMPLETED", event.eventId)
        assertEquals("alert-1", IncidentMapper.emergencyIdFor(event))
        assertEquals("alert-1", event.payload["emergencyId"])
        assertEquals("COMPLETED", event.payload["status"])
        assertEquals("5000", event.payload["startedAtMs"])
        assertEquals("9000", event.payload["stoppedAtMs"])
        assertEquals(9_000L, event.occurredAtMs)
    }
}
