#include <iostream>
#include <cassert>
#include <memory>
#include <cmath>
#include <limits>
#include <QCoreApplication>

#include "domain/RouteService.h"
#include "domain/OfflineRouteRepository.h"
#include "domain/MapPackageManager.h"
#include "platform/storage/LocalDatabase.h"
#include "platform/storage/SqliteMapPackageRepository.h"

using namespace georescux::domain;
using namespace georescux::storage;
using namespace georescux::routing;

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] Running Phase 7 Offline Routing & Safe Route Tests" << std::endl;
    std::cout << "=================================================================" << std::endl;

    auto db = std::make_shared<LocalDatabase>();
    assert(db->initialize(":memory:"));
    auto pkgRepo = std::make_shared<SqliteMapPackageRepository>(db);
    auto pkgMgr = std::make_shared<MapPackageManager>(pkgRepo);
    auto routeRepo = std::make_shared<OfflineRouteRepository>(pkgMgr);
    RouteService service(routeRepo, pkgMgr);

    // Build a deterministic controlled test graph
    // Topology:
    // N1 (30.0, 78.0) --- 1000m --- N2 (30.009, 78.0) --- 2000m --- N3 (30.027, 78.0)
    //                                  |
    //                                1500m
    //                                  |
    //                                N4 (30.009, 78.015) [Island/Dead-end]
    // Unconnected node: N_ISLAND (30.5, 78.5)
    std::vector<RouteNode> testNodes = {
        RouteNode{"N1", 30.0, 78.0, false},
        RouteNode{"N2", 30.009, 78.0, false},
        RouteNode{"N3", 30.027, 78.0, true},
        RouteNode{"N4", 30.009, 78.015, false},
        RouteNode{"N_ISLAND", 30.5, 78.5, false}
    };

    std::vector<RouteEdge> testEdges = {
        RouteEdge{"N1", "N2", 1000.0, false},
        RouteEdge{"N2", "N3", 2000.0, false},
        RouteEdge{"N2", "N4", 1500.0, false}
    };

    RouteGraph validGraph(testNodes, testEdges, {}, 1);
    routeRepo->seedGraph(QStringLiteral("test_region"), validGraph);

    // Test 1: Known Valid Route in a Small Controlled Graph
    {
        std::cout << "[Test 1] Testing known valid route in controlled graph..." << std::endl;
        auto res = service.computeRouteDirect(QStringLiteral("test_region"), 30.0, 78.0, 30.027, 78.0);
        assert(res.success);
        assert(res.nodeIds.size() == 3);
        assert(res.nodeIds.at(0) == "N1");
        assert(res.nodeIds.at(1) == "N2");
        assert(res.nodeIds.at(2) == "N3");
        // Distance: 1000m + 2000m = 3000m
        assert(std::abs(res.totalDistanceMeters - 3000.0) < 1e-3);
        assert(res.pathCoordinates.size() == 3);
        assert(res.stepInstructions.size() == 2);
        std::cout << "  -> PASSED: Valid route N1 -> N2 -> N3 found with exact 3000m distance." << std::endl;
    }

    // Test 2: Start Equals Destination
    {
        std::cout << "[Test 2] Testing start equals destination..." << std::endl;
        auto res = service.computeRouteDirect(QStringLiteral("test_region"), 30.0, 78.0, 30.0, 78.0);
        assert(res.success);
        assert(res.totalDistanceMeters == 0.0);
        assert(res.totalCostMeters == 0.0);
        assert(!res.pathCoordinates.isEmpty());
        std::cout << "  -> PASSED: Start == destination returns 0 distance immediately." << std::endl;
    }

    // Test 3: Invalid Coordinates Rejection (NaN, Infinite, Out-of-bounds)
    {
        std::cout << "[Test 3] Testing invalid coordinate rejection..." << std::endl;
        const double qnan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();

        auto resNaN = service.computeRouteDirect(QStringLiteral("test_region"), qnan, 78.0, 30.0, 78.0);
        assert(!resNaN.success);
        assert(resNaN.statusMessage.contains("Invalid start coordinates"));

        auto resInf = service.computeRouteDirect(QStringLiteral("test_region"), 30.0, inf, 30.0, 78.0);
        assert(!resInf.success);

        auto resOOB = service.computeRouteDirect(QStringLiteral("test_region"), 95.0, 78.0, 30.0, 78.0);
        assert(!resOOB.success);
        std::cout << "  -> PASSED: Invalid coordinates safely rejected." << std::endl;
    }

    // Test 4: Missing Graph Rejection
    {
        std::cout << "[Test 4] Testing missing graph rejection..." << std::endl;
        auto res = service.computeRouteDirect(QStringLiteral("nonexistent_region"), 30.0, 78.0, 30.027, 78.0);
        assert(!res.success);
        assert(res.statusMessage.contains("Missing routing graph"));
        std::cout << "  -> PASSED: Missing graph reported with structured error." << std::endl;
    }

    // Test 5: Empty Graph Rejection
    {
        std::cout << "[Test 5] Testing empty graph rejection..." << std::endl;
        RouteGraph emptyGraph({}, {}, {}, 1);
        routeRepo->seedGraph(QStringLiteral("empty_region"), emptyGraph);
        auto res = service.computeRouteDirect(QStringLiteral("empty_region"), 30.0, 78.0, 30.027, 78.0);
        assert(!res.success);
        assert(res.statusMessage.contains("is empty"));
        std::cout << "  -> PASSED: Empty graph rejected." << std::endl;
    }

    // Test 6: Unsupported Graph Version Rejection
    {
        std::cout << "[Test 6] Testing unsupported graph version rejection..." << std::endl;
        RouteGraph badVerGraph(testNodes, testEdges, {}, -1);
        routeRepo->seedGraph(QStringLiteral("bad_ver_region"), badVerGraph);
        auto res = service.computeRouteDirect(QStringLiteral("bad_ver_region"), 30.0, 78.0, 30.027, 78.0);
        assert(!res.success);
        assert(res.statusMessage.contains("Unsupported graph version"));
        std::cout << "  -> PASSED: Unsupported negative version rejected." << std::endl;
    }

    // Test 7: Unreachable Destination Detection
    {
        std::cout << "[Test 7] Testing unreachable destination detection..." << std::endl;
        // N_ISLAND is not connected by any edge to N1
        auto res = service.computeRouteDirect(QStringLiteral("test_region"), 30.0, 78.0, 30.5, 78.5);
        assert(!res.success);
        assert(res.statusMessage.contains("unreachable"));
        std::cout << "  -> PASSED: Unreachable destination detected." << std::endl;
    }

    // Test 8: Endpoints Outside Snapping Tolerance Coverage
    {
        std::cout << "[Test 8] Testing endpoint outside snapping tolerance..." << std::endl;
        // Coordinate in Pacific Ocean (0.0, 0.0) is thousands of km away from graph
        auto res = service.computeRouteDirect(QStringLiteral("test_region"), 0.0, 0.0, 30.027, 78.0);
        assert(!res.success);
        assert(res.statusMessage.contains("outside graph coverage"));
        std::cout << "  -> PASSED: Out-of-coverage coordinates rejected by snapping tolerance." << std::endl;
    }

    // Test 9: Snapping Tolerance Configuration
    {
        std::cout << "[Test 9] Testing snapping tolerance threshold behavior..." << std::endl;
        // Test point 200m away from N1 (30.0018, 78.0)
        // With 100m tolerance -> should fail
        service.setSnappingToleranceMeters(100.0);
        auto snapFail = service.snapToNearestNode(QStringLiteral("test_region"), 30.005, 78.0, 100.0);
        assert(!snapFail.has_value());

        // With 1000m tolerance -> should snap to N1 or N2
        auto snapSuccess = service.snapToNearestNode(QStringLiteral("test_region"), 30.005, 78.0, 1000.0);
        assert(snapSuccess.has_value());
        service.setSnappingToleranceMeters(5000.0); // Reset default
        std::cout << "  -> PASSED: Configurable snapping tolerance works as expected." << std::endl;
    }

    // Test 10: Invalid Edge Weights Rejection
    {
        std::cout << "[Test 10] Testing invalid negative edge weight rejection..." << std::endl;
        std::vector<RouteEdge> badEdges = {
            RouteEdge{"N1", "N2", -500.0, false}
        };
        RouteGraph negGraph(testNodes, badEdges, {}, 1);
        routeRepo->seedGraph(QStringLiteral("neg_edge_region"), negGraph);
        auto res = service.computeRouteDirect(QStringLiteral("neg_edge_region"), 30.0, 78.0, 30.009, 78.0);
        assert(!res.success);
        assert(res.statusMessage.contains("invalid or negative edge distance"));
        std::cout << "  -> PASSED: Negative edge weights rejected." << std::endl;
    }

    // Test 11: Route Geometry Continuity
    {
        std::cout << "[Test 11] Testing route geometry continuity..." << std::endl;
        auto res = service.computeRouteDirect(QStringLiteral("test_region"), 30.0, 78.0, 30.027, 78.0);
        assert(res.success);
        // Verify every successive pair of nodes has an edge connecting them in the graph
        for (int i = 0; i + 1 < res.nodeIds.size(); ++i) {
            auto edge = validGraph.edgeBetween(res.nodeIds.at(i).toStdString(), res.nodeIds.at(i + 1).toStdString());
            assert(edge.has_value());
            assert(edge->distanceMeters > 0.0);
        }
        std::cout << "  -> PASSED: Route geometry is strictly contiguous with no leaps." << std::endl;
    }

    // Test 12: Correct Distance Calculation
    {
        std::cout << "[Test 12] Testing exact distance calculation..." << std::endl;
        auto resBranch = service.computeRouteDirect(QStringLiteral("test_region"), 30.0, 78.0, 30.009, 78.015);
        assert(resBranch.success);
        // N1 -> N2 (1000m) + N2 -> N4 (1500m) = 2500m
        assert(std::abs(resBranch.totalDistanceMeters - 2500.0) < 1e-3);
        std::cout << "  -> PASSED: Multi-hop distance matches sum of edges (2500m)." << std::endl;
    }

    // Test 13: Region Isolation
    {
        std::cout << "[Test 13] Testing region isolation..." << std::endl;
        std::vector<RouteNode> reg2Nodes = { RouteNode{"R2_A", 12.0, 15.0, false}, RouteNode{"R2_B", 12.01, 15.0, false} };
        std::vector<RouteEdge> reg2Edges = { RouteEdge{"R2_A", "R2_B", 800.0, false} };
        RouteGraph reg2Graph(reg2Nodes, reg2Edges, {}, 1);
        routeRepo->seedGraph(QStringLiteral("region_other"), reg2Graph);

        // Attempting to route using N1 (from test_region) in region_other must fail
        auto resIso = service.calculateRouteByNodes(QStringLiteral("region_other"), QStringLiteral("N1"), QStringLiteral("R2_B"));
        assert(!resIso);
        std::cout << "  -> PASSED: Cross-region node leakage is completely prevented." << std::endl;
    }

    // Test 14: Repeated Route Calculations (Idempotent and Memoized)
    {
        std::cout << "[Test 14] Testing repeated route calculations..." << std::endl;
        for (int i = 0; i < 5; ++i) {
            assert(service.calculateRoute(QStringLiteral("test_region"), 30.0, 78.0, 30.027, 78.0));
            assert(service.hasRoute());
            assert(std::abs(service.lastDistanceMeters() - 3000.0) < 1e-3);
        }
        std::cout << "  -> PASSED: Repeated queries execute deterministically and idempotently." << std::endl;
    }

    // Test 15: Graph Persistence & Reload in OfflineRouteRepository
    {
        std::cout << "[Test 15] Testing graph caching and persistence in repository..." << std::endl;
        assert(routeRepo->hasGraph(QStringLiteral("test_region")));
        auto cached = routeRepo->getGraph(QStringLiteral("test_region"));
        assert(cached.has_value());
        assert(cached->nodes.size() == 5);
        assert(cached->edges.size() == 3);
        std::cout << "  -> PASSED: Graph caching verified." << std::endl;
    }

    // Test 16: No False Route Success After Routing Error
    {
        std::cout << "[Test 16] Testing no false route success state after an error..." << std::endl;
        // First compute valid route
        assert(service.calculateRoute(QStringLiteral("test_region"), 30.0, 78.0, 30.027, 78.0));
        assert(service.hasRoute());

        // Then execute invalid route query (unreachable island)
        bool secondRes = service.calculateRoute(QStringLiteral("test_region"), 30.0, 78.0, 30.5, 78.5);
        assert(!secondRes);
        // Previous route state must NOT persist as successful
        assert(!service.hasRoute());
        assert(service.lastDistanceMeters() == 0.0);
        assert(service.lastCoordinates().isEmpty());
        std::cout << "  -> PASSED: Failed route cleanly clears route state with no false positives." << std::endl;
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] ALL 16 PHASE 7 ROUTING TESTS PASSED!" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return 0;
}
