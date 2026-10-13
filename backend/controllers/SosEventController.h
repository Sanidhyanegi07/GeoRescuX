#pragma once

#include <drogon/HttpController.h>
#include <memory>
#include "../storage/SqliteStorage.h"

namespace georescux::backend::controllers {

class SosEventController : public drogon::HttpController<SosEventController> {
public:
    explicit SosEventController(std::shared_ptr<storage::SqliteStorage> storage = nullptr);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SosEventController::ingestEvent, "/api/v1/sos/events", drogon::Post, "georescux::backend::filters::FirebaseAuthFilter");
    ADD_METHOD_TO(SosEventController::getEvent, "/api/v1/sos/events/{1}", drogon::Get, "georescux::backend::filters::FirebaseAuthFilter");
    ADD_METHOD_TO(SosEventController::getAdminEvents, "/api/v1/admin/sos/events", drogon::Get, "georescux::backend::filters::FirebaseAuthFilter", "georescux::backend::filters::FirebaseAdminFilter");
    METHOD_LIST_END

    void ingestEvent(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getEvent(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        std::string eventId
    );

    void getAdminEvents(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void setStorage(std::shared_ptr<storage::SqliteStorage> storage) {
        storage_ = std::move(storage);
    }

private:
    std::shared_ptr<storage::SqliteStorage> storage_;
};

} // namespace georescux::backend::controllers
