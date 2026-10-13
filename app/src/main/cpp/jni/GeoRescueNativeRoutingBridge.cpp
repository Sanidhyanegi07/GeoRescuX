#include <jni.h>
#include <string>
#include "../routing/RouteGraph.h"
#include "../routing/SafeHavenFallback.h"
#include "../routing/GraphJsonParser.h"

using namespace georescux::routing;

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
}

} // extern "C"
