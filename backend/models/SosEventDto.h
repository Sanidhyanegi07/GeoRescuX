#pragma once

#include <string>
#include <optional>
#include <json/json.h>

namespace georescux::backend::models {

/**
 * Data Transfer Object for Emergency SOS events.
 * Matches client SosEmergency model and BleSosBridge schema.
 */
struct SosEventDto {
    std::string eventId;       // Stable ID for idempotency (e.g. UUID)
    std::string userId;        // Firebase UID or origin device ID
    long long startedAtMs{0};
    long long stoppedAtMs{0};
    bool isActive{true};
    std::optional<double> latitude;
    std::optional<double> longitude;
    std::string locationStatus{"UNKNOWN"};
    std::optional<std::string> note;
    std::string source{"APP"}; // "APP" or "BLE_MESH_RELAY"
    long long receivedAtMs{0};

    static std::optional<SosEventDto> fromJson(const Json::Value& root, std::string& errorMsg) {
        if (!root.isObject()) {
            errorMsg = "Root must be a JSON object";
            return std::nullopt;
        }

        if (!root.isMember("eventId") || !root["eventId"].isString() || root["eventId"].asString().empty()) {
            errorMsg = "Missing or empty 'eventId'";
            return std::nullopt;
        }
        if (root["eventId"].asString().length() > 128) {
            errorMsg = "'eventId' exceeds maximum size of 128 characters";
            return std::nullopt;
        }

        SosEventDto dto;
        dto.eventId = root["eventId"].asString();
        dto.userId = root.get("userId", "").asString();
        dto.startedAtMs = root.get("startedAtMs", 0).asInt64();
        dto.stoppedAtMs = root.get("stoppedAtMs", 0).asInt64();
        dto.isActive = root.get("isActive", true).asBool();

        if (root.isMember("latitude") && root["latitude"].isNumeric()) {
            dto.latitude = root["latitude"].asDouble();
            if (*dto.latitude < -90.0 || *dto.latitude > 90.0) {
                errorMsg = "Invalid latitude value (must be between -90 and 90)";
                return std::nullopt;
            }
        }
        if (root.isMember("longitude") && root["longitude"].isNumeric()) {
            dto.longitude = root["longitude"].asDouble();
            if (*dto.longitude < -180.0 || *dto.longitude > 180.0) {
                errorMsg = "Invalid longitude value (must be between -180 and 180)";
                return std::nullopt;
            }
        }

        dto.locationStatus = root.get("locationStatus", "UNKNOWN").asString();
        if (root.isMember("note") && root["note"].isString()) {
            std::string n = root["note"].asString();
            if (n.length() > 500) {
                errorMsg = "'note' exceeds maximum size of 500 characters";
                return std::nullopt;
            }
            dto.note = n;
        }

        dto.source = root.get("source", "APP").asString();
        return dto;
    }

    Json::Value toJson() const {
        Json::Value val;
        val["eventId"] = eventId;
        val["userId"] = userId;
        val["startedAtMs"] = Json::Int64(startedAtMs);
        val["stoppedAtMs"] = Json::Int64(stoppedAtMs);
        val["isActive"] = isActive;
        if (latitude) val["latitude"] = *latitude;
        else val["latitude"] = Json::nullValue;
        if (longitude) val["longitude"] = *longitude;
        else val["longitude"] = Json::nullValue;
        val["locationStatus"] = locationStatus;
        if (note) val["note"] = *note;
        else val["note"] = Json::nullValue;
        val["source"] = source;
        val["receivedAtMs"] = Json::Int64(receivedAtMs);
        return val;
    }
};

} // namespace georescux::backend::models
