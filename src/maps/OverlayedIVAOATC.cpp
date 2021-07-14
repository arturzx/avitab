/*
 *   AviTab - Aviator's Virtual Tablet
 *   Copyright (C) 2018 Folke Will <folko@solhost.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <memory>

#include "OverlayedIVAOATC.h"
#include "src/Logger.h"

namespace maps {

static std::map<std::string, std::shared_ptr<img::Image>> globalImageCache;

OverlayedIVAOATC::OverlayedIVAOATC(OverlayHelper helper, std::shared_ptr<ivao::IVAOATC> atc_): overlayHelper(helper), atc(atc_)
{
}

void OverlayedIVAOATC::drawGraphics() {
    auto mapImage = overlayHelper->getMapImage();
    auto nmPerPixel = mapImage->getWidth() / overlayHelper->getMapWidthNM();
    auto position = atc->atcSession->position;

    // Regular images positions
    // - DEL - delivery
    // - GND - ground
    // - TWR - tower
    // - DEP - departure
    if ((position == "DEL" || position == "GND" || position == "TWR" || position == "DEP") && atc->atcPosition && atc->atcPosition->airport) {
        double longitude = atc->atcPosition->airport->longitude;
        double latitude = atc->atcPosition->airport->latitude;
        if (overlayHelper->isLocVisibleWithMargin({latitude, longitude}, 300)) {
            int px, py;
            overlayHelper->positionToPixel(latitude, longitude, px, py);
            // Circles will be cached at img::Image
            if (position == "TWR") {
                int radius = nmPerPixel * 14;
                mapImage->drawCircle(px, py, radius, IVAO_TWR_BORDER_COLOR);
                mapImage->fillCircle(px, py, radius, IVAO_TWR_BACKGROUND_COLOR);
            } else if (position == "DEP") {
                int radius = nmPerPixel * 16;
                mapImage->drawCircle(px, py, radius, IVAO_DEP_BORDER_COLOR);
                mapImage->fillCircle(px, py, radius, IVAO_DEP_BACKGROUND_COLOR);
            } else {
                int radius = nmPerPixel * 14;
                std::shared_ptr<img::Image> img;
                std::string key = position + std::to_string(radius);
                auto it = globalImageCache.find(key);
                if (it != globalImageCache.end()) {
                    img = it->second;
                } else {
                    img = std::make_shared<img::Image>(radius * 2 + 1, radius * 2 + 1, 0);
                    if (position == "DEL") {
                        // Draw four-pointed star rotated at 45 degrees
                        int d1 = nmPerPixel * 3.75;
                        int d2 = nmPerPixel * 4.15;
                        int polyX[] = {d2, radius, radius * 2 - d2, radius + d1, radius * 2 - d2, radius, d2, radius - d1, d2};
                        int polyY[] = {d2, radius - d1, d2, radius, radius * 2 - d2, radius + d1, radius * 2 - d2, radius, d2};
                        img->fillPolygon(9, polyX, polyY, IVAO_DEL_BACKGROUND_COLOR);
                        img->drawPolygon(9, polyX, polyY, IVAO_DEL_BORDER_COLOR);
                    } else {
                        // Draw four-pointer star
                        int d1 = nmPerPixel * 3;
                        int polyX[] = {0, radius - d1, radius, radius + d1, radius * 2, radius + d1, radius, radius - d1, 0};
                        int polyY[] = {radius, radius - d1, 0, radius - d1, radius, radius + d1, radius * 2, radius + d1, radius};
                        img->fillPolygon(9, polyX, polyY, IVAO_GND_BACKGROUND_COLOR);
                        img->drawPolygon(9, polyX, polyY, IVAO_GND_BORDER_COLOR);
                    }
                    globalImageCache.insert(std::make_pair(key, img));
                }
                mapImage->blendImage0(*img, px - radius, py - radius);
            }
        }
    } else { // Irregular (polygons) images positions - APP - approach, CTR - control, FSS - flight service
        if ((position == "APP" && atc->atcPosition) || (position == "CTR" && atc->subcenter)) {
            std::vector<std::tuple<double, double>> polygon;
            uint32_t backgroundColor = 0, borderColor = 0;
            if (position == "APP") {
                polygon = atc->atcPosition->regionMapPolygon;
                backgroundColor = IVAO_APP_BACKGROUND_COLOR;
                borderColor = IVAO_APP_BORDER_COLOR;
            } else if (position == "CTR") {
                polygon = atc->subcenter->regionMapPolygon;
                backgroundColor = IVAO_CTR_BACKGROUND_COLOR;
                borderColor = IVAO_CTR_BORDER_COLOR;
            }
            int polyX[polygon.size()];
            int polyY[polygon.size()];
            int polygonBoundingBox[4] = {
                std::numeric_limits<int>::max(),
                std::numeric_limits<int>::max(),
                -std::numeric_limits<int>::max(),
                -std::numeric_limits<int>::max()
            };
            bool polygonCornerInView = false;
            for (std::size_t i = 0; i < polygon.size(); i++) {
                int px, py;
                overlayHelper->positionToPixel(std::get<1>(polygon[i]), std::get<0>(polygon[i]), px, py);
                polygonBoundingBox[0] = std::min(px, polygonBoundingBox[0]);
                polygonBoundingBox[1] = std::min(py, polygonBoundingBox[1]);
                polygonBoundingBox[2] = std::max(px, polygonBoundingBox[2]);
                polygonBoundingBox[3] = std::max(py, polygonBoundingBox[3]);
                polyX[i] = px;
                polyY[i] = py;
                if (px >= 0 && px <= mapImage->getWidth() && py >= 0 && py <= mapImage->getHeight()) {
                    polygonCornerInView = true;
                }
            }
            if ((polygonBoundingBox[0] <= 0 && polygonBoundingBox[2] >= mapImage->getWidth() - 1 &&
                polygonBoundingBox[1] <= 0 && polygonBoundingBox[3] >= mapImage->getHeight() - 1) || polygonCornerInView) {
                auto zoomLevel = overlayHelper->getZoomLevel();
                std::shared_ptr<img::Image> img;
                auto it = images.find(zoomLevel);
                if (it != images.end()) {
                    img = it->second;
                } else {
                    for (std::size_t i = 0; i < polygon.size(); i++) {
                        polyX[i] -= polygonBoundingBox[0];
                        polyY[i] -= polygonBoundingBox[1];
                    }
                    img = std::make_shared<img::Image>(
                        polygonBoundingBox[2] - polygonBoundingBox[0] + 1,
                        polygonBoundingBox[3] - polygonBoundingBox[1] + 1,
                        0
                    );
                    img->fillPolygon(polygon.size(), polyX, polyY, backgroundColor);
                    img->drawPolygon(polygon.size(), polyX, polyY, borderColor);
                    images.insert(std::make_pair(zoomLevel, img));
                }
                mapImage->blendImage0(*img, polygonBoundingBox[0], polygonBoundingBox[1]);
            }
        }
    }
}

void OverlayedIVAOATC::drawText(bool detailed) {
    auto zoomLevel = overlayHelper->getZoomLevel();
    auto mapImage = overlayHelper->getMapImage();
    auto position = atc->atcSession->position;
    int px, py;
    int size = 12;
    int lineOffset = 15;
    if (zoomLevel >= 8) {
        size += 2;
        lineOffset += 2;
    }
    uint32_t color = 0;
    if ((position == "DEL" || position == "GND" || position == "TWR" || position == "DEP" || position == "APP") && atc->atcPosition && atc->atcPosition->airport) {
        double longitude = atc->atcPosition->airport->longitude;
        double latitude = atc->atcPosition->airport->latitude;
        overlayHelper->positionToPixel(latitude, longitude, px, py);
        if (position == "DEL") {
            py += lineOffset * 2;
            color = IVAO_DEL_FONT_COLOR;
        } else if (position == "GND") {
            color = IVAO_GND_FONT_COLOR;
            py += lineOffset;
        } else if (position == "TWR") {
            color = IVAO_TWR_FONT_COLOR;
        } else if (position == "DEP") {
            color = IVAO_DEP_FONT_COLOR;
            py -= lineOffset;
        } else if (position == "APP") {
            color = IVAO_APP_FONT_COLOR;
            py -= lineOffset * 2;
        }
    } else if (atc->subcenter) {
        if (position == "CTR") {
            color = IVAO_CTR_FONT_COLOR;
            overlayHelper->positionToPixel(atc->subcenter->latitude, atc->subcenter->longitude, px, py);
        } else if (position == "FSS") {
            color = IVAO_FSS_FONT_COLOR;
            overlayHelper->positionToPixel(atc->subcenter->latitude, atc->subcenter->longitude, px, py);
        }
    }

    if (overlayHelper->isVisibleWithMargin(px, py, 150)) {
        std::string text = atc->callsign;
        if (detailed) {
            std::string frequencyStr = std::to_string(atc->atcSession->frequency);
            text += " " + frequencyStr.substr(0, frequencyStr.find('.') + 4);
        }
        mapImage->drawText(text, size, px, py - (size / 2), color, 0xB0FFFFFF, img::Align::CENTRE);
    }
}

} /* namespace maps */
