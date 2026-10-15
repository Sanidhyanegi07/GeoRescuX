#include <jni.h>
#include <string>
#include <sstream>
#include "../routing/RouteGraph.h"
#include "../routing/SafeHavenFallback.h"
#include "../routing/GraphJsonParser.h"
#include "../routing/BidirectionalAStar.h"
#include "../routing/DynamicGraphUpdater.h"
#include "../spatial/GeodesicMath.h"
#include "../spatial/KDTree.h"
#include "../spatial/ElevationCostModel.h"
#include "../mesh/MeshPacketCodec.h"
#include "../mesh/BloomFilterDeduplicator.h"
#include "../triage/TriageEngine.h"
#include "../crypto/PacketAuthenticator.h"

using namespace georescux::routing;
using namespace georescux::spatial;
using namespace georescux::mesh;
using namespace georescux::triage;
using namespace georescux::crypto;

namespace {
    static BloomFilterDeduplicator sGlobalDeduplicator(65536, 4, 300);
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeIsAvailable(
    JNIEnv* /*env*/,
    jclass /*clazz*/
) {
    return JNI_TRUE;
}

JNIEXPORT jstring JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeFindRoute(
    JNIEnv* env,
    jclass /*clazz*/,
    jstring jGraphJson,
    jstring jStartId,
    jstring jDestId
) {
    if (!jGraphJson || !jStartId || !jDestId) {
        return env->NewStringUTF("{\"found\":false,\"error\":\"null_arguments\"}");
    }

    try {
        const char* graphJsonChars = env->GetStringUTFChars(jGraphJson, nullptr);
        const char* startIdChars = env->GetStringUTFChars(jStartId, nullptr);
        const char* destIdChars = env->GetStringUTFChars(jDestId, nullptr);

        std::string graphJson = graphJsonChars ? graphJsonChars : "";
        std::string startId = startIdChars ? startIdChars : "";
        std::string destId = destIdChars ? destIdChars : "";

        if (graphJsonChars) env->ReleaseStringUTFChars(jGraphJson, graphJsonChars);
        if (startIdChars) env->ReleaseStringUTFChars(jStartId, startIdChars);
        if (destIdChars) env->ReleaseStringUTFChars(jDestId, destIdChars);

        auto graphOpt = GraphJsonParser::parse(graphJson);
        if (!graphOpt) {
            return env->NewStringUTF("{\"found\":false,\"error\":\"invalid_graph_json\"}");
        }

        auto routeOpt = SafeHavenFallback::findRoute(*graphOpt, startId, destId);
        std::string resultJson = GraphJsonParser::serializeRoute(routeOpt);

        return env->NewStringUTF(resultJson.c_str());
    } catch (const std::exception& e) {
        return env->NewStringUTF("{\"found\":false,\"error\":\"native_exception\"}");
    } catch (...) {
        return env->NewStringUTF("{\"found\":false,\"error\":\"unknown_native_exception\"}");
    }
}

JNIEXPORT jstring JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeFindBidirectionalRoute(
    JNIEnv* env,
    jclass /*clazz*/,
    jstring jGraphJson,
    jstring jStartId,
    jstring jDestId
) {
    if (!jGraphJson || !jStartId || !jDestId) {
        return env->NewStringUTF("{\"found\":false,\"error\":\"null_arguments\"}");
    }

    try {
        const char* graphJsonChars = env->GetStringUTFChars(jGraphJson, nullptr);
        const char* startIdChars = env->GetStringUTFChars(jStartId, nullptr);
        const char* destIdChars = env->GetStringUTFChars(jDestId, nullptr);

        std::string graphJson = graphJsonChars ? graphJsonChars : "";
        std::string startId = startIdChars ? startIdChars : "";
        std::string destId = destIdChars ? destIdChars : "";

        if (graphJsonChars) env->ReleaseStringUTFChars(jGraphJson, graphJsonChars);
        if (startIdChars) env->ReleaseStringUTFChars(jStartId, startIdChars);
        if (destIdChars) env->ReleaseStringUTFChars(jDestId, destIdChars);

        auto graphOpt = GraphJsonParser::parse(graphJson);
        if (!graphOpt) {
            return env->NewStringUTF("{\"found\":false,\"error\":\"invalid_graph_json\"}");
        }

        auto routeOpt = BidirectionalAStar::findRoute(*graphOpt, startId, destId);
        std::string resultJson = GraphJsonParser::serializeRoute(routeOpt);

        return env->NewStringUTF(resultJson.c_str());
    } catch (const std::exception& e) {
        return env->NewStringUTF("{\"found\":false,\"error\":\"native_exception\"}");
    } catch (...) {
        return env->NewStringUTF("{\"found\":false,\"error\":\"unknown_native_exception\"}");
    }
}

JNIEXPORT jdouble JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeHaversineDistance(
    JNIEnv* /*env*/,
    jclass /*clazz*/,
    jdouble lat1, jdouble lon1,
    jdouble lat2, jdouble lon2
) {
    try {
        return GeodesicMath::haversineDistanceMeters(lat1, lon1, lat2, lon2);
    } catch (...) {
        return 0.0;
    }
}

JNIEXPORT jdouble JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeVincentyDistance(
    JNIEnv* /*env*/,
    jclass /*clazz*/,
    jdouble lat1, jdouble lon1,
    jdouble lat2, jdouble lon2
) {
    try {
        return GeodesicMath::vincentyDistanceMeters(lat1, lon1, lat2, lon2);
    } catch (...) {
        return GeodesicMath::haversineDistanceMeters(lat1, lon1, lat2, lon2);
    }
}

JNIEXPORT jdouble JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeElevationImpedance(
    JNIEnv* /*env*/,
    jclass /*clazz*/,
    jdouble fromElev, jdouble toElev,
    jdouble distanceMeters, jboolean injured
) {
    try {
        return ElevationCostModel::calculateImpedanceMultiplier(fromElev, toElev, distanceMeters, injured);
    } catch (...) {
        return 1.0;
    }
}

JNIEXPORT jstring JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeEvaluateTriage(
    JNIEnv* env,
    jclass /*clazz*/,
    jboolean canWalk,
    jboolean breathingSpontaneously,
    jboolean breathingAfterAirway,
    jint respirationRate,
    jdouble capillaryRefill,
    jboolean radialPulse,
    jboolean followsCommands,
    jboolean hasHemorrhage,
    jboolean isTrapped
) {
    try {
        VictimAssessment a;
        a.canWalk = canWalk;
        a.breathingSpontaneously = breathingSpontaneously;
        a.breathingAfterAirwayPositioned = breathingAfterAirway;
        a.respirationRatePerMinute = static_cast<uint8_t>(respirationRate);
        a.capillaryRefillSeconds = capillaryRefill;
        a.radialPulsePresent = radialPulse;
        a.followsSimpleCommands = followsCommands;
        a.hasUncontrolledHemorrhage = hasHemorrhage;
        a.isTrapped = isTrapped;

        TriageResult res = TriageEngine::evaluateStart(a);

        std::ostringstream oss;
        oss << "{"
            << "\"category\":\"" << TriageEngine::categoryToString(res.category) << "\","
            << "\"priorityScore\":" << static_cast<int>(res.priorityScore) << ","
            << "\"primaryReason\":\"" << res.primaryReason << "\","
            << "\"recommendedIntervention\":\"" << res.recommendedIntervention << "\""
            << "}";

        return env->NewStringUTF(oss.str().c_str());
    } catch (...) {
        return env->NewStringUTF("{}");
    }
}

JNIEXPORT jboolean JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeDeduplicatePacket(
    JNIEnv* /*env*/,
    jclass /*clazz*/,
    jint senderHash,
    jint seqNumber
) {
    try {
        return sGlobalDeduplicator.checkAndMark(
            static_cast<uint32_t>(senderHash),
            static_cast<uint16_t>(seqNumber)
        );
    } catch (...) {
        return JNI_FALSE;
    }
}

JNIEXPORT jbyteArray JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeEncodeSosPacket(
    JNIEnv* env,
    jclass /*clazz*/,
    jint priority,
    jint ttl,
    jint seqNum,
    jstring jSenderId,
    jdouble lat, jdouble lon, jdouble alt,
    jint batteryPct, jint urgency, jint peopleCount,
    jlong timestampSec
) {
    if (!jSenderId) {
        return nullptr;
    }
    try {
        const char* senderIdChars = env->GetStringUTFChars(jSenderId, nullptr);
        std::string senderId = senderIdChars ? senderIdChars : "";
        if (senderIdChars) env->ReleaseStringUTFChars(jSenderId, senderIdChars);

        std::vector<uint8_t> packet = MeshPacketCodec::encodeSosPacket(
            static_cast<uint8_t>(priority),
            static_cast<uint8_t>(ttl),
            static_cast<uint16_t>(seqNum),
            senderId,
            lat, lon, alt,
            static_cast<uint8_t>(batteryPct),
            static_cast<uint8_t>(urgency),
            static_cast<uint16_t>(peopleCount),
            static_cast<uint32_t>(timestampSec)
        );

        jbyteArray result = env->NewByteArray(static_cast<jsize>(packet.size()));
        if (!result) return nullptr;
        env->SetByteArrayRegion(result, 0, static_cast<jsize>(packet.size()), reinterpret_cast<const jbyte*>(packet.data()));
        return result;
    } catch (...) {
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_com_example_georescux_data_routing_GeoRescueNativeRouting_nativeHmacSha256(
    JNIEnv* env,
    jclass /*clazz*/,
    jstring jKey,
    jstring jMessage
) {
    if (!jKey || !jMessage) {
        return env->NewStringUTF("");
    }
    try {
        const char* keyChars = env->GetStringUTFChars(jKey, nullptr);
        const char* msgChars = env->GetStringUTFChars(jMessage, nullptr);

        std::string key = keyChars ? keyChars : "";
        std::string msg = msgChars ? msgChars : "";

        if (keyChars) env->ReleaseStringUTFChars(jKey, keyChars);
        if (msgChars) env->ReleaseStringUTFChars(jMessage, msgChars);

        auto digest = PacketAuthenticator::hmacSha256(
            reinterpret_cast<const uint8_t*>(key.data()), key.size(),
            reinterpret_cast<const uint8_t*>(msg.data()), msg.size()
        );

        std::string hex = PacketAuthenticator::toHexString(digest);
        return env->NewStringUTF(hex.c_str());
    } catch (...) {
        return env->NewStringUTF("");
    }
}

} // extern "C"
