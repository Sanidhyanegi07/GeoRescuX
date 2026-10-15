#ifndef GEORESCUX_VECTOR_TILE_RASTERIZER_H
#define GEORESCUX_VECTOR_TILE_RASTERIZER_H

#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include "HazardPolygon.h"

namespace georescux::spatial {

struct PixelColor {
    uint8_t r, g, b, a;
};

struct CanvasPoint {
    float x;
    float y;
};

struct MapFeatureStyle {
    PixelColor strokeColor;
    float strokeWidthPixels;
    PixelColor fillColor;
    bool hasFill;
    bool isDashed;
};

struct BoundingBox2D {
    float minX, minY, maxX, maxY;
};

struct PlacedLabel {
    std::string text;
    float x, y;
    BoundingBox2D bbox;
};

/**
 * Offline Vector Tile and Geographic Feature Rasterizer.
 * Renders roads, evacuation corridors, hazard polygons, and relief zones
 * directly to an RGBA pixel buffer using anti-aliased scanline rasterization
 * and Xiaolin Wu line drawing algorithms with zero GPU/network dependencies.
 */
class VectorTileRasterizer {
public:
    VectorTileRasterizer(uint32_t width, uint32_t height);

    void clearCanvas(PixelColor background = {245, 245, 242, 255});

    /**
     * Draws an anti-aliased polyline on the canvas using Xiaolin Wu's algorithm.
     */
    void drawPolyline(const std::vector<CanvasPoint>& points, const MapFeatureStyle& style);

    /**
     * Draws a filled and stroked polygon using scanline edge-table rasterization.
     */
    void drawPolygon(const std::vector<CanvasPoint>& ring, const MapFeatureStyle& style);

    /**
     * Draws a circular beacon / POI icon.
     */
    void drawCircle(CanvasPoint center, float radiusPixels, const MapFeatureStyle& style);

    /**
     * Attempts to place a text label with greedy bounding-box collision avoidance.
     */
    bool placeLabel(const std::string& text, float x, float y, float charWidth = 8.0f, float charHeight = 12.0f);

    [[nodiscard]] const std::vector<uint8_t>& getRgbaBuffer() const noexcept { return pixelBuffer_; }
    [[nodiscard]] uint32_t getWidth() const noexcept { return width_; }
    [[nodiscard]] uint32_t getHeight() const noexcept { return height_; }
    [[nodiscard]] const std::vector<PlacedLabel>& getPlacedLabels() const noexcept { return placedLabels_; }

private:
    uint32_t width_;
    uint32_t height_;
    std::vector<uint8_t> pixelBuffer_; // RGBA8888, size = width * height * 4
    std::vector<PlacedLabel> placedLabels_;

    void setPixel(int x, int y, PixelColor color, float alphaWeight = 1.0f);
    void drawWuLine(float x0, float y0, float x1, float y1, PixelColor color, float width);
};

} // namespace georescux::spatial

#endif // GEORESCUX_VECTOR_TILE_RASTERIZER_H
