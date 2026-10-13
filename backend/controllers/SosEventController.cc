#include "SosEventController.h"
#include <drogon/HttpResponse.h>
#include <iostream>

namespace georescux::backend::controllers {

SosEventController::SosEventController(std::shared_ptr<storage::SqliteStorage> storage)
    : storage_(std::move(storage)) {}

void SosEventController::ingestEvent(
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

    if (req->body().length() > 65536) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "payload_too_large";
            r["message"] = "SOS payload exceeds maximum limit of 64KB";
            return r;
        }());
        resp->setStatusCode(drogon::k413RequestEntityTooLarge);
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
    auto eventOpt = models::SosEventDto::fromJson(*jsonOpt, parseError);
    if (!eventOpt) {
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

    auto event = *eventOpt;
    // Set authenticated user ID if not provided in payload
    auto authUid = req->attributes()->get<std::string>("auth_uid");
    if (authUid && event.userId.empty()) {
        event.userId = *authUid;
    }

    bool wasDuplicate = false;
    bool saved = storage_->saveSosEvent(event, wasDuplicate);
    if (!saved) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "storage_failure";
            r["message"] = "Failed to durably persist emergency event";
            return r;
        }());
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
        return;
    }

    Json::Value result;
    result["status"] = wasDuplicate ? "updated" : "accepted";
    result["eventId"] = event.eventId;
    result["isDuplicate"] = wasDuplicate;
    result["persistedDurable"] = true;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
    resp->setStatusCode(wasDuplicate ? drogon::k200OK : drogon::k201Created);
    callback(resp);
}

void SosEventController::getEvent(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    std::string eventId
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

    auto eventOpt = storage_->getSosEvent(eventId);
    if (!eventOpt) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse([] {
            Json::Value r;
            r["error"] = "not_found";
            r["message"] = "SOS event not found";
            return r;
        }());
        resp->setStatusCode(drogon::k404NotFound);
        callback(resp);
        return;
    }

    auto resp = drogon::HttpResponse::newHttpJsonResponse(eventOpt->toJson());
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

void SosEventController::getAdminEvents(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback
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

    auto events = storage_->getAllSosEvents(100);
    Json::Value root;
    Json::Value eventsArr(Json::arrayValue);
    for (const auto& ev : events) {
        eventsArr.append(ev.toJson());
    }
    root["count"] = static_cast<int>(events.size());
    root["events"] = eventsArr;

    auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
    resp->setStatusCode(drogon::k200OK);
    callback(resp);
}

} // namespace georescux::backend::controllers
