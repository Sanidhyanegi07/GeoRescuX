#include "GraphJsonParser.h"
#include <sstream>
#include <cctype>
#include <iomanip>

namespace georescux::routing {

namespace {

class SimpleJsonScanner {
public:
    explicit SimpleJsonScanner(const std::string& src) : src_(src), pos_(0) {}

    void skipWhitespace() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            pos_++;
        }
    }

    bool hasNext() const {
        return pos_ < src_.size();
    }

    char peek() {
        skipWhitespace();
        return pos_ < src_.size() ? src_[pos_] : '\0';
    }

    char next() {
        skipWhitespace();
        return pos_ < src_.size() ? src_[pos_++] : '\0';
    }

    bool expect(char c) {
        skipWhitespace();
        if (pos_ < src_.size() && src_[pos_] == c) {
            pos_++;
            return true;
        }
        return false;
    }

    std::string parseString() {
        skipWhitespace();
        if (pos_ >= src_.size() || src_[pos_] != '"') return "";
        pos_++; // skip opening quote
        std::string result;
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') {
                return result;
            }
            if (c == '\\' && pos_ < src_.size()) {
                char esc = src_[pos_++];
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += esc; break;
                }
            } else {
                result += c;
            }
        }
        return result;
    }

    double parseNumber() {
        skipWhitespace();
        size_t start = pos_;
        if (pos_ < src_.size() && (src_[pos_] == '-' || src_[pos_] == '+')) pos_++;
        while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) ||
               src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' ||
               src_[pos_] == '-' || src_[pos_] == '+')) {
            pos_++;
        }
        std::string numStr = src_.substr(start, pos_ - start);
        try {
            return std::stod(numStr);
        } catch (...) {
            return 0.0;
        }
    }

    bool parseBool() {
        skipWhitespace();
        if (src_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            return true;
        }
        if (src_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            return false;
        }
        return false;
    }

    void skipValue() {
        skipWhitespace();
        if (pos_ >= src_.size()) return;
        char c = src_[pos_];
        if (c == '"') {
            parseString();
        } else if (c == '{') {
            pos_++;
            int depth = 1;
            while (pos_ < src_.size() && depth > 0) {
                if (src_[pos_] == '{') depth++;
                else if (src_[pos_] == '}') depth--;
                else if (src_[pos_] == '"') parseString();
                else pos_++;
            }
        } else if (c == '[') {
            pos_++;
            int depth = 1;
            while (pos_ < src_.size() && depth > 0) {
                if (src_[pos_] == '[') depth++;
                else if (src_[pos_] == ']') depth--;
                else if (src_[pos_] == '"') parseString();
                else pos_++;
            }
        } else {
            while (pos_ < src_.size() && src_[pos_] != ',' && src_[pos_] != '}' && src_[pos_] != ']') {
                pos_++;
            }
        }
    }

private:
    const std::string& src_;
    size_t pos_;
};

} // namespace

