#include "HealthController.h"
#include <drogon/HttpResponse.h>
#include <chrono>

namespace georescux::backend::controllers {

void HealthController::getHealth(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
) {
    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    Json::Value root;
    root["status"] = "UP";
    root["service"] = "GeoRescuX Drogon C++ Emergency Backend";
    root["version"] = "1.0.0";
    root["timestampMs"] = Json::Int64(nowMs);
    root["schemaVersion"] = "2026-v1";

    auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

} // namespace georescux::backend::controllers
