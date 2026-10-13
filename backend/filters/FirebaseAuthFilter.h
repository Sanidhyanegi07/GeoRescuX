#pragma once

#include <drogon/HttpFilter.h>
#include <string>
#include <optional>
#include <json/json.h>

namespace georescux::backend::filters {

struct AuthContext {
    std::string uid;
    std::string email;
    bool isAdmin{false};
};

/**
 * Validates Firebase ID tokens cryptographically and verifies:
 * - Bearer authorization header format
 * - JWT header (alg == RS256)
 * - Expiration (exp > now)
 * - Issuer (iss == https://securetoken.google.com/<FIREBASE_PROJECT_ID>)
 * - Audience (aud == <FIREBASE_PROJECT_ID>)
 * - Non-empty Subject (sub == Firebase UID)
 * - Custom Claims: checks "admin" claim for admin routes.
 */
class FirebaseAuthFilter : public drogon::HttpFilter<FirebaseAuthFilter> {
public:
    FirebaseAuthFilter();
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;

    static std::optional<AuthContext> verifyFirebaseToken(
        const std::string& token,
        const std::string& expectedProjectId,
        std::string& errorReason
    );
};

/**
 * Filter requiring verified Firebase Admin custom claim (admin == true).
 */
class FirebaseAdminFilter : public drogon::HttpFilter<FirebaseAdminFilter> {
public:
    FirebaseAdminFilter();
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

} // namespace georescux::backend::filters
