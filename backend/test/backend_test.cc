#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include "../models/SosEventDto.h"
#include "../models/BleMeshPacketDto.h"
#include "../storage/SqliteStorage.h"
#include "../filters/FirebaseAuthFilter.h"
#include "../spatial/DbscanClusterer.h"
#include "../dispatch/RescueResourceMatcher.h"
#include "../mesh/MeshIngestionPipeline.h"

using namespace georescux::backend;

void testSosEventDto() {
    std::cout << "[TEST] Running testSosEventDto..." << std::endl;

    // Valid event
    Json::Value validJson;
    validJson["eventId"] = "evt-12345";
    validJson["userId"] = "user-abc";
    validJson["startedAtMs"] = Json::Int64(1700000000000LL);
    validJson["isActive"] = true;
    validJson["latitude"] = 28.6139;
    validJson["longitude"] = 77.2090;
    validJson["note"] = "Trapped in room";

    std::string err;
    auto dto = models::SosEventDto::fromJson(validJson, err);
    assert(dto.has_value());
    assert(dto->eventId == "evt-12345");
    assert(dto->latitude.has_value() && *dto->latitude == 28.6139);
    assert(dto->note.has_value() && *dto->note == "Trapped in room");

    // Invalid latitude (> 90)
    Json::Value invalidLat = validJson;
    invalidLat["latitude"] = 99.9;
    auto badDto = models::SosEventDto::fromJson(invalidLat, err);
    assert(!badDto.has_value());
    assert(err.find("Invalid latitude") != std::string::npos);

    // Missing eventId
    Json::Value noId = validJson;
    noId.removeMember("eventId");
    auto noIdDto = models::SosEventDto::fromJson(noId, err);
    assert(!noIdDto.has_value());

    std::cout << "[PASS] testSosEventDto passed." << std::endl;
}

void testBleMeshPacketDto() {
    std::cout << "[TEST] Running testBleMeshPacketDto..." << std::endl;

    Json::Value packetJson;
    packetJson["packetId"] = "pkt-999";
    packetJson["originDeviceId"] = "dev-1";
    packetJson["originEmergencyId"] = "evt-12345";
    packetJson["timestampMs"] = Json::Int64(1700000000000LL);
    packetJson["ttl"] = 3;
    packetJson["type"] = "SOS";
    packetJson["latitude"] = 30.1;
    packetJson["longitude"] = 78.2;

    std::string err;
    auto dto = models::BleMeshPacketDto::fromJson(packetJson, err);
    assert(dto.has_value());
    assert(dto->packetId == "pkt-999");
    assert(dto->ttl == 3);
    assert(dto->latitude.has_value() && *dto->latitude == 30.1);

    std::cout << "[PASS] testBleMeshPacketDto passed." << std::endl;
}

void testSqliteStorage() {
    std::cout << "[TEST] Running testSqliteStorage..." << std::endl;

    storage::SqliteStorage store(":memory:");
    assert(store.init());

    models::SosEventDto ev;
    ev.eventId = "e-1";
    ev.userId = "u-1";
    ev.startedAtMs = 123456789;
    ev.isActive = true;
    ev.latitude = 30.3165;
    ev.longitude = 78.0322;
    ev.status = "CRITICAL";

    bool wasDup = false;
    assert(store.saveSosEvent(ev, wasDup));
    assert(!wasDup);

    // Duplicate check
    assert(store.saveSosEvent(ev, wasDup));
    assert(wasDup);

    auto retrieved = store.getSosEvent("e-1");
    assert(retrieved.has_value());
    assert(retrieved->status == "CRITICAL");
    assert(retrieved->latitude.has_value() && *retrieved->latitude == 30.3165);

    models::BleMeshPacketDto pkt;
    pkt.packetId = "mesh-pkt-1";
    pkt.originDeviceId = "phone-A";
    pkt.type = "TEST";

    bool pktDup = false;
    assert(store.saveMeshPacket(pkt, pktDup));
    assert(!pktDup);

    assert(store.saveMeshPacket(pkt, pktDup));
    assert(pktDup); // duplicate detected

    std::cout << "[PASS] testSqliteStorage passed." << std::endl;
}

