#include "nqt_survey.hpp"
#include "nqt_helpers.hpp"
#include <algorithm>
#include <cmath>

using nqt::Geo; using nqt::Pt; using nqt::Segment;

// --------- camera math (μένει εδώ, όχι στο nqt_helpers) ---------
void computeSpacingAndTriggerFromCamera(const CameraSpec& cam,
                                        double altitudeAGL_m,
                                        double sideOverlapPct,
                                        double frontOverlapPct,
                                        double& out_gridSpacing_m,
                                        double& out_triggerDist_m)
{
    // απλή προσέγγιση: footprint ~ altitude * sensor / focal
    const double gsd_w = (altitudeAGL_m * cam.sensorWidth_mm  / cam.focalLength_mm) / cam.imageWidth_px;
    const double gsd_h = (altitudeAGL_m * cam.sensorHeight_mm / cam.focalLength_mm) / cam.imageHeight_px;
    const double footprint_w = gsd_w * cam.imageWidth_px;   // πλάτος λωρίδας
    const double footprint_h = gsd_h * cam.imageHeight_px;  // μήκος λήψης
    out_gridSpacing_m = footprint_w * (1.0 - sideOverlapPct  / 100.0);
    out_triggerDist_m = footprint_h * (1.0 - frontOverlapPct / 100.0);
}

// ---------------------- κύρια λογική ----------------------
std::vector<Waypoint>
generateSurvey(const std::vector<Geo>& polygonLLA,
               const CameraSpec& cam,
               SurveyParams params)
{
    std::vector<Waypoint> out;
    if (polygonLLA.size() < 3) return out;

    // 1) spacing/trigger, αν δεν δόθηκαν
    if (params.gridSpacing_m <= 0 || params.triggerDist_m <= 0){
        double s = params.gridSpacing_m, t = params.triggerDist_m;
        computeSpacingAndTriggerFromCamera(cam, params.altitudeAGL_m,
                                           cam.sideOverlapPct, cam.frontOverlapPct, s, t);
        if (params.gridSpacing_m <= 0) params.gridSpacing_m = s;
        if (params.triggerDist_m <= 0) params.triggerDist_m = t;
    }

    // 2) LLA -> τοπικό επίπεδο (origin: πρώτο vertex)
    const Geo origin = polygonLLA.front();
    std::vector<Pt> poly;
    poly.reserve(polygonLLA.size()+1);
    for (size_t i=0;i<polygonLLA.size();++i){
        Pt p = (i==0)? Pt{0,0} : nqt::convertGeoToNed(polygonLLA[i], origin);
        poly.push_back(p);
    }
    poly = nqt::closePolygon(poly);

    // 3) φτιάξε παράλληλες γραμμές στο expanded bbox, με gridAngle
    const double gridAngle = nqt::clampGridAngle90(params.gridAngle_deg);

    Pt bmin, bmax; nqt::polygonBounds(poly, bmin, bmax);
    const Pt C{ (bmin.x+bmax.x)/2.0, (bmin.y+bmax.y)/2.0 };
    const double maxW = std::max(bmax.x-bmin.x, bmax.y-bmin.y) + 2000.0;
    const double half = maxW/2.0;

    std::vector<Segment> rawLines;
    for (double x=C.x - half; x < C.x + half; x += params.gridSpacing_m){
        Pt top{ x, C.y - half }, bot{ x, C.y + half };
        rawLines.push_back({ nqt::rotatePoint(top, C, gridAngle),
                             nqt::rotatePoint(bot, C, gridAngle) });
    }

    // 4) κλιπάρισε τις γραμμές με το πολύγωνο
    std::vector<Segment> cut;
    nqt::intersectLinesWithPolygon(rawLines, poly, cut);

    // fallback: αν πήρες <2, κέντραρε μία γραμμή
    if (cut.size() < 2 && !rawLines.empty()){
        Segment first = rawLines.front();
        Pt mid{ (first.a.x+first.b.x)/2.0, (first.a.y+first.b.y)/2.0 };
        Pt d{ C.x - mid.x, C.y - mid.y };
        first.a.x += d.x; first.a.y += d.y; first.b.x += d.x; first.b.y += d.y;
        rawLines = { first };
        cut.clear();
        nqt::intersectLinesWithPolygon(rawLines, poly, cut);
    }

    // 5) ομοιόμορφη κατεύθυνση
    std::vector<Segment> lines;
    nqt::adjustLineDirection(cut, lines);

    // 6) σε LLA (transects με 2 σημεία)
    std::vector<std::vector<Geo>> transects;
    transects.reserve(lines.size());
    for (const auto& L : lines){
        Geo A = nqt::convertNedToGeo(L.a, origin, params.altitudeAGL_m);
        Geo B = nqt::convertNedToGeo(L.b, origin, params.altitudeAGL_m);
        transects.push_back({A,B});
    }

    // 7) entry corner & alternate
    auto adjustToEntryCorner = [](std::vector<std::vector<Geo>>& trs, EntryCorner c){
        if (trs.empty()) return;
        const bool reversePts = (c==EntryCorner::BottomLeft || c==EntryCorner::BottomRight);
        const bool reverseTrs = (c==EntryCorner::TopRight   || c==EntryCorner::BottomRight);
        if (reversePts) for (auto& t: trs) std::reverse(t.begin(), t.end());
        if (reverseTrs) std::reverse(trs.begin(), trs.end());
    };
    adjustToEntryCorner(transects, params.entryCorner);

    if (params.alternateTransects){
        std::vector<std::vector<Geo>> alt;
        for (size_t i=0;i<transects.size();++i) if (!(i&1)) alt.push_back(transects[i]);
        for (size_t i=transects.size(); i>0; --i) if (((i-1)&1)) alt.push_back(transects[i-1]);
        transects.swap(alt);
    }

    // 8) lawn-mower (αντιστροφή ανά δεύτερο)
    bool rev=false; for (auto& t: transects){ if (rev) std::reverse(t.begin(), t.end()); rev=!rev; }

    // 9) turnarounds + interior triggers
    for (const auto& tr : transects){
        const Geo A = tr.front();
        const Geo B = tr.back();

        if (params.turnaround_m > 0){
            const double azAB = nqt::azimuth(A,B);
            const double azBA = nqt::azimuth(B,A);
            Geo pre  = nqt::atDistanceAzimuth(A, -params.turnaround_m, azAB, params.altitudeAGL_m);
            Geo post = nqt::atDistanceAzimuth(B, -params.turnaround_m, azBA, params.altitudeAGL_m);
            out.push_back({pre,  WaypointType::Turnaround});
        }

        out.push_back({A, WaypointType::SurveyEntry});

        if (params.triggerDist_m > 0){
            const double L = nqt::distance(A,B);
            const double az = nqt::azimuth(A,B);
            const int k = static_cast<int>(std::floor(L / params.triggerDist_m));
            for (int i=1;i<=k;++i){
                Geo mid = nqt::atDistanceAzimuth(A, params.triggerDist_m*i, az, params.altitudeAGL_m);
                out.push_back({mid, WaypointType::InteriorTrigger});
            }
        }

        out.push_back({B, WaypointType::SurveyExit});

        if (params.turnaround_m > 0){
            const double azBA = nqt::azimuth(B,A);
            Geo post = nqt::atDistanceAzimuth(B, -params.turnaround_m, azBA, params.altitudeAGL_m);
            out.push_back({post, WaypointType::Turnaround});
        }
    }

    return out;
}
