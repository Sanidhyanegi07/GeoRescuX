#include "VectorTileRasterizer.h"
#include <algorithm>
#include <cmath>

namespace georescux::spatial {

VectorTileRasterizer::VectorTileRasterizer(uint32_t width, uint32_t height)
    : width_(width), height_(height) {
    pixelBuffer_.assign(width_ * height_ * 4, 255);
}

void VectorTileRasterizer::clearCanvas(PixelColor background) {
    for (size_t i = 0; i < width_ * height_; ++i) {
        pixelBuffer_[i * 4 + 0] = background.r;
        pixelBuffer_[i * 4 + 1] = background.g;
        pixelBuffer_[i * 4 + 2] = background.b;
        pixelBuffer_[i * 4 + 3] = background.a;
    }
    placedLabels_.clear();
}

void VectorTileRasterizer::setPixel(int x, int y, PixelColor color, float alphaWeight) {
    if (x < 0 || x >= static_cast<int>(width_) || y < 0 || y >= static_cast<int>(height_)) {
        return;
    }

    size_t idx = (static_cast<size_t>(y) * width_ + static_cast<size_t>(x)) * 4;
    float effectiveAlpha = (color.a / 255.0f) * std::clamp(alphaWeight, 0.0f, 1.0f);
    float invAlpha = 1.0f - effectiveAlpha;

    pixelBuffer_[idx + 0] = static_cast<uint8_t>(color.r * effectiveAlpha + pixelBuffer_[idx + 0] * invAlpha);
    pixelBuffer_[idx + 1] = static_cast<uint8_t>(color.g * effectiveAlpha + pixelBuffer_[idx + 1] * invAlpha);
    pixelBuffer_[idx + 2] = static_cast<uint8_t>(color.b * effectiveAlpha + pixelBuffer_[idx + 2] * invAlpha);
    pixelBuffer_[idx + 3] = static_cast<uint8_t>(std::min(255.0f, pixelBuffer_[idx + 3] + effectiveAlpha * 255.0f));
}

void VectorTileRasterizer::drawWuLine(float x0, float y0, float x1, float y1, PixelColor color, float width) {
    bool steep = std::abs(y1 - y0) > std::abs(x0 - x1);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = (dx == 0.0f) ? 1.0f : (dy / dx);

    // Handle first endpoint
    float xEnd = std::round(x0);
    float yEnd = y0 + gradient * (xEnd - x0);
    float xGap = 1.0f - (x0 + 0.5f - std::floor(x0 + 0.5f));
    int xPx1 = static_cast<int>(xEnd);
    int yPx1 = static_cast<int>(std::floor(yEnd));

    if (steep) {
        setPixel(yPx1, xPx1, color, (1.0f - (yEnd - yPx1)) * xGap);
        setPixel(yPx1 + 1, xPx1, color, (yEnd - yPx1) * xGap);
    } else {
        setPixel(xPx1, yPx1, color, (1.0f - (yEnd - yPx1)) * xGap);
        setPixel(xPx1, yPx1 + 1, color, (yEnd - yPx1) * xGap);
    }
    float interY = yEnd + gradient;

    // Handle second endpoint
    xEnd = std::round(x1);
    yEnd = y1 + gradient * (xEnd - x1);
    xGap = x1 + 0.5f - std::floor(x1 + 0.5f);
    int xPx2 = static_cast<int>(xEnd);
    int yPx2 = static_cast<int>(std::floor(yEnd));

    if (steep) {
        setPixel(yPx2, xPx2, color, (1.0f - (yEnd - yPx2)) * xGap);
        setPixel(yPx2 + 1, xPx2, color, (yEnd - yPx2) * xGap);
    } else {
        setPixel(xPx2, yPx2, color, (1.0f - (yEnd - yPx2)) * xGap);
        setPixel(xPx2, yPx2 + 1, color, (yEnd - yPx2) * xGap);
    }

    // Main line loop
    int startX = xPx1 + 1;
    int endX = xPx2 - 1;
    for (int x = startX; x <= endX; ++x) {
        int yFloor = static_cast<int>(std::floor(interY));
        float frac = interY - yFloor;
        if (steep) {
            setPixel(yFloor, x, color, 1.0f - frac);
            setPixel(yFloor + 1, x, color, frac);
        } else {
            setPixel(x, yFloor, color, 1.0f - frac);
            setPixel(x, yFloor + 1, color, frac);
        }
        interY += gradient;
    }
}

void VectorTileRasterizer::drawPolyline(const std::vector<CanvasPoint>& points, const MapFeatureStyle& style) {
    if (points.size() < 2) return;

    for (size_t i = 0; i < points.size() - 1; ++i) {
        drawWuLine(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, style.strokeColor, style.strokeWidthPixels);
    }
}

void VectorTileRasterizer::drawPolygon(const std::vector<CanvasPoint>& ring, const MapFeatureStyle& style) {
    if (ring.size() < 3) return;

    if (style.hasFill) {
        // Find vertical bounds
        float minY = ring[0].y;
        float maxY = ring[0].y;
        for (const auto& pt : ring) {
            if (pt.y < minY) minY = pt.y;
            if (pt.y > maxY) maxY = pt.y;
        }

        int scanMinY = std::max(0, static_cast<int>(std::floor(minY)));
        int scanMaxY = std::min(static_cast<int>(height_ - 1), static_cast<int>(std::ceil(maxY)));

        // Scanline rasterization
        size_t n = ring.size();
        for (int y = scanMinY; y <= scanMaxY; ++y) {
            std::vector<float> nodeX;
            float scanY = static_cast<float>(y) + 0.5f;

            for (size_t i = 0; i < n; ++i) {
                size_t j = (i + 1) % n;
                const auto& p1 = ring[i];
                const auto& p2 = ring[j];

                if ((p1.y < scanY && p2.y >= scanY) || (p2.y < scanY && p1.y >= scanY)) {
                    float x = p1.x + (scanY - p1.y) / (p2.y - p1.y) * (p2.x - p1.x);
                    nodeX.push_back(x);
                }
            }

            std::sort(nodeX.begin(), nodeX.end());

            // Fill spans between pairs of intersections
            for (size_t i = 0; i + 1 < nodeX.size(); i += 2) {
                int startX = std::max(0, static_cast<int>(std::ceil(nodeX[i])));
                int endX = std::min(static_cast<int>(width_ - 1), static_cast<int>(std::floor(nodeX[i + 1])));
                for (int x = startX; x <= endX; ++x) {
                    setPixel(x, y, style.fillColor, 1.0f);
                }
            }
        }
    }

    // Draw boundary stroke
    size_t n = ring.size();
    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        drawWuLine(ring[i].x, ring[i].y, ring[j].x, ring[j].y, style.strokeColor, style.strokeWidthPixels);
    }
}

