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
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include <iostream>
#include <curl/curl.h>

#include "IVAO.h"
#include "src/Logger.h"

#include "src/platform/Platform.h"

#define IVAO_WEBEYE_ATC_SUMMARY_URL "https://api.ivao.aero/v2/tracker/now/atc/summary"
//#define IVAO_WEBEYE_ATC_SUMMARY_URL "file:///home/artur/Development/Private/FlightSimulation/XPlane/avitab/ivao-atc8.json"

using json = nlohmann::json;
using namespace std::chrono_literals;

namespace ivao {

IVAO::IVAO(std::string& apiKey_): apiKey(apiKey_) {
    updaterThread = std::make_unique<std::thread>(&IVAO::update, this);
}

IVAO::~IVAO() {
    shutdown = true;
    updaterThread->join();
}

void IVAO::forEachATC(std::function<void(std::shared_ptr<IVAOATC>)> f) {
    if (!atcs) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    for (auto it : *atcs) {
        f(it);
    }
}

void IVAO::update() {
    bool first = true;
    auto start = std::chrono::high_resolution_clock::now();
    while (true) {
        std::this_thread::sleep_for(50ms);

        if (shutdown) {
            break;
        }

        auto now = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> diff = now - start;

        if (now - start < 5000ms && !first) {
            continue;
        }

        first = false;
        start = now;

        CURL *curl = curl_easy_init();
        bool cancel = false;
        std::vector<uint8_t> downloadBuf;

        curl_easy_setopt(curl, CURLOPT_URL, IVAO_WEBEYE_ATC_SUMMARY_URL);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "AviTab " AVITAB_VERSION_STR);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onProgress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancel);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &downloadBuf);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onData);

        struct curl_slist *headers = nullptr;
        std::string apiKeyHeader = std::string("ApiKey: ") + apiKey;
        headers = curl_slist_append(headers, apiKeyHeader.c_str());

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode code = curl_easy_perform(curl);

        curl_slist_free_all(headers);

        if (code != CURLE_OK) {
            if (code == CURLE_ABORTED_BY_CALLBACK) {
                curl_easy_cleanup(curl);
                logger::error("IVAO WebEye request cancelled");
                continue;
            } else {
                curl_easy_cleanup(curl);
                logger::error("IVAO WebEye request error: %s", curl_easy_strerror(code));
                continue;
            }
        }

        long httpStatus = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

        curl_easy_cleanup(curl);

        if (httpStatus != 200) {
//        if (httpStatus != 0) {
            logger::error("Invalid IVAO WebEye response: %d", httpStatus);
        } else {
            auto newAtcs = std::vector<std::shared_ptr<IVAOATC>>();

            try {
                auto atcsData = json::parse(downloadBuf);
                for (json::iterator it = atcsData.begin(); it != atcsData.end(); ++it) {
                    auto atcData = *it;
                    newAtcs.push_back(std::make_shared<IVAOATC>(IVAOATC {
                        atcData["id"].get<unsigned long>(),
                        atcData["callsign"].get<std::string>(),
                        atcData["userId"].get<unsigned long>(),
                        atcData["connectionType"].get<std::string>(),
                        std::make_shared<IVAOATCSession>(IVAOATCSession {
                            atcData["atcSession"]["frequency"].get<double>(),
                            atcData["atcSession"]["position"].get<std::string>()
                        }),
                        atcData["atcPosition"].is_null() ? nullptr : std::make_shared<IVAOATCPosition>(IVAOATCPosition {
                            atcData["atcPosition"]["atcCallsign"].get<std::string>(),
                            atcData["atcPosition"]["position"].get<std::string>(),
                            atcData["atcPosition"]["middleIdentifier"].is_null() ? "" : atcData["atcPosition"]["middleIdentifier"].get<std::string>(),
                            atcData["atcPosition"]["composePosition"].get<std::string>(),
                            atcData["atcPosition"]["military"].get<bool>(),
                            atcData["atcPosition"]["frequency"].get<double>(),
                            atcData["atcPosition"]["regionMapPolygon"].get<std::vector<std::tuple<double, double>>>(),
                            atcData["atcPosition"]["airportId"].get<std::string>(),
                            std::make_shared<IVAOATCAirport>(IVAOATCAirport {
                                atcData["atcPosition"]["airport"]["icao"].get<std::string>(),
                                atcData["atcPosition"]["airport"]["iata"].is_null() ? "" : atcData["atcPosition"]["airport"]["iata"].get<std::string>(),
                                atcData["atcPosition"]["airport"]["name"].get<std::string>(),
                                atcData["atcPosition"]["airport"]["countryId"].get<std::string>(),
                                atcData["atcPosition"]["airport"]["longitude"].get<double>(),
                                atcData["atcPosition"]["airport"]["latitude"].get<double>(),
//                                atcData["atcPosition"]["airport"]["military"].get<bool>(),
                                atcData["atcPosition"]["airport"]["city"].get<std::string>()
                            }),
                            atcData["atcPosition"]["order"].get<int>()
                        }),
                        atcData["subcenter"].is_null() ? nullptr : std::make_shared<IVAOATCCenter>(IVAOATCCenter {
                            atcData["subcenter"]["atcCallsign"].get<std::string>(),
                            atcData["subcenter"]["position"].get<std::string>(),
                            atcData["subcenter"]["middleIdentifier"].is_null() ? "" : atcData["subcenter"]["middleIdentifier"].get<std::string>(),
                            atcData["subcenter"]["composePosition"].get<std::string>(),
                            atcData["subcenter"]["military"].get<bool>(),
                            atcData["subcenter"]["frequency"].get<double>(),
                            atcData["subcenter"]["regionMapPolygon"].get<std::vector<std::tuple<double, double>>>(),
                            atcData["subcenter"]["centerId"].get<std::string>(),
                            atcData["subcenter"]["longitude"].get<double>(),
                            atcData["subcenter"]["latitude"].get<double>()
                        })
                    }));
                }
            } catch (const std::exception &e) {
                logger::error("Couldn't parse IVAO ATCs: %s", e.what());
                continue;
            }

            std::sort(newAtcs.begin(), newAtcs.end(), compareAtcs);

            std::lock_guard<std::mutex> lock(mutex);
            atcs = std::make_shared<std::vector<std::shared_ptr<IVAOATC>>>(newAtcs);

            logger::verbose("IVAO ATCs updated (%lu)", newAtcs.size());
        }
    }
}

bool IVAO::compareAtcs(std::shared_ptr<IVAOATC>& atc1, std::shared_ptr<IVAOATC>& atc2) {
    return atc1->getOrder() < atc2->getOrder();
}

size_t IVAO::onData(void* buffer, size_t size, size_t nmemb, void* vecPtr) {
    std::vector<uint8_t> *vec = reinterpret_cast<std::vector<uint8_t> *>(vecPtr);
    if (!vec) {
        return 0;
    }
    size_t pos = vec->size();
    vec->resize(pos + size * nmemb);
    std::memcpy(vec->data() + pos, buffer, size * nmemb);
    return size * nmemb;
}

int IVAO::onProgress(void* client, curl_off_t dlTotal, curl_off_t dlNow, curl_off_t ulTotal, curl_off_t ulNow) {
    bool *cancel = reinterpret_cast<bool *>(client);
    return *cancel;
}

} /* namespace ivao */
