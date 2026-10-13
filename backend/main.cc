#include <drogon/drogon.h>
#include <iostream>
#include <memory>
#include "storage/SqliteStorage.h"
#include "controllers/HealthController.h"
#include "controllers/SosEventController.h"
#include "controllers/BleMeshController.h"

int main() {
    std::cout << "Starting GeoRescuX Drogon C++ Emergency Backend..." << std::endl;

    auto storage = std::make_shared<georescux::backend::storage::SqliteStorage>("georescux_backend.db");
    if (!storage->init()) {
        std::cerr << "FATAL: Failed to initialize SQLite storage." << std::endl;
        return 1;
    }
    std::cout << "SQLite durable storage initialized successfully." << std::endl;

    // Load Drogon configuration
    try {
        drogon::app().loadConfigFile("config.json");
    } catch (const std::exception& e) {
        std::cout << "Notice: config.json not loaded (" << e.what() << "). Using default settings (port 8080)." << std::endl;
        drogon::app().addListener("0.0.0.0", 8080);
    }

    std::cout << "GeoRescuX Drogon backend listening on port 8080." << std::endl;
    drogon::app().run();
    return 0;
}
