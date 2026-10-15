#include "OsmPbfReader.h"
#include "GraphExtractor.h"
#include <iostream>
#include <chrono>

using namespace georescux::tools;

int main(int argc, char* argv[]) {
    std::cout << "=========================================================" << std::endl;
    std::cout << "  GeoRescuX C++ Native Regional Map & Graph Builder     " << std::endl;
    std::cout << "=========================================================" << std::endl;

    std::string inputFile = (argc > 1) ? argv[1] : "source/uttarakhand-latest.osm.pbf";
    std::string outputFile = (argc > 2) ? argv[2] : "output/uttarakhand/graph_cpp.json";

    std::cout << "Input OSM Extract: " << inputFile << std::endl;
    std::cout << "Target Graph Output: " << outputFile << std::endl;

    // Uttarakhand Region Bounds (bounding box)
    RegionBounds uttarakhandBounds{
        .minLat = 28.7,
        .maxLat = 31.5,
        .minLon = 77.5,
        .maxLon = 81.1
    };

    GraphExtractor extractor(uttarakhandBounds);

    auto startTime = std::chrono::high_resolution_clock::now();

    OsmPbfReader reader(inputFile);
    bool success = reader.parse(
        [&](const OsmNode& node) { extractor.addNode(node); },
        [&](const OsmWay& way) { extractor.addWay(way); }
    );

    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - startTime
    ).count();

    std::cout << "Parsing completed in " << elapsedMs << " ms." << std::endl;
    std::cout << "Extracted Nodes: " << extractor.getNodeCount() << std::endl;
    std::cout << "Extracted Edges: " << extractor.getEdgeCount() << std::endl;
    std::cout << "Safe Havens Identified: " << extractor.getSafeHavenCount() << std::endl;

    if (extractor.exportGraphJson(outputFile)) {
        std::cout << "[SUCCESS] Regional graph serialized to " << outputFile << std::endl;
    } else {
        std::cerr << "[WARNING] Could not write output to " << outputFile << std::endl;
    }

    return 0;
}
