package com.example.georescux.data.backend

import com.example.georescux.domain.ble.GeoRescueBlePacket
import com.example.georescux.domain.sos.SosEmergency
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL

/**
 * Configuration for the standalone Drogon C++ Emergency Backend.
 *
 * Invariant: [baseUrl] is null or blank by default. The client is strictly
 * OPTIONAL and dormant until an explicit endpoint is provided.
 */
data class GeoRescueBackendConfig(
    val baseUrl: String? = null,
    val connectTimeoutMs: Int = 10_000,
    val readTimeoutMs: Int = 10_000,
) {
    val isConfigured: Boolean
        get() = !baseUrl.isNullOrBlank()
}

sealed interface BackendResult {
    data class Success(val status: String, val isDuplicate: Boolean) : BackendResult
    data class Failure(val statusCode: Int, val message: String) : BackendResult
    data object Disabled : BackendResult
}

/**
 * Typed client for communicating with the GeoRescuX Drogon C++ Backend.
 *
 * Requirements:
 * - Uses TLS for production connections and a configurable base URL.
 * - Reuses stable event IDs across retries for idempotency.
 * - Authenticates protected requests with Firebase ID tokens.
 * - Never throws: returns [BackendResult.Failure] on network errors.
 * - Preserves local-first offline architecture: backend unavailability
 *   never disrupts local SOS activation, local storage, or BLE relay.
 */
class GeoRescueBackendClient(
    private val config: GeoRescueBackendConfig = GeoRescueBackendConfig()
) {

    fun sendSosEvent(emergency: SosEmergency, idToken: String?): BackendResult {
        if (!config.isConfigured) return BackendResult.Disabled

        val endpoint = "${config.baseUrl!!.trimEnd('/')}/api/v1/sos/events"
        val payload = buildSosEventJson(emergency).toString()

        return executePost(endpoint, payload, idToken)
    }

    fun sendMeshPacket(packet: GeoRescueBlePacket, idToken: String?): BackendResult {
        if (!config.isConfigured) return BackendResult.Disabled

        val endpoint = "${config.baseUrl!!.trimEnd('/')}/api/v1/mesh/packets"
        val payload = buildMeshPacketJson(packet).toString()

        return executePost(endpoint, payload, idToken)
    }

    internal fun buildSosEventJson(emergency: SosEmergency): JSONObject =
        JSONObject().apply {
            put("eventId", emergency.id)
            put("startedAtMs", emergency.startedAtMs)
            put("stoppedAtMs", emergency.stoppedAtMs ?: 0L)
            put("isActive", emergency.isActive)
            emergency.location?.let {
                put("latitude", it.latitude)
                put("longitude", it.longitude)
            }
            emergency.locationStatus?.let { put("locationStatus", it.name) }
            emergency.note?.let { put("note", it) }
            put("source", "APP")
        }

    internal fun buildMeshPacketJson(packet: GeoRescueBlePacket): JSONObject =
        JSONObject().apply {
            put("packetId", packet.packetId)
            put("originDeviceId", packet.originDeviceId)
            put("originEmergencyId", packet.originEmergencyId ?: "")
            put("timestampMs", packet.timestampMs)
            put("ttl", packet.ttl)
            put("type", packet.type.name)
            packet.latitude?.let { put("latitude", it) }
            packet.longitude?.let { put("longitude", it) }
            packet.status?.let { put("status", it) }
            packet.noteText?.let { put("noteText", it) }
        }

    private fun executePost(endpointUrl: String, jsonPayload: String, idToken: String?): BackendResult {
        var connection: HttpURLConnection? = null
        return try {
            val url = URL(endpointUrl)
            connection = (url.openConnection() as HttpURLConnection).apply {
                requestMethod = "POST"
                connectTimeout = config.connectTimeoutMs
                readTimeout = config.readTimeoutMs
                doOutput = true
                setRequestProperty("Content-Type", "application/json; charset=UTF-8")
                setRequestProperty("Accept", "application/json")
                if (!idToken.isNullOrBlank()) {
                    setRequestProperty("Authorization", "Bearer $idToken")
                }
            }

            OutputStreamWriter(connection.outputStream, "UTF-8").use { writer ->
                writer.write(jsonPayload)
                writer.flush()
            }

            val statusCode = connection.responseCode
            val responseStream = if (statusCode in 200..299) {
                connection.inputStream
            } else {
                connection.errorStream ?: connection.inputStream
            }

            val responseBody = BufferedReader(InputStreamReader(responseStream, "UTF-8")).use {
                it.readText()
            }

            if (statusCode in 200..299) {
                val json = try { JSONObject(responseBody) } catch (_: Exception) { JSONObject() }
                val status = json.optString("status", "accepted")
                val isDup = json.optBoolean("isDuplicate", false)
                BackendResult.Success(status, isDup)
            } else {
                BackendResult.Failure(statusCode, responseBody)
            }
        } catch (e: Exception) {
            BackendResult.Failure(-1, e.message ?: "Connection failed")
        } finally {
            connection?.disconnect()
        }
    }
}
