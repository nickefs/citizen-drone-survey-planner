// survey_cli.cpp
// Διαβάζει polygon από stdin και τυπώνει JSON με waypoints στο stdout

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdlib>
#include <limits>
#include <algorithm>
#include <cmath>

#include "nqt_survey.hpp"  // δηλώνει: nqt::Geo, CameraSpec, SurveyParams, Waypoint, WaypointType, generateSurvey

using nqt::Geo;
using nqt::distance;

namespace {
constexpr double kDefaultAltitude_m = 10.0;
constexpr double kDefaultFrontOverlapPct = 25.0;
constexpr double kDefaultSideOverlapPct = 25.0;
constexpr double kDefaultSensorWidth_mm = 3.68;
constexpr double kDefaultSensorHeight_mm = 2.76;
constexpr double kDefaultFocalLength_mm = 3.04;
constexpr int kDefaultImageWidth_px = 3280;
constexpr int kDefaultImageHeight_px = 2474;
constexpr double kDefaultTurnaround_m = 3.0;

bool readDoubleArg(int argc, char* argv[], int& i, double& out) {
    if (i + 1 >= argc) return false;
    out = std::atof(argv[++i]);
    return true;
}

bool readIntArg(int argc, char* argv[], int& i, int& out) {
    if (i + 1 >= argc) return false;
    out = std::atoi(argv[++i]);
    return true;
}

bool isFlag(const char* value) {
    return value && value[0] == '-';
}
}

// Helper: waypoint type -> string
static const char* toString(WaypointType t) {
    switch (t) {
        case WaypointType::SurveyEntry:     return "SurveyEntry";
        case WaypointType::SurveyExit:      return "SurveyExit";
        case WaypointType::Turnaround:      return "Turnaround";
        case WaypointType::InteriorTrigger: return "InteriorTrigger";
        default:                            return "Unknown";
    }
}

