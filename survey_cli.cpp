// survey_cli.cpp
// Διαβάζει polygon από stdin και τυπώνει JSON με waypoints στο stdout

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cstdlib>

#include "nqt_survey.hpp"  // δηλώνει: nqt::Geo, CameraSpec, SurveyParams, Waypoint, WaypointType, generateSurvey

using nqt::Geo;

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
    params.gridAngle_deg      = 90.0;
    params.gridSpacing_m      = -1.0;    // auto από camera/overlap
    params.triggerDist_m      = -1.0;    // auto από camera/overlap
    params.turnaround_m       = 0.0;
    params.alternateTransects = false;
    params.refly90            = false;
    params.entryCorner        = EntryCorner::TopLeft;

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
