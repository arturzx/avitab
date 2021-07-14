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
#ifndef SRC_LIBIVAO_IVAO_H_
#define SRC_LIBIVAO_IVAO_H_

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <curl/curl.h>

namespace ivao {

struct IVAOATCSession {
    double frequency;
    std::string position;
};


struct IVAOATCBase {
    std::string atcCallsign;
    std::string position;
    std::string middleIdentifier;
    std::string composePosition;
    bool military;
    double frequency;
    std::vector<std::tuple<double, double>> regionMapPolygon;
};

struct IVAOATCAirport {
    std::string icao;
    std::string iata;
    std::string name;
    std::string countryId;
    double longitude;
    double latitude;
//    bool military;
    std::string city;
};

struct IVAOATCPosition : IVAOATCBase {
    std::string airportId;
    std::shared_ptr<IVAOATCAirport> airport;
    int order;
};

struct IVAOATCCenter : IVAOATCBase {
    std::string centerId;
    double longitude;
    double latitude;
};

struct IVAOATC {
    unsigned long id;
    std::string callsign;
    unsigned long userId;
    std::string connectionType;
    std::shared_ptr<IVAOATCSession> atcSession;
    std::shared_ptr<IVAOATCPosition> atcPosition;
    std::shared_ptr<IVAOATCCenter> subcenter;

    int getOrder() const {
        if (atcSession) {
            auto position = atcSession->position;
            if (position == "FSS") {
                return 0;
            } else if (position == "CTR") {
                return 1;
            } else if (position == "DEP") {
                return 2;
            } else if (position == "APP") {
                return 3;
            } else if (position == "TWR") {
                return 4;
            } else if (position == "GND") {
                return 5;
            } else if (position == "DEL") {
                return 6;
            }
        }
        return -1;
    }
};

class IVAO {
public:
    IVAO(std::string& apiKey);
    ~IVAO();

    void forEachATC(std::function<void(std::shared_ptr<IVAOATC>)> f);

private:
    std::string apiKey;
    std::unique_ptr<std::thread> updaterThread;
    std::mutex mutex;
    std::atomic_bool shutdown { false };

    std::shared_ptr<std::vector<std::shared_ptr<IVAOATC>>> atcs;

    void update();

    static bool compareAtcs(std::shared_ptr<IVAOATC>& atc1, std::shared_ptr<IVAOATC>& atc2);
    static size_t onData(void* buffer, size_t size, size_t nmemb, void* vecPtr);
    static int onProgress(void* client, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow);
};

} /* namespace ivao */

#endif /* SRC_LIBIVAO_IVAO_H_ */
