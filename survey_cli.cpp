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
    const double altitudeAGL_m = (altFromArg > 0.0) ? altFromArg : 10.0;

    const bool hasHome = (argc >= 4);
    Geo home{};
    if (hasHome) {
        home.lat = std::atof(argv[2]);
        home.lon = std::atof(argv[3]);
        home.alt = 0.0;
    }

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
    CameraSpec cam{};
    cam.frontOverlapPct = 25;
    cam.sideOverlapPct  = 25;
    cam.sensorWidth_mm  = 3.68;
    cam.sensorHeight_mm = 2.76;
    cam.focalLength_mm  = 3.04;
    cam.imageWidth_px   = 3280;
    cam.imageHeight_px  = 2474;

    // 3) SurveyParams (ίδιοι με main.cpp για αρχή)
    SurveyParams params{};
    params.altitudeAGL_m      = altitudeAGL_m;
    params.gridAngle_deg      = 90.0; // will be auto-updated from polygon below
    params.gridSpacing_m      = -1.0;    // auto από camera/overlap
    params.triggerDist_m      = -1.0;    // auto από camera/overlap
    params.turnaround_m       = 0.0;
    params.alternateTransects = false;
    params.refly90            = false;
    params.entryCorner        = EntryCorner::TopLeft;

    // Auto grid angle: align transects with polygon major axis (PCA) to reduce total flight distance.
    // In generateSurvey(), base transect direction is +North; rotating by gridAngle_deg rotates transects.
    // So we choose gridAngle such that transect direction follows the polygon's long axis.
    if (polygon.size() >= 3) {
        const Geo origin = polygon.front();

        // Convert polygon vertices to local NED plane (x=East, y=North)
        std::vector<nqt::Pt> pts;
        pts.reserve(polygon.size());
        for (size_t i = 0; i < polygon.size(); ++i) {
            const nqt::Pt p = (i == 0) ? nqt::Pt{0, 0} : nqt::convertGeoToNed(polygon[i], origin);
            pts.push_back(p);
        }

        // Mean
        double mx = 0.0, my = 0.0;
        for (const auto& p : pts) { mx += p.x; my += p.y; }
        mx /= static_cast<double>(pts.size());
        my /= static_cast<double>(pts.size());

        // Covariance (2x2)
        double sxx = 0.0, sxy = 0.0, syy = 0.0;
        for (const auto& p : pts) {
            const double dx = p.x - mx;
            const double dy = p.y - my;
            sxx += dx * dx;
            sxy += dx * dy;
            syy += dy * dy;
        }

        // Principal axis angle θ (radians) of the largest eigenvector:
        // θ = 0.5 * atan2(2*sxy, sxx - syy)
        const double theta = 0.5 * std::atan2(2.0 * sxy, (sxx - syy));
        const double majorAxisDeg = nqt::rad2deg(theta); // angle from +East toward +North

        // Transect direction angle = 90deg + gridAngle_deg (since base is +North).
        // Set gridAngle so that transect direction aligns to major axis.
        params.gridAngle_deg = nqt::clampGridAngle90(majorAxisDeg - 90.0);
    }

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

    // 4) ΚΑΛΕΣΕ generateSurvey
    std::vector<Waypoint> wps = generateSurvey(polygon, cam, params);

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
