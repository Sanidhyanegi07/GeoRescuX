#pragma once

#include <string>
#include <optional>
#include <json/json.h>

namespace georescux::backend::models {

/**
 * Data Transfer Object for BLE Mesh store-and-forward packets.
 * Matches client GeoRescueBlePacket wire schema.
 */
struct BleMeshPacketDto {
    std::string packetId;
    std::string originDeviceId;
    std::string originEmergencyId;
    long long timestampMs{0};
    int ttl{3};
    std::string type{"SOS"}; // "SOS", "ACK", "SYNC", "TEST"
    std::optional<double> latitude;
    std::optional<double> longitude;
    std::string status{""};
    std::optional<std::string> noteText;
    long long ingestedAtMs{0};

    static std::optional<BleMeshPacketDto> fromJson(const Json::Value& root, std::string& errorMsg) {
        if (!root.isObject()) {
            errorMsg = "Root must be a JSON object";
            return std::nullopt;
        }

        if (!root.isMember("packetId") || !root["packetId"].isString() || root["packetId"].asString().empty()) {
            errorMsg = "Missing or empty 'packetId'";
            return std::nullopt;
        }
        if (root["packetId"].asString().length() > 128) {
            errorMsg = "'packetId' exceeds maximum length of 128 characters";
            return std::nullopt;
        }

        if (!root.isMember("originDeviceId") || !root["originDeviceId"].isString()) {
            errorMsg = "Missing 'originDeviceId'";
            return std::nullopt;
        }

        BleMeshPacketDto dto;
        dto.packetId = root["packetId"].asString();
        dto.originDeviceId = root["originDeviceId"].asString();
        dto.originEmergencyId = root.get("originEmergencyId", "").asString();
        dto.timestampMs = root.get("timestampMs", 0).asInt64();
        dto.ttl = root.get("ttl", 3).asInt();
        dto.type = root.get("type", "SOS").asString();

        if (root.isMember("latitude") && root["latitude"].isNumeric()) {
            dto.latitude = root["latitude"].asDouble();
        }
        if (root.isMember("longitude") && root["longitude"].isNumeric()) {
            dto.longitude = root["longitude"].asDouble();
        }
        dto.status = root.get("status", "").asString();
        if (root.isMember("noteText") && root["noteText"].isString()) {
            dto.noteText = root["noteText"].asString();
        }

        return dto;
    }

    Json::Value toJson() const {
        Json::Value val;
        val["packetId"] = packetId;
        val["originDeviceId"] = originDeviceId;
        val["originEmergencyId"] = originEmergencyId;
        val["timestampMs"] = Json::Int64(timestampMs);
        val["ttl"] = ttl;
        val["type"] = type;
        if (latitude) val["latitude"] = *latitude;
        else val["latitude"] = Json::nullValue;
        if (longitude) val["longitude"] = *longitude;
        else val["longitude"] = Json::nullValue;
        val["status"] = status;
        if (noteText) val["noteText"] = *noteText;
        else val["noteText"] = Json::nullValue;
        val["ingestedAtMs"] = Json::Int64(ingestedAtMs);
        return val;
    }
};

} // namespace georescux::backend::models
