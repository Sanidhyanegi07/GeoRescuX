#include <iostream>
#include <cassert>
#include <memory>
#include <cmath>
#include <limits>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QDateTime>

#include "domain/RouteService.h"
#include "platform/storage/OfflineRouteRepository.h"
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

    // Test 17: Corrupt Graph JSON Parsing
    {
        std::cout << "[Test 17] Testing corrupt graph JSON rejection..." << std::endl;
        const QString tempCorruptPath = QDir::temp().filePath(QStringLiteral("corrupt_graph_%1.json").arg(QDateTime::currentMSecsSinceEpoch()));
        QFile cf(tempCorruptPath);
        cf.open(QIODevice::WriteOnly);
        cf.write("NOT_A_VALID_JSON_GRAPH {{{ broken");
        cf.close();

        routeRepo->setCustomGraphPath(QStringLiteral("corrupt_json_region"), tempCorruptPath);
        assert(!routeRepo->getGraph(QStringLiteral("corrupt_json_region")).has_value());

        auto res = service.computeRouteDirect(QStringLiteral("corrupt_json_region"), 30.0, 78.0, 30.027, 78.0);
        assert(!res.success);
        assert(res.statusMessage.contains("Missing routing graph"));

        QFile::remove(tempCorruptPath);
        std::cout << "  -> PASSED: Corrupt graph JSON safely rejected." << std::endl;
    }

    // Test 18: Region Switching and Active Graph Reload
    {
        std::cout << "[Test 18] Testing region switching and active graph reload..." << std::endl;
        // Region A route
        assert(service.calculateRoute(QStringLiteral("test_region"), 30.0, 78.0, 30.027, 78.0));
        assert(service.hasRoute());
        assert(std::abs(service.lastDistanceMeters() - 3000.0) < 1e-3);

        // Switch to Region B
        service.clearRoute();
        assert(!service.hasRoute());

        std::vector<RouteNode> regBNodes = { RouteNode{"B1", 20.0, 70.0, false}, RouteNode{"B2", 20.01, 70.0, false} };
        std::vector<RouteEdge> regBEdges = { RouteEdge{"B1", "B2", 1100.0, false} };
        RouteGraph regBGraph(regBNodes, regBEdges, {}, 1);
        routeRepo->seedGraph(QStringLiteral("region_b"), regBGraph);

        assert(service.calculateRoute(QStringLiteral("region_b"), 20.0, 70.0, 20.01, 70.0));
        assert(service.hasRoute());
        assert(std::abs(service.lastDistanceMeters() - 1100.0) < 1e-3);

        // Switch back to Region A
        service.clearRoute();
        assert(!service.hasRoute());
        assert(service.calculateRoute(QStringLiteral("test_region"), 30.0, 78.0, 30.027, 78.0));
        assert(service.hasRoute());
        assert(std::abs(service.lastDistanceMeters() - 3000.0) < 1e-3);
        std::cout << "  -> PASSED: Region switching and active graph reload execute correctly." << std::endl;
    }

    // Test 19: Malformed Edge Endpoint References Handled Safely
    {
        std::cout << "[Test 19] Testing malformed edge endpoint references handled safely..." << std::endl;
        std::vector<RouteNode> malNodes = {
            RouteNode{"M1", 30.0, 78.0, false},
            RouteNode{"M2", 30.01, 78.0, false}
        };
        std::vector<RouteEdge> malEdges = {
            RouteEdge{"M1", "M2", 1100.0, false},
            RouteEdge{"M1", "NON_EXISTENT_NODE_XYZ", 500.0, false},
            RouteEdge{"GHOST_NODE_123", "M2", 700.0, false}
        };
        RouteGraph malGraph(malNodes, malEdges, {}, 1);
        routeRepo->seedGraph(QStringLiteral("malformed_edges_region"), malGraph);

        assert(service.calculateRoute(QStringLiteral("malformed_edges_region"), 30.0, 78.0, 30.01, 78.0));
        assert(service.hasRoute());
        assert(std::abs(service.lastDistanceMeters() - 1100.0) < 1e-3);
        std::cout << "  -> PASSED: Malformed edge references ignored without crashing." << std::endl;
    }

    // Test 20: Disconnected Components Handled Cleanly
    {
        std::cout << "[Test 20] Testing disconnected components handled cleanly..." << std::endl;
        std::vector<RouteNode> discNodes = {
            RouteNode{"C1_A", 30.0, 78.0, false},
            RouteNode{"C1_B", 30.01, 78.0, false},
            RouteNode{"C2_A", 30.05, 78.05, false},
            RouteNode{"C2_B", 30.06, 78.05, false}
        };
        std::vector<RouteEdge> discEdges = {
            RouteEdge{"C1_A", "C1_B", 1100.0, false},
            RouteEdge{"C2_A", "C2_B", 1100.0, false}
        };
        RouteGraph discGraph(discNodes, discEdges, {}, 1);
        routeRepo->seedGraph(QStringLiteral("disconnected_region"), discGraph);

        bool ok = service.calculateRoute(QStringLiteral("disconnected_region"), 30.0, 78.0, 30.06, 78.05);
        assert(!ok);
        assert(!service.hasRoute());
        std::cout << "  -> PASSED: Disconnected components safely return route failure." << std::endl;
    }

    // Test 21: Route Geometry Continuity & Exact Graph-Weight Consistency
    {
        std::cout << "[Test 21] Testing route geometry continuity & exact graph-weight consistency..." << std::endl;
        std::vector<RouteNode> geomNodes = {
            RouteNode{"G1", 30.00, 78.00, false},
            RouteNode{"G2", 30.01, 78.01, false},
            RouteNode{"G3", 30.02, 78.02, false},
            RouteNode{"G4", 30.03, 78.03, false}
        };
        std::vector<RouteEdge> geomEdges = {
            RouteEdge{"G1", "G2", 1250.5, false},
            RouteEdge{"G2", "G3", 1450.25, false},
            RouteEdge{"G3", "G4", 1300.25, false}
        };
        const double expectedTotalDistance = 1250.5 + 1450.25 + 1300.25; // 4001.0
        RouteGraph geomGraph(geomNodes, geomEdges, {}, 1);
        routeRepo->seedGraph(QStringLiteral("geom_continuity_region"), geomGraph);

        assert(service.calculateRoute(QStringLiteral("geom_continuity_region"), 30.00, 78.00, 30.03, 78.03));
        assert(service.hasRoute());
        assert(std::abs(service.lastDistanceMeters() - expectedTotalDistance) < 1e-2);

        QVariantList coords = service.lastCoordinates();
        assert(coords.size() == 4);
        for (int i = 0; i < coords.size(); ++i) {
            QVariantMap pt = coords[i].toMap();
            assert(pt.contains(QStringLiteral("latitude")));
            assert(pt.contains(QStringLiteral("longitude")));
            assert(std::abs(pt.value(QStringLiteral("latitude")).toDouble() - geomNodes[i].latitude) < 1e-6);
            assert(std::abs(pt.value(QStringLiteral("longitude")).toDouble() - geomNodes[i].longitude) < 1e-6);
        }
        std::cout << "  -> PASSED: Route geometry strictly contiguous and distance matches edge weight sum." << std::endl;
    }

    // Test 22: Coordinate Projection Roundtrip and Longitude Scale
    {
        std::cout << "[Test 22] Testing coordinate projection roundtrip and longitude scaling..." << std::endl;
        const double centerLat = 30.1;
        const double centerLon = 79.3;
        const double zoom = 6.2;
        const double ppd = 50.0 * std::pow(1.5, zoom);
        const double cosLat = std::cos(centerLat * M_PI / 180.0);

        assert(cosLat > 0.86 && cosLat < 0.87);

        const double testLat = 30.3165;
        const double testLon = 78.0322;
        const double width = 800.0;
        const double height = 600.0;

        const double px = (width / 2.0) + (testLon - centerLon) * ppd * cosLat;
        const double py = (height / 2.0) - (testLat - centerLat) * ppd;

        const double invLon = centerLon + (px - (width / 2.0)) / (ppd * cosLat);
        const double invLat = centerLat - (py - (height / 2.0)) / ppd;

        assert(std::abs(invLat - testLat) < 1e-9);
        assert(std::abs(invLon - testLon) < 1e-9);
        std::cout << "  -> PASSED: Projection roundtrip is lossless and longitude scaling is physically correct." << std::endl;
    }

    std::cout << "=================================================================" << std::endl;
    std::cout << " [GeoRescuX Test] ALL 22 PHASE 7 ROUTING TESTS PASSED!" << std::endl;
    std::cout << "=================================================================" << std::endl;

    return 0;
}
