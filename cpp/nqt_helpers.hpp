#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace nqt {

// ------------------ βασικοί τύποι (Qt-free) ------------------
struct Geo { double lat{0}, lon{0}, alt{0}; };   // degrees, degrees, meters
struct Pt  { double x{0}, y{0}; };               // local plane: x=East, y=North
struct Segment { Pt a, b; };

// ------------------ constants & conversions ------------------
inline constexpr double kEarthR = 6371000.0; // meters

inline double deg2rad(double d) { return d * M_PI / 180.0; }
inline double rad2deg(double r) { return r * 180.0 / M_PI; }

// κρατά τη γωνία στο [-90, 90] (για σταθερό προσανατολισμό γραμμών)
inline double clampGridAngle90(double ang){
    if (ang >  90.0) ang -= 180.0;
    else if (ang < -90.0) ang += 180.0;
    return ang;
}

// ------------------ γεωδαισία (σφαίρα) ------------------
inline double distance(const Geo& a, const Geo& b){
    const double φ1=deg2rad(a.lat), φ2=deg2rad(b.lat);
    const double dφ=φ2-φ1, dλ=deg2rad(b.lon-a.lon);
    const double s = std::pow(std::sin(dφ/2),2)
                   + std::cos(φ1)*std::cos(φ2)*std::pow(std::sin(dλ/2),2);
    return 2.0 * kEarthR * std::asin(std::min(1.0, std::sqrt(s)));
}

inline double azimuth(const Geo& a, const Geo& b){
    const double φ1=deg2rad(a.lat), φ2=deg2rad(b.lat);
    const double dλ=deg2rad(b.lon-a.lon);
    const double y = std::sin(dλ)*std::cos(φ2);
    const double x = std::cos(φ1)*std::sin(φ2) - std::sin(φ1)*std::cos(φ2)*std::cos(dλ);
    double θ = std::atan2(y,x);
    double deg = std::fmod(rad2deg(θ) + 360.0, 360.0);
    return deg;
}

inline Geo atDistanceAzimuth(const Geo& a, double dist_m, double azimuth_deg, double alt_m){
    const double δ = dist_m / kEarthR;
    const double θ = deg2rad(azimuth_deg);
    const double φ1 = deg2rad(a.lat), λ1 = deg2rad(a.lon);
    const double sinφ1 = std::sin(φ1), cosφ1 = std::cos(φ1);
    const double sinδ  = std::sin(δ),  cosδ  = std::cos(δ);
    const double sinφ2 = sinφ1*cosδ + cosφ1*sinδ*std::cos(θ);
    const double φ2 = std::asin(sinφ2);
    const double λ2 = λ1 + std::atan2(std::sin(θ)*sinδ*cosφ1, cosδ - sinφ1*sinφ2);
    return { rad2deg(φ2), rad2deg(λ2), alt_m };
}

// ------------------ LLA <-> τοπικό επίπεδο ------------------
inline Pt convertGeoToNed(const Geo& p, const Geo& origin){
    const double lat0 = deg2rad(origin.lat);
    const double dLat = deg2rad(p.lat - origin.lat);
    const double dLon = deg2rad(p.lon - origin.lon);
    return { dLon * kEarthR * std::cos(lat0), dLat * kEarthR };
}
inline Geo convertNedToGeo(const Pt& ned, const Geo& origin, double alt){
    const double lat0 = deg2rad(origin.lat);
    const double dLat = ned.y / kEarthR;
    const double dLon = ned.x / (kEarthR * std::cos(lat0));
    return { origin.lat + rad2deg(dLat), origin.lon + rad2deg(dLon), alt };
}

// ------------------ 2D γεωμετρία/πολύγωνο ------------------
inline Pt rotatePoint(const Pt& p, const Pt& origin, double angleDeg){
    const double r = (M_PI/180.0) * -angleDeg;
    const double cx = p.x - origin.x, cy = p.y - origin.y;
    return { cx*std::cos(r) - cy*std::sin(r) + origin.x,
             cx*std::sin(r) + cy*std::cos(r) + origin.y };
}

inline bool segIntersect(const Pt& A, const Pt& B, const Pt& C, const Pt& D, Pt& out){
    auto sub=[](Pt u, Pt v){ return Pt{u.x-v.x, u.y-v.y}; };
    auto cross=[](Pt u, Pt v){ return u.x*v.y - u.y*v.x; };
    const Pt r=sub(B,A), s=sub(D,C); const double rxs = cross(r,s);
    if (std::fabs(rxs) < 1e-12) return false;
    const double t = cross(sub(C,A), s)/rxs;
    const double u = cross(sub(C,A), r)/rxs;
    if (t>=0 && t<=1 && u>=0 && u<=1){ out={A.x + t*r.x, A.y + t*r.y}; return true; }
    return false;
}

inline void intersectLinesWithPolygon(const std::vector<Segment>& lines,
                                      const std::vector<Pt>& polyClosed,
                                      std::vector<Segment>& result)
{
    result.clear();
    for (const auto& L: lines){
        std::vector<Pt> hits;
        for (size_t j=0; j+1<polyClosed.size(); ++j){
            Pt P;
            if (segIntersect(L.a, L.b, polyClosed[j], polyClosed[j+1], P)){
                bool dup=false;
                for (auto& h: hits) if (std::hypot(h.x-P.x, h.y-P.y) < 1e-8){ dup=true; break; }
                if (!dup) hits.push_back(P);
            }
        }
        if (hits.size() >= 2){
            double best=0; Pt A{},B{};
            for (size_t i=0;i<hits.size();++i)
                for (size_t k=0;k<hits.size();++k){
                    const double d = std::hypot(hits[i].x-hits[k].x, hits[i].y-hits[k].y);
                    if (d>best){ best=d; A=hits[i]; B=hits[k]; }
                }
            result.push_back({A,B});
        }
    }
}

inline void adjustLineDirection(const std::vector<Segment>& in, std::vector<Segment>& out){
    out.clear();
    bool have=false; double base=0;
    auto ang=[](const Segment& s){ return std::atan2(s.b.y-s.a.y, s.b.x-s.a.x); };
    for (auto s : in){
        if (!have){ base=ang(s); have=true; }
        if (std::fabs(ang(s)-base) > (1.0*M_PI/180.0)) std::swap(s.a, s.b);
        out.push_back(s);
    }
}

// helpers για bbox/κλείσιμο πολύγωνου (χρήσιμα στον generator)
inline std::vector<Pt> closePolygon(std::vector<Pt> poly){
    if (poly.empty()) return poly;
    const Pt& f = poly.front(); const Pt& b = poly.back();
    if (std::hypot(f.x - b.x, f.y - b.y) > 1e-9) poly.push_back(f);
    return poly;
}

inline void polygonBounds(const std::vector<Pt>& poly, Pt& minP, Pt& maxP){
    double minx= 1e300, miny= 1e300, maxx=-1e300, maxy=-1e300;
    for (const auto& p : poly){
        minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
        miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
    }
    minP = {minx, miny}; maxP = {maxx, maxy};
}

} // namespace nqt
