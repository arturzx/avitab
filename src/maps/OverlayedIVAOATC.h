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
#ifndef SRC_MAPS_OVERLAYED_IVAOATC_H_
#define SRC_MAPS_OVERLAYED_IVAOATC_H_

#include "OverlayHelper.h"
#include "src/libivao/IVAO.h"

#define IVAO_DEL_BORDER_COLOR       0xFF000000
#define IVAO_DEL_BACKGROUND_COLOR   0x90FCCBA8
#define IVAO_DEL_FONT_COLOR         0xFFE08341
#define IVAO_GND_BORDER_COLOR       0xFF000000
#define IVAO_GND_BACKGROUND_COLOR   0x90FDFC86
#define IVAO_GND_FONT_COLOR         0xFFb3b100
#define IVAO_TWR_BORDER_COLOR       0xFFFF5C57
#define IVAO_TWR_BACKGROUND_COLOR   0x90FF5C57
#define IVAO_TWR_FONT_COLOR         0xFFE34242
#define IVAO_APP_BORDER_COLOR       0xFF78AAED
#define IVAO_APP_BACKGROUND_COLOR   0x9078AAED
#define IVAO_APP_FONT_COLOR         0xFF2587ED
#define IVAO_DEP_BORDER_COLOR       0xFFFF8CFF
#define IVAO_DEP_BACKGROUND_COLOR   0x90FF8CFF
#define IVAO_DEP_FONT_COLOR         0xFFFF67FF
#define IVAO_CTR_BORDER_COLOR       0xFF99B0C0
#define IVAO_CTR_BACKGROUND_COLOR   0x9099B0C0
#define IVAO_CTR_FONT_COLOR         0xFF677D8C
#define IVAO_FSS_BORDER_COLOR       0xFF00FFFF
#define IVAO_FSS_BACKGROUND_COLOR   0xFF00FFFF
#define IVAO_FSS_FONT_COLOR         0xFF00FFFF

namespace maps {

using OverlayHelper = std::shared_ptr<IOverlayHelper>;

class OverlayedIVAOATC {

public:
    OverlayedIVAOATC(OverlayHelper helper, std::shared_ptr<ivao::IVAOATC> atc);

    void drawGraphics();
    void drawText(bool detailed);

private:
    OverlayHelper overlayHelper;
    std::shared_ptr<ivao::IVAOATC> atc;
    std::map<int, std::shared_ptr<img::Image>> images;
};

} /* namespace maps */

#endif /* SRC_MAPS_OVERLAYED_IVAOATC_H_ */
