package com.example.georescux.data.backend

import com.example.georescux.domain.ble.GeoRescueBlePacket
import com.example.georescux.domain.ble.GeoRescueBlePacketType
import com.example.georescux.domain.sos.SosEmergency
import com.example.georescux.domain.sos.SosLocation
import com.example.georescux.domain.sos.SosLocationStatus
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class GeoRescueBackendClientTest {

    @Test
    fun `unconfigured client returns Disabled and does not connect`() {
        val client = GeoRescueBackendClient(GeoRescueBackendConfig(baseUrl = null))
        val emergency = SosEmergency(id = "sos-1", startedAtMs = 1000L)

        val result = client.sendSosEvent(emergency, idToken = "dummy-token")
        assertEquals(BackendResult.Disabled, result)
    }

    @Test
    fun `blank baseUrl returns Disabled`() {
        val client = GeoRescueBackendClient(GeoRescueBackendConfig(baseUrl = "   "))
        val emergency = SosEmergency(id = "sos-1", startedAtMs = 1000L)

        val result = client.sendSosEvent(emergency, idToken = "dummy-token")
        assertEquals(BackendResult.Disabled, result)
    }

    @Test
    fun `buildSosEventJson serializes all emergency fields accurately`() {
        val client = GeoRescueBackendClient()
        val location = SosLocation(latitude = 28.6139, longitude = 77.2090, accuracyMeters = 5.0f, timestampMs = 1000L, provider = "gps")
        val emergency = SosEmergency(
            id = "sos-test-123",
            startedAtMs = 1000L,
            stoppedAtMs = 2000L,
            location = location,
            locationStatus = SosLocationStatus.ACQUIRED,
            note = "Need medical help"
        )

        val json = client.buildSosEventJson(emergency)
        assertEquals("sos-test-123", json.getString("eventId"))
        assertEquals(1000L, json.getLong("startedAtMs"))
        assertEquals(2000L, json.getLong("stoppedAtMs"))
        assertEquals(false, json.getBoolean("isActive"))
        assertEquals(28.6139, json.getDouble("latitude"), 0.0001)
        assertEquals(77.2090, json.getDouble("longitude"), 0.0001)
        assertEquals("ACQUIRED", json.getString("locationStatus"))
        assertEquals("Need medical help", json.getString("note"))
        assertEquals("APP", json.getString("source"))
    }

    @Test
    fun `buildMeshPacketJson serializes mesh packet matching wire format`() {
        val client = GeoRescueBackendClient()
        val packet = GeoRescueBlePacket(
            packetId = "pkt-mesh-456",
            originDeviceId = "phone-dev-A",
            originEmergencyId = "sos-test-123",
            timestampMs = 5000L,
            ttl = 2,
            type = GeoRescueBlePacketType.SOS,
            latitude = 30.123,
            longitude = 78.456,
            status = "ACTIVE",
            noteText = "Relayed packet"
        )

        val json = client.buildMeshPacketJson(packet)
        assertEquals("pkt-mesh-456", json.getString("packetId"))
        assertEquals("phone-dev-A", json.getString("originDeviceId"))
        assertEquals("sos-test-123", json.getString("originEmergencyId"))
        assertEquals(5000L, json.getLong("timestampMs"))
        assertEquals(2, json.getInt("ttl"))
        assertEquals("SOS", json.getString("type"))
        assertEquals(30.123, json.getDouble("latitude"), 0.001)
        assertEquals(78.456, json.getDouble("longitude"), 0.001)
        assertEquals("ACTIVE", json.getString("status"))
        assertEquals("Relayed packet", json.getString("noteText"))
    }

    @Test
    fun `unreachable server endpoint returns Failure without throwing exception`() {
        // Point to an unavailable local port
        val client = GeoRescueBackendClient(
            GeoRescueBackendConfig(baseUrl = "http://127.0.0.1:54321", connectTimeoutMs = 500)
        )
        val emergency = SosEmergency(id = "sos-unreachable", startedAtMs = 1000L)

        val result = client.sendSosEvent(emergency, idToken = "test-token")
        assertTrue(result is BackendResult.Failure)
        val failure = result as BackendResult.Failure
        assertEquals(-1, failure.statusCode)
    }
}
