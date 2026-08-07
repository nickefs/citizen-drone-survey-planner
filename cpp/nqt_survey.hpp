#pragma once
#include <vector>
#include <cstdint>
#include "nqt_helpers.hpp" // για nqt::Geo

// ---- Business/API types (όχι στο nqt_helpers) ----
struct CameraSpec {
    double frontOverlapPct{70};
    double sideOverlapPct{60};
    double sensorWidth_mm{6.45};
    double sensorHeight_mm{4.80};
    double focalLength_mm{4.5};
    int    imageWidth_px{4000};
    int    imageHeight_px{3000};
};

enum class EntryCorner : uint8_t {
    TopLeft = 0, TopRight, BottomRight, BottomLeft
};

struct SurveyParams {
    double altitudeAGL_m      = 80.0;
    double gridAngle_deg      = 0.0;     // προσανατολισμός κάνναβου
    double gridSpacing_m      = -1.0;    // αν <=0, θα υπολογιστεί από camera/overlap
    double triggerDist_m      = -1.0;    // αν <=0, θα υπολογιστεί από camera/overlap
    double turnaround_m       = 0.0;     // απόσταση επέκτασης πριν/μετά
    bool   refly90            = false;   // (προαιρετικό) δεύτερο πέρασμα 90°
    bool   alternateTransects = false;   // εναλλάξ διάταξη
    EntryCorner entryCorner   = EntryCorner::TopLeft;
};

enum class WaypointType : uint8_t { SurveyEntry, SurveyExit, Turnaround, InteriorTrigger };

struct Waypoint {
    nqt::Geo    coord;
    WaypointType type;
};

// κύριο API
std::vector<Waypoint>
generateSurvey(const std::vector<nqt::Geo>& polygonLLA,
               const CameraSpec& cam,
               SurveyParams params);

// προαιρετικό helper (μένει στο .cpp — ΟΧΙ στο nqt_helpers.hpp)
void computeSpacingAndTriggerFromCamera(const CameraSpec& cam,
                                        double altitudeAGL_m,
                                        double sideOverlapPct,
                                        double frontOverlapPct,
                                        double& out_gridSpacing_m,
                                        double& out_triggerDist_m);