int main(int argc, char* argv[]) {
    const double altFromArg = (argc > 1) ? std::atof(argv[1]) : 0.0;
    const double altitudeAGL_m = (altFromArg > 0.0) ? altFromArg : kDefaultAltitude_m;

    CameraSpec cam{};
    cam.frontOverlapPct = kDefaultFrontOverlapPct;
    cam.sideOverlapPct  = kDefaultSideOverlapPct;
    cam.sensorWidth_mm  = kDefaultSensorWidth_mm;
    cam.sensorHeight_mm = kDefaultSensorHeight_mm;
    cam.focalLength_mm  = kDefaultFocalLength_mm;
    cam.imageWidth_px   = kDefaultImageWidth_px;
    cam.imageHeight_px  = kDefaultImageHeight_px;

    double turnaround_m = kDefaultTurnaround_m;
    bool hasHome = false;
    Geo home{};
    int argi = 2;

    if (argc >= 4 && !isFlag(argv[2])) {
        hasHome = true;
        home.lat = std::atof(argv[2]);
        home.lon = std::atof(argv[3]);
        home.alt = 0.0;
        argi = 4;
    }

    for (int i = argi; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--home" && i + 2 < argc) {
            hasHome = true;
            home.lat = std::atof(argv[++i]);
            home.lon = std::atof(argv[++i]);
            home.alt = 0.0;
        } else if (flag == "--sensor-width") {
            readDoubleArg(argc, argv, i, cam.sensorWidth_mm);
        } else if (flag == "--sensor-height") {
            readDoubleArg(argc, argv, i, cam.sensorHeight_mm);
        } else if (flag == "--focal-length") {
            readDoubleArg(argc, argv, i, cam.focalLength_mm);
        } else if (flag == "--image-width") {
            readIntArg(argc, argv, i, cam.imageWidth_px);
        } else if (flag == "--image-height") {
            readIntArg(argc, argv, i, cam.imageHeight_px);
        } else if (flag == "--front-overlap") {
            readDoubleArg(argc, argv, i, cam.frontOverlapPct);
        } else if (flag == "--side-overlap") {
            readDoubleArg(argc, argv, i, cam.sideOverlapPct);
        } else if (flag == "--turnaround") {
            readDoubleArg(argc, argv, i, turnaround_m);
        }
    }

    if (cam.sensorWidth_mm <= 0) cam.sensorWidth_mm = kDefaultSensorWidth_mm;
    if (cam.sensorHeight_mm <= 0) cam.sensorHeight_mm = kDefaultSensorHeight_mm;
    if (cam.focalLength_mm <= 0) cam.focalLength_mm = kDefaultFocalLength_mm;
    if (cam.imageWidth_px <= 0) cam.imageWidth_px = kDefaultImageWidth_px;
    if (cam.imageHeight_px <= 0) cam.imageHeight_px = kDefaultImageHeight_px;
    if (cam.frontOverlapPct < 0 || cam.frontOverlapPct > 95) cam.frontOverlapPct = kDefaultFrontOverlapPct;
    if (cam.sideOverlapPct < 0 || cam.sideOverlapPct > 95) cam.sideOverlapPct = kDefaultSideOverlapPct;
    if (turnaround_m < 0) turnaround_m = kDefaultTurnaround_m;

    // 1) ΔΙΑΒΑΣΜΑ ΠΟΛΥΓΩΝΟΥ ΑΠΟ stdin
    // Περιμένουμε γραμμές: lat lon
    std::vector<Geo> polygon;
    double lat, lon;

    while (std::cin >> lat >> lon) {
        Geo g;
        g.lat = lat;
        g.lon = lon;
        g.alt = 0.0; // AGL 0 ως βάση, το πραγματικό altitude θα μπει από params
        polygon.push_back(g);
    }

    if (polygon.size() < 3) {
        std::cerr << "Not enough points for polygon\n";
        // Επιστρέφουμε ένα άδειο JSON 
        std::cout << "{\"waypoints\":[]}";
        return 0;
    }

    // 2) CameraSpec (ίδια με το demo στο main.cpp)
    // 3) SurveyParams (ίδιοι με main.cpp για αρχή)
    SurveyParams params{};
    params.altitudeAGL_m      = altitudeAGL_m;
    params.gridAngle_deg      = 90.0; // will be auto-updated from polygon below
    params.gridSpacing_m      = -1.0;    // auto από camera/overlap
    params.triggerDist_m      = -1.0;    // auto από camera/overlap
    params.turnaround_m       = turnaround_m;
    params.alternateTransects = false;
    params.refly90            = false;
    params.entryCorner        = EntryCorner::TopLeft;

    // Αν υπάρχει Home, διάλεξε entryCorner ως το bbox corner του polygon (σε local NED)
    // που είναι πιο κοντά στο Home.
    if (hasHome && polygon.size() >= 3) {
        const Geo origin = polygon.front();
        const nqt::Pt homeNed = nqt::convertGeoToNed(home, origin);

        std::vector<nqt::Pt> polyNed;
        polyNed.reserve(polygon.size() + 1);
        for (size_t i = 0; i < polygon.size(); ++i) {
            const nqt::Pt p = (i == 0) ? nqt::Pt{0, 0} : nqt::convertGeoToNed(polygon[i], origin);
            polyNed.push_back(p);
        }
        polyNed = nqt::closePolygon(polyNed);

        nqt::Pt bmin{}, bmax{};
        nqt::polygonBounds(polyNed, bmin, bmax);

        const nqt::Pt tl{bmin.x, bmax.y};
        const nqt::Pt tr{bmax.x, bmax.y};
        const nqt::Pt br{bmax.x, bmin.y};
        const nqt::Pt bl{bmin.x, bmin.y};

        auto d2 = [](const nqt::Pt& a, const nqt::Pt& b) {
            const double dx = a.x - b.x, dy = a.y - b.y;
            return dx * dx + dy * dy;
        };

        double best = d2(homeNed, tl);
        params.entryCorner = EntryCorner::TopLeft;

        const double dtr = d2(homeNed, tr);
        if (dtr < best) { best = dtr; params.entryCorner = EntryCorner::TopRight; }

        const double dbr = d2(homeNed, br);
        if (dbr < best) { best = dbr; params.entryCorner = EntryCorner::BottomRight; }

        const double dbl = d2(homeNed, bl);
        if (dbl < best) { /*best = dbl;*/ params.entryCorner = EntryCorner::BottomLeft; }
    }

    // 4) Δοκίμασε διαφορετικές γωνίες και κράτα αυτή με το μικρότερο "cost"
    // cost = συνολικό μήκος διαδρομής + penalty για στροφές (approximated)
    constexpr double kTurnPenalty_m = 25.0;
    auto score = [&](const std::vector<Waypoint>& wps) -> double {
        if (wps.size() < 2) return std::numeric_limits<double>::infinity();
        double total = 0.0;
        for (size_t i = 1; i < wps.size(); ++i) {
            total += nqt::distance(wps[i - 1].coord, wps[i].coord);
        }
        size_t entries = 0;
        for (const auto& w : wps) if (w.type == WaypointType::SurveyEntry) ++entries;
        const size_t turns = (entries > 0) ? (entries - 1) : 0;
        total += static_cast<double>(turns) * kTurnPenalty_m;
        return total;
    };

    auto evalAngle = [&](double angDeg) -> std::pair<double, std::vector<Waypoint>> {
        SurveyParams p = params;
        p.gridAngle_deg = angDeg;
        auto w = generateSurvey(polygon, cam, p);
        const double s = score(w);
        return {s, std::move(w)};
    };

    // coarse sweep
    double bestAngle = params.gridAngle_deg;
    double bestScore = std::numeric_limits<double>::infinity();
    std::vector<Waypoint> bestWps;

    for (double ang = -90.0; ang <= 90.0; ang += 5.0) {
        auto [s, w] = evalAngle(ang);
        if (s < bestScore) { bestScore = s; bestAngle = ang; bestWps = std::move(w); }
    }

    // refine sweep around best coarse angle
    for (double ang = bestAngle - 5.0; ang <= bestAngle + 5.0; ang += 1.0) {
        auto [s, w] = evalAngle(ang);
        if (s < bestScore) { bestScore = s; bestAngle = ang; bestWps = std::move(w); }
    }

    std::vector<Waypoint> wps = std::move(bestWps);

    // 5) ΤΥΠΩΣΕ JSON ΣΤΟ stdout
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "{ \"waypoints\": [";

    for (size_t i = 0; i < wps.size(); ++i) {
        const auto& w = wps[i];
        std::cout
            << "{"
            << "\"lat\":"  << w.coord.lat << ","
            << "\"lon\":"  << w.coord.lon << ","
            << "\"alt_m\":" << w.coord.alt << ","
            << "\"type\":\"" << toString(w.type) << "\""
            << "}";
        if (i + 1 < wps.size()) {
            std::cout << ",";
        }
    }

    std::cout << "] }";

    return 0;
}
