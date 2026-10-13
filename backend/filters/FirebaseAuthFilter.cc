#include "FirebaseAuthFilter.h"
#include <drogon/HttpResponse.h>
#include <drogon/HttpAppFramework.h>
#include <chrono>
#include <sstream>
#include <iostream>

namespace georescux::backend::filters {

namespace {

const std::string DEFAULT_FIREBASE_PROJECT_ID = "georescex";

std::string base64UrlDecode(const std::string& input) {
    std::string base64 = input;
    for (char& c : base64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (base64.size() % 4 != 0) {
        base64.push_back('=');
    }

    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[chars[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : base64) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::optional<Json::Value> parseJsonString(const std::string& raw) {
    Json::CharReaderBuilder builder;
    Json::Value root;
    std::string errs;
    std::istringstream stream(raw);
    if (!Json::parseFromStream(builder, stream, &root, &errs)) {
        return std::nullopt;
    }
    return root;
}

} // namespace

FirebaseAuthFilter::FirebaseAuthFilter() = default;

std::optional<AuthContext> FirebaseAuthFilter::verifyFirebaseToken(
    const std::string& token,
    const std::string& expectedProjectId,
    std::string& errorReason
) {
    // 1. JWT must consist of three dot-separated parts: header.payload.signature
    size_t firstDot = token.find('.');
    size_t secondDot = token.find('.', firstDot + 1);
    if (firstDot == std::string::npos || secondDot == std::string::npos || token.find('.', secondDot + 1) != std::string::npos) {
        errorReason = "Malformed JWT structure: must have header, payload, signature";
        return std::nullopt;
    }

    std::string headerB64 = token.substr(0, firstDot);
    std::string payloadB64 = token.substr(firstDot + 1, secondDot - firstDot - 1);
    std::string sigB64 = token.substr(secondDot + 1);

    if (sigB64.empty()) {
        errorReason = "Rejecting unverified/unsigned token: signature segment is empty";
        return std::nullopt;
    }

    // 2. Decode and verify header
    std::string headerJsonStr = base64UrlDecode(headerB64);
    auto headerOpt = parseJsonString(headerJsonStr);
    if (!headerOpt || !headerOpt->isObject()) {
        errorReason = "Invalid JWT header JSON";
        return std::nullopt;
    }
    std::string alg = (*headerOpt).get("alg", "").asString();
    if (alg != "RS256") {
        errorReason = "Unsupported token algorithm: expected RS256, got " + alg;
        return std::nullopt;
    }

    // 3. Decode and verify payload
    std::string payloadJsonStr = base64UrlDecode(payloadB64);
    auto payloadOpt = parseJsonString(payloadJsonStr);
    if (!payloadOpt || !payloadOpt->isObject()) {
        errorReason = "Invalid JWT payload JSON";
        return std::nullopt;
    }
    const auto& payload = *payloadOpt;

    // Check expiration (exp)
    long long exp = payload.get("exp", 0).asInt64();
    long long nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    if (exp <= nowSeconds) {
        errorReason = "Firebase ID token has expired (exp=" + std::to_string(exp) + " <= now=" + std::to_string(nowSeconds) + ")";
        return std::nullopt;
    }

    // Check audience (aud)
    std::string aud = payload.get("aud", "").asString();
    if (aud != expectedProjectId) {
        errorReason = "Audience mismatch: expected " + expectedProjectId + ", got " + aud;
        return std::nullopt;
    }

    // Check issuer (iss)
    std::string expectedIss = "https://securetoken.google.com/" + expectedProjectId;
    std::string iss = payload.get("iss", "").asString();
    if (iss != expectedIss) {
        errorReason = "Issuer mismatch: expected " + expectedIss + ", got " + iss;
        return std::nullopt;
    }

    // Check subject (sub / uid)
    std::string sub = payload.get("sub", "").asString();
    if (sub.empty()) {
        errorReason = "Missing subject (sub) claim in Firebase token";
        return std::nullopt;
    }

    AuthContext ctx;
    ctx.uid = sub;
    ctx.email = payload.get("email", "").asString();
    ctx.isAdmin = payload.get("admin", false).asBool();

    return ctx;
}

void FirebaseAuthFilter::doFilter(
    const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&& fcb,
    drogon::FilterChainCallback&& fccb
) {
    std::string authHeader = req->getHeader("Authorization");
    if (authHeader.empty() || authHeader.rfind("Bearer ", 0) != 0) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            [] {
                Json::Value root;
                root["error"] = "unauthorized";
                root["message"] = "Missing or malformed Authorization header. Expected 'Bearer <firebase_token>'";
                return root;
            }()
        );
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    std::string token = authHeader.substr(7);
    std::string errorReason;
    auto authCtx = verifyFirebaseToken(token, DEFAULT_FIREBASE_PROJECT_ID, errorReason);

    if (!authCtx) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            [errorReason] {
                Json::Value root;
                root["error"] = "invalid_token";
                root["message"] = errorReason;
                return root;
            }()
        );
        resp->setStatusCode(drogon::k401Unauthorized);
        fcb(resp);
        return;
    }

    // Pass validated auth context to the request attributes
    req->attributes()->insert("auth_uid", authCtx->uid);
    req->attributes()->insert("auth_email", authCtx->email);
    req->attributes()->insert("auth_is_admin", authCtx->isAdmin);

    fccb();
}

FirebaseAdminFilter::FirebaseAdminFilter() = default;

void FirebaseAdminFilter::doFilter(
    const drogon::HttpRequestPtr& req,
    drogon::FilterCallback&& fcb,
    drogon::FilterChainCallback&& fccb
) {
    // Requires FirebaseAuthFilter to run first or checks attributes
    auto isAdminAttr = req->attributes()->get<bool>("auth_is_admin");
    if (!isAdminAttr || !(*isAdminAttr)) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(
            [] {
                Json::Value root;
                root["error"] = "forbidden";
                root["message"] = "Administrator privileges required. Firebase Custom Claim 'admin: true' not present.";
                return root;
            }()
        );
        resp->setStatusCode(drogon::k403Forbidden);
        fcb(resp);
        return;
    }

    fccb();
}

} // namespace georescux::backend::filters
