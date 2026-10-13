#include "BleMeshController.h"
#include <drogon/HttpResponse.h>

namespace georescux::backend::controllers {

BleMeshController::BleMeshController(std::shared_ptr<storage::SqliteStorage> storage)
    : storage_(std::move(storage)) {}

void BleMeshController::ingestPacket(
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

    auto jsonOpt = req->getJsonObject();
    if (!jsonOpt) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "invalid_json";
            r["message"] = "Invalid or unparseable JSON payload";
            return r;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    std::string parseError;
    auto packetOpt = models::BleMeshPacketDto::fromJson(*jsonOpt, parseError);
    if (!packetOpt) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([parseError] {
            Json::Value r;
            r["error"] = "validation_failed";
            r["message"] = parseError;
            return r;
        }());
        resp->setStatusCode(drogon::k400BadRequest);
        callback(resp);
        return;
    }

    auto packet = *packetOpt;
    bool wasDuplicate = false;
    bool saved = storage_->saveMeshPacket(packet, wasDuplicate);
    if (!saved) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "storage_failure";
            r["message"] = "Failed to durably persist mesh packet";
            return r;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
        return;
    }

    Json::Value result;
    result["status"] = wasDuplicate ? "duplicate_ignored" : "accepted";
    result["packetId"] = packet.packetId;
    result["isDuplicate"] = wasDuplicate;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    resp->setStatusCode(wasDuplicate ? drogon::k200OK : drogon::k201Created);
    callback(resp);
}

void BleMeshController::getPacket(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string packetId
) {
    if (!storage_) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "server_error";
            return r;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
        return;
    }

    auto packetOpt = storage_->getMeshPacket(packetId);
    if (!packetOpt) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "not_found";
            r["message"] = "Mesh packet not found";
            return r;
        }());
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(packetOpt->toJson());
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

} // namespace georescux::backend::controllers