void testFirebaseAuthVerification() {
    std::cout << "[TEST] Running testFirebaseAuthVerification..." << std::endl;

    std::string err;
    // 1. Malformed token (not 3 dots)
    auto r1 = filters::FirebaseAuthFilter::verifyFirebaseToken("invalid.token", "georescex", err);
    assert(!r1.has_value());
    assert(err.find("Malformed JWT") != std::string::npos);

    // 2. Unsigned token (empty signature segment)
    auto r2 = filters::FirebaseAuthFilter::verifyFirebaseToken("header.payload.", "georescex", err);
    assert(!r2.has_value());
    assert(err.find("Rejecting unverified") != std::string::npos);

    std::cout << "[PASS] testFirebaseAuthVerification passed." << std::endl;
}

void testDbscanClusterer() {
    std::cout << "[TEST] Running testDbscanClusterer..." << std::endl;

    std::vector<spatial::DistressPoint> points = {
        {"p1", 30.0001, 78.0001, 90, 2, -1},
        {"p2", 30.0002, 78.0002, 85, 1, -1},
        {"p3", 30.0003, 78.0003, 70, 3, -1},
        {"p_isolated", 30.1500, 78.5000, 30, 1, -1} // 20 km away
    };

    spatial::DbscanClusterer clusterer(500.0, 2);
    auto clusters = clusterer.cluster(points);

    assert(clusters.size() == 1);
    assert(clusters[0].totalPoints == 3);
    assert(clusters[0].totalPeopleCount == 6);
    assert(clusters[0].maxUrgencyScore == 90);

    std::cout << "[PASS] testDbscanClusterer passed." << std::endl;
}

void testRescueResourceMatcher() {
    std::cout << "[TEST] Running testRescueResourceMatcher..." << std::endl;

    std::vector<dispatch::RescueUnit> units = {
        {"u1", "Ambulance-1", dispatch::RescueUnitType::MEDICAL_AMBULANCE, 30.0010, 78.0010, 4, 50.0, true},
        {"u2", "NDRF-Alpha", dispatch::RescueUnitType::NDRF_SEARCH_RESCUE, 30.0500, 78.0500, 12, 40.0, true}
    };

    std::vector<spatial::DisasterCluster> clusters = {
        {1, 30.0002, 78.0002, 5, 5, 88.0, 92, 120.0, {"p1", "p2", "p3", "p4", "p5"}}
    };

    auto assignments = dispatch::RescueResourceMatcher::matchUnitsToClusters(units, clusters);
    assert(!assignments.empty());
    assert(assignments[0].unitId == "u1"); // Closer and specialized for medical emergency
    assert(assignments[0].targetClusterId == 1);

    std::cout << "[PASS] testRescueResourceMatcher passed." << std::endl;
}

void testMeshIngestionPipeline() {
    std::cout << "[TEST] Running testMeshIngestionPipeline..." << std::endl;

    std::atomic<size_t> receivedBatches{0};
    std::atomic<size_t> totalItems{0};

    mesh::MeshIngestionPipeline pipeline(1000, 5, 50);
    pipeline.start([&](const std::vector<mesh::IngestedPacket>& batch) {
        receivedBatches++;
        totalItems += batch.size();
    });

    for (int i = 0; i < 12; ++i) {
        mesh::IngestedPacket p{};
        p.gatewayId = "gw-1";
        p.senderId = "sender-" + std::to_string(i);
        p.sequenceNumber = static_cast<uint16_t>(i);
        p.latitude = 30.0;
        p.longitude = 78.0;
        assert(pipeline.enqueue(p));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    pipeline.stop();

    assert(totalItems.load() == 12);
    assert(pipeline.getTotalIngested() == 12);
    assert(pipeline.getTotalDropped() == 0);

    std::cout << "[PASS] testMeshIngestionPipeline passed." << std::endl;
}

int main() {
    std::cout << "Running GeoRescuX Backend Automated Tests..." << std::endl;
    testSosEventDto();
    testBleMeshPacketDto();
    testSqliteStorage();
    testFirebaseAuthVerification();
    testDbscanClusterer();
    testRescueResourceMatcher();
    testMeshIngestionPipeline();
    std::cout << "All backend tests completed successfully!" << std::endl;
    return 0;
}