void VectorTileRasterizer::drawCircle(CanvasPoint center, float radiusPixels, const MapFeatureStyle& style) {
    int rInt = static_cast<int>(std::ceil(radiusPixels));
    int cx = static_cast<int>(std::round(center.x));
    int cy = static_cast<int>(std::round(center.y));

    for (int y = -rInt; y <= rInt; ++y) {
        for (int x = -rInt; x <= rInt; ++x) {
            float dist = std::sqrt(static_cast<float>(x * x + y * y));
            if (dist <= radiusPixels) {
                if (style.hasFill) {
                    setPixel(cx + x, cy + y, style.fillColor, 1.0f);
                }
            } else if (dist <= radiusPixels + 1.0f) {
                // Anti-aliased boundary
                float alpha = 1.0f - (dist - radiusPixels);
                setPixel(cx + x, cy + y, style.strokeColor, alpha);
            }
        }
    }
}

bool VectorTileRasterizer::placeLabel(const std::string& text, float x, float y, float charWidth, float charHeight) {
    float textWidth = text.length() * charWidth;
    BoundingBox2D bbox{x - 2.0f, y - 2.0f, x + textWidth + 2.0f, y + charHeight + 2.0f};

    // Check collision against placed labels
    for (const auto& label : placedLabels_) {
        bool noOverlap = (bbox.maxX < label.bbox.minX) ||
                         (bbox.minX > label.bbox.maxX) ||
                         (bbox.maxY < label.bbox.minY) ||
                         (bbox.minY > label.bbox.maxY);
        if (!noOverlap) {
            return false; // collision detected
        }
    }

    placedLabels_.push_back({text, x, y, bbox});
    return true;
}

} // namespace georescux::spatial
