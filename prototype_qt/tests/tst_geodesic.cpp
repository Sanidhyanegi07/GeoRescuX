#include <iostream>
#include <cmath>
#include <cassert>
#include "GeodesicMath.h"

int main() {
    std::cout << "[GeoRescuX Test] Running GeodesicMath C++20 unit verification..." << std::endl;

    // Test corridor: Dehradun to Rishikesh (Uttarakhand evacuation route)
    constexpr double lat1 = 30.3165;
    constexpr double lon1 = 78.0322;
    constexpr double lat2 = 30.0869;
    constexpr double lon2 = 78.2676;

    // 1. Identity test: distance to self must be 0
    const double selfDist = georescux::spatial::GeodesicMath::haversineDistanceMeters(lat1, lon1, lat1, lon1);
    if (std::abs(selfDist) > 1e-6) {
        std::cerr << "FAIL: Self-distance is not zero: " << selfDist << std::endl;
        return 1;
    }

    // 2. Haversine calculation
    const double haversine = georescux::spatial::GeodesicMath::haversineDistanceMeters(lat1, lon1, lat2, lon2);
    std::cout << "[GeoRescuX Test] Haversine Distance: " << haversine << " m" << std::endl;
    if (haversine < 34000.0 || haversine > 34200.0) {
        std::cerr << "FAIL: Haversine distance out of expected range: " << haversine << std::endl;
        return 2;
    }

    // 3. Vincenty high-precision calculation
    const double vincenty = georescux::spatial::GeodesicMath::vincentyDistanceMeters(lat1, lon1, lat2, lon2);
    std::cout << "[GeoRescuX Test] Vincenty Distance: " << vincenty << " m" << std::endl;
    if (vincenty < 34000.0 || vincenty > 34200.0) {
        std::cerr << "FAIL: Vincenty distance out of expected range: " << vincenty << std::endl;
        return 3;
    }

    // 4. Initial bearing calculation
    const double bearing = georescux::spatial::GeodesicMath::initialBearingDegrees(lat1, lon1, lat2, lon2);
    std::cout << "[GeoRescuX Test] Initial Bearing: " << bearing << " deg" << std::endl;
    if (bearing < 130.0 || bearing > 145.0) {
        std::cerr << "FAIL: Bearing out of expected range: " << bearing << std::endl;
        return 4;
    }

    // 5. Symmetry / Commutativity test
    const double revVincenty = georescux::spatial::GeodesicMath::vincentyDistanceMeters(lat2, lon2, lat1, lon1);
    if (std::abs(vincenty - revVincenty) > 0.01) {
        std::cerr << "FAIL: Geodesic calculation is not symmetric! Delta: " << std::abs(vincenty - revVincenty) << std::endl;
        return 5;
    }

    std::cout << "[GeoRescuX Test] ALL GEODESIC MATH CHECKS PASSED SUCCESSFULLY." << std::endl;
    return 0;
}