std::optional<RouteGraph> GraphJsonParser::parse(const std::string& json) {
    if (json.empty()) return std::nullopt;

    SimpleJsonScanner scanner(json);
    if (!scanner.expect('{')) return std::nullopt;

    std::vector<RouteNode> nodes;
    std::vector<RouteEdge> edges;
    std::vector<RoadHazard> hazards;
    int version = 0;

    while (scanner.peek() != '}' && scanner.hasNext()) {
        std::string key = scanner.parseString();
        if (!scanner.expect(':')) break;

        if (key == "region") {
            if (scanner.expect('{')) {
                while (scanner.peek() != '}' && scanner.hasNext()) {
                    std::string rKey = scanner.parseString();
                    scanner.expect(':');
                    if (rKey == "version") {
                        version = static_cast<int>(scanner.parseNumber());
                    } else {
                        scanner.skipValue();
                    }
                    if (scanner.peek() == ',') scanner.next();
                }
                scanner.expect('}');
            } else {
                scanner.skipValue();
            }
        } else if (key == "nodes") {
            if (scanner.expect('[')) {
                while (scanner.peek() != ']' && scanner.hasNext()) {
                    if (scanner.expect('{')) {
                        RouteNode nd;
                        while (scanner.peek() != '}' && scanner.hasNext()) {
                            std::string nKey = scanner.parseString();
                            scanner.expect(':');
                            if (nKey == "id") nd.id = scanner.parseString();
                            else if (nKey == "latitude") nd.latitude = scanner.parseNumber();
                            else if (nKey == "longitude") nd.longitude = scanner.parseNumber();
                            else if (nKey == "isSafeHaven") nd.isSafeHaven = scanner.parseBool();
                            else scanner.skipValue();

                            if (scanner.peek() == ',') scanner.next();
                        }
                        scanner.expect('}');
                        if (!nd.id.empty()) {
                            nodes.push_back(std::move(nd));
                        }
                    } else {
                        scanner.skipValue();
                    }
                    if (scanner.peek() == ',') scanner.next();
                }
                scanner.expect(']');
            } else {
                scanner.skipValue();
            }
        } else if (key == "edges") {
            if (scanner.expect('[')) {
                while (scanner.peek() != ']' && scanner.hasNext()) {
                    if (scanner.expect('{')) {
                        RouteEdge ed;
                        while (scanner.peek() != '}' && scanner.hasNext()) {
                            std::string eKey = scanner.parseString();
                            scanner.expect(':');
                            if (eKey == "fromNodeId") ed.fromNodeId = scanner.parseString();
                            else if (eKey == "toNodeId") ed.toNodeId = scanner.parseString();
                            else if (eKey == "distanceMeters") ed.distanceMeters = scanner.parseNumber();
                            else if (eKey == "blocked") ed.blocked = scanner.parseBool();
                            else scanner.skipValue();

                            if (scanner.peek() == ',') scanner.next();
                        }
                        scanner.expect('}');
                        if (!ed.fromNodeId.empty() && !ed.toNodeId.empty()) {
                            edges.push_back(std::move(ed));
                        }
                    } else {
                        scanner.skipValue();
                    }
                    if (scanner.peek() == ',') scanner.next();
                }
                scanner.expect(']');
            } else {
                scanner.skipValue();
            }
        } else if (key == "hazards") {
            if (scanner.expect('[')) {
                while (scanner.peek() != ']' && scanner.hasNext()) {
                    if (scanner.expect('{')) {
                        RoadHazard hz;
                        while (scanner.peek() != '}' && scanner.hasNext()) {
                            std::string hKey = scanner.parseString();
                            scanner.expect(':');
                            if (hKey == "id") hz.id = scanner.parseString();
                            else if (hKey == "fromNodeId") hz.fromNodeId = scanner.parseString();
                            else if (hKey == "toNodeId") hz.toNodeId = scanner.parseString();
                            else if (hKey == "penaltyMeters") hz.penaltyMeters = scanner.parseNumber();
                            else scanner.skipValue();

                            if (scanner.peek() == ',') scanner.next();
                        }
                        scanner.expect('}');
                        if (!hz.id.empty()) {
                            hazards.push_back(std::move(hz));
                        }
                    } else {
                        scanner.skipValue();
                    }
                    if (scanner.peek() == ',') scanner.next();
                }
                scanner.expect(']');
            } else {
                scanner.skipValue();
            }
        } else {
            scanner.skipValue();
        }

        if (scanner.peek() == ',') scanner.next();
    }
    scanner.expect('}');

    if (nodes.empty()) return std::nullopt;

    return RouteGraph(std::move(nodes), std::move(edges), std::move(hazards), version);
}

std::string GraphJsonParser::serializeRoute(const std::optional<EvacuationRoute>& route) {
    if (!route || route->empty()) {
        return "{\"found\":false}";
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\"found\":true,";
    ss << "\"totalDistanceMeters\":" << route->totalDistanceMeters << ",";
    ss << "\"totalCostMeters\":" << route->totalCostMeters << ",";
    ss << "\"estimatedDurationSeconds\":" << route->estimatedDurationSeconds << ",";
    ss << "\"nodeIds\":[";
    for (size_t i = 0; i < route->nodeIds.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "\"" << route->nodeIds[i] << "\"";
    }
    ss << "],";
    ss << "\"hazardWarnings\":[";
    for (size_t i = 0; i < route->hazardWarnings.size(); ++i) {
        if (i > 0) ss << ",";
        const auto& hz = route->hazardWarnings[i];
        ss << "{\"id\":\"" << hz.id << "\",\"penaltyMeters\":" << hz.penaltyMeters << "}";
    }
    ss << "]}";
    return ss.str();
}

} // namespace georescux::routing
