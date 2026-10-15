#include "ClusterController.h"
#include <drogon/HttpResponse.h>

namespace georescux::backend::controllers {

ClusterController::ClusterController(std::shared_ptr<storage::SqliteStorage> storage)
    : storage_(std::move(storage)) {}

void ClusterController::getClusters(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    if (!storage_) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "server_error";
            r["message"] = "Storage layer uninitialized";
            return r;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
        return;
    }

    const auto events = storage_->getAllSosEvents();
    std::vector<spatial::DistressPoint> points;
    points.reserve(events.size());

    for (const auto& ev : events) {
        points.push_back(spatial::DistressPoint{
            .id = ev.eventId,
            .latitude = ev.latitude,
            .longitude = ev.longitude,
            .urgencyScore = static_cast<uint8_t>(ev.status == "CRITICAL" ? 90 : (ev.status == "HIGH" ? 70 : 40)),
            .peopleCount = 1,
            .clusterId = -1
        });
    }

    spatial::DbscanClusterer clusterer(500.0, 2);
    auto clusters = clusterer.cluster(points);

    Json::Value root;
    root["status"] = "success";
    root["totalClusters"] = static_cast<Json::UInt>(clusters.size());
    root["totalDistressPoints"] = static_cast<Json::UInt>(points.size());

    Json::Value clusterArray(Json::arrayValue);
    for (const auto& c : clusters) {
        Json::Value item;
        item["clusterId"] = c.clusterId;
        item["centroidLat"] = c.centroidLat;
        item["centroidLon"] = c.centroidLon;
        item["totalPoints"] = static_cast<Json::UInt>(c.totalPoints);
        item["totalPeopleCount"] = c.totalPeopleCount;
        item["averageUrgencyScore"] = c.averageUrgencyScore;
        item["maxUrgencyScore"] = c.maxUrgencyScore;
        item["radiusMeters"] = c.radiusMeters;

        Json::Value idArr(Json::arrayValue);
        for (const auto& id : c.distressIds) {
            idArr.append(id);
        }
        item["distressIds"] = idArr;
        clusterArray.append(item);
    }
    root["clusters"] = clusterArray;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
    callback(resp);
}

void ClusterController::matchDispatch(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    auto jsonOpt = req->getJsonObject();
    if (!jsonOpt || !(*jsonOpt).isMember("units")) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "invalid_payload";
            r["message"] = "Missing 'units' array in request body";
            return r;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::vector<dispatch::RescueUnit> units;
    const auto& unitsJson = (*jsonOpt)["units"];
    for (Json::ArrayIndex i = 0; i < unitsJson.size(); ++i) {
        const auto& u = unitsJson[i];
        dispatch::RescueUnit unit{};
        unit.unitId = u.get("unitId", "UNIT-" + std::to_string(i)).asString();
        unit.unitName = u.get("unitName", "Ambulance").asString();
        unit.type = dispatch::RescueUnitType::MEDICAL_AMBULANCE;
        unit.currentLat = u.get("lat", 0.0).asDouble();
        unit.currentLon = u.get("lon", 0.0).asDouble();
        unit.capacityPeople = static_cast<uint16_t>(u.get("capacity", 4).asUInt());
        unit.operationalSpeedKmh = u.get("speedKmh", 45.0).asDouble();
        unit.isAvailable = u.get("isAvailable", true).asBool();
        units.push_back(unit);
    }

    // Query clusters
    std::vector<spatial::DistressPoint> points;
    if (storage_) {
        const auto events = storage_->getAllSosEvents();
        for (const auto& ev : events) {
            points.push_back(spatial::DistressPoint{
                .id = ev.eventId,
                .latitude = ev.latitude,
                .longitude = ev.longitude,
                .urgencyScore = 80,
                .peopleCount = 1,
                .clusterId = -1
            });
        }
    }

    spatial::DbscanClusterer clusterer(1000.0, 1);
    auto clusters = clusterer.cluster(points);

    auto assignments = dispatch::RescueResourceMatcher::matchUnitsToClusters(units, clusters);

    Json::Value root;
    root["status"] = "success";
    root["totalAssignments"] = static_cast<Json::UInt>(assignments.size());

    Json::Value assignArr(Json::arrayValue);
    for (const auto& a : assignments) {
        Json::Value item;
        item["unitId"] = a.unitId;
        item["unitName"] = a.unitName;
        item["targetClusterId"] = a.targetClusterId;
        item["targetLat"] = a.targetLat;
        item["targetLon"] = a.targetLon;
        item["estimatedDistanceKm"] = a.estimatedDistanceKm;
        item["estimatedEtaMinutes"] = a.estimatedEtaMinutes;
        item["peopleCovered"] = a.peopleCovered;
        item["matchScore"] = a.matchScore;
        assignArr.append(item);
    }
    root["assignments"] = assignArr;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
    callback(resp);
}

} // namespace georescux::backend::controllers
