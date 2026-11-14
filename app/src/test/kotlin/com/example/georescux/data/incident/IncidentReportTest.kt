package com.example.georescux.data.incident

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class IncidentReportTest {

    @Test
    fun `round-trip JSON serialization preserves all fields`() {
        val original = IncidentReport(
            id = "test-inc-123",
            type = "FLOOD",
            title = "Severe Flash Flood",
            description = "Water rising rapidly over the causeway",
            latitude = 30.5564,
            longitude = 79.5670,
            severity = "CRITICAL",
            status = "REPORTED",
            timestampMs = 1729000000000L,
            imageUri = "/data/user/0/com.example.georescux/files/incident_images/incident_123.jpg",
            imageBase64 = "base64encodedbytes==",
            reportedBy = "Tester",
        )

        val json = original.toJson()
        val restored = IncidentReport.fromJson(json)

        assertEquals(original.id, restored.id)
        assertEquals(original.type, restored.type)
        assertEquals(original.title, restored.title)
        assertEquals(original.description, restored.description)
        assertEquals(original.latitude, restored.latitude, 0.0001)
        assertEquals(original.longitude, restored.longitude, 0.0001)
        assertEquals(original.severity, restored.severity)
        assertEquals(original.status, restored.status)
        assertEquals(original.timestampMs, restored.timestampMs)
        assertEquals(original.imageUri, restored.imageUri)
        assertEquals(original.imageBase64, restored.imageBase64)
        assertEquals(original.reportedBy, restored.reportedBy)
    }

    @Test
    fun `round-trip Map conversion preserves cloud fields`() {
        val original = IncidentReport(
            id = "cloud-inc-456",
            type = "EARTHQUAKE",
            title = "Earthquake Damage",
            description = "Fissures across main road, structural damage",
            latitude = 30.1234,
            longitude = 78.5678,
            severity = "HIGH",
            status = "VERIFIED",
            timestampMs = 1729100000000L,
            imageUri = null,
            imageBase64 = null,
            reportedBy = "FirstResponder-1",
        )

        val map = original.toMap()
        val restored = IncidentReport.fromMap(original.id, map)

        assertEquals(original.id, restored.id)
        assertEquals(original.type, restored.type)
        assertEquals(original.description, restored.description)
        assertEquals(original.latitude, restored.latitude, 0.0001)
        assertEquals(original.longitude, restored.longitude, 0.0001)
        assertEquals(original.severity, restored.severity)
        assertEquals(original.status, restored.status)
        assertNull(restored.imageUri)
        assertNull(restored.imageBase64)
    }

    @Test
    fun `toCloudMap strips private device file and content URIs while preserving imageBase64 and remote URLs`() {
        val fileReport = IncidentReport(
            id = "inc-file",
            type = "FLOOD",
            title = "Causeway Flood",
            description = "Submerged",
            latitude = 30.0,
            longitude = 78.0,
            imageUri = "file:///data/user/0/com.example.georescux/files/photo.jpg",
            imageBase64 = "photo_base64_payload",
        )
        val fileCloudMap = fileReport.toCloudMap()
        assertNull("file:// URI must be stripped from cloud payload", fileCloudMap["imageUri"])
        assertEquals("photo_base64_payload", fileCloudMap["imageBase64"])

        val dataPathReport = fileReport.copy(
            id = "inc-data",
            imageUri = "/data/user/0/com.example.georescux/cache/temp.jpg",
        )
        assertNull("/data/ path must be stripped from cloud payload", dataPathReport.toCloudMap()["imageUri"])

        val contentUriReport = fileReport.copy(
            id = "inc-content",
            imageUri = "content://media/external/images/media/42",
        )
        assertNull("content:// URI must be stripped from cloud payload", contentUriReport.toCloudMap()["imageUri"])

        val remoteHttpsReport = fileReport.copy(
            id = "inc-https",
            imageUri = "https://firebasestorage.googleapis.com/v0/b/app/o/incidents%2Fphoto.jpg",
        )
        assertEquals("https://firebasestorage.googleapis.com/v0/b/app/o/incidents%2Fphoto.jpg", remoteHttpsReport.toCloudMap()["imageUri"])
    }

    @Test
    fun `fromMap rejects and sanitizes local file URIs received from untrusted cloud data`() {
        val maliciousCloudMap = mapOf<String, Any?>(
            "type" to "LANDSLIDE",
            "title" to "Rockfall",
            "description" to "Debris",
            "latitude" to 30.1,
            "longitude" to 78.2,
            "imageUri" to "file:///etc/shadow",
            "imageBase64" to "payload",
        )
        val restored = IncidentReport.fromMap("inc-untrusted", maliciousCloudMap)
        assertNull("Untrusted local file URI received from cloud must be rejected", restored.imageUri)
        assertEquals("payload", restored.imageBase64)

        val legitimateCloudMap = maliciousCloudMap + ("imageUri" to "https://cdn.example.com/disaster.png")
        val legitimateRestored = IncidentReport.fromMap("inc-legit", legitimateCloudMap)
        assertEquals("https://cdn.example.com/disaster.png", legitimateRestored.imageUri)
    }

    @Test
    fun `type emoji and display names resolve correctly for key disaster types`() {
        val flood = IncidentReport(type = "FLOOD", title = "", description = "", latitude = 0.0, longitude = 0.0)
        assertEquals("🌊", flood.getTypeEmoji())
        assertEquals("Flood", flood.getTypeDisplayName())

        val earthquake = IncidentReport(type = "EARTHQUAKE", title = "", description = "", latitude = 0.0, longitude = 0.0)
        assertEquals("🌋", earthquake.getTypeEmoji())
        assertEquals("Earthquake", earthquake.getTypeDisplayName())

        val landslide = IncidentReport(type = "LANDSLIDE", title = "", description = "", latitude = 0.0, longitude = 0.0)
        assertEquals("🪨", landslide.getTypeEmoji())
        assertEquals("Landslide", landslide.getTypeDisplayName())

        val fire = IncidentReport(type = "FIRE", title = "", description = "", latitude = 0.0, longitude = 0.0)
        assertEquals("🔥", fire.getTypeEmoji())
        assertEquals("Fire / Wildfire", fire.getTypeDisplayName())

        val roadBlock = IncidentReport(type = "ROUTE_BLOCK", title = "", description = "", latitude = 0.0, longitude = 0.0)
        assertEquals("🚧", roadBlock.getTypeEmoji())
        assertEquals("Road Blockage", roadBlock.getTypeDisplayName())

        val collapse = IncidentReport(type = "BUILDING_COLLAPSE", title = "", description = "", latitude = 0.0, longitude = 0.0)
        assertEquals("🏚️", collapse.getTypeEmoji())
        assertEquals("Building Collapse", collapse.getTypeDisplayName())

        val medical = IncidentReport(type = "MEDICAL", title = "", description = "", latitude = 0.0, longitude = 0.0)
        assertEquals("🏥", medical.getTypeEmoji())
        assertEquals("Medical Emergency", medical.getTypeDisplayName())
    }

    @Test
    fun `relative time returns human readable strings`() {
        val now = System.currentTimeMillis()

        val recent = IncidentReport(type = "OTHER", title = "", description = "", latitude = 0.0, longitude = 0.0, timestampMs = now - 10_000L)
        assertEquals("Just now", recent.getRelativeTime())

        val tenMinsAgo = IncidentReport(type = "OTHER", title = "", description = "", latitude = 0.0, longitude = 0.0, timestampMs = now - (10 * 60 * 1000L))
        assertEquals("10m ago", tenMinsAgo.getRelativeTime())

        val twoHoursAgo = IncidentReport(type = "OTHER", title = "", description = "", latitude = 0.0, longitude = 0.0, timestampMs = now - (2 * 60 * 60 * 1000L))
        assertEquals("2h ago", twoHoursAgo.getRelativeTime())
    }
}
