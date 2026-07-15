#include "geo_math.h"
#include <cmath>

namespace geo {

static constexpr double R_TIERRA_KM = 6371.0;
static constexpr double PI = 3.14159265358979323846;

static inline double aRadianes(double grados) {
  return grados * PI / 180.0;
}

static inline double aGrados(double radianes) {
  return radianes * 180.0 / PI;
}

double distanciaKm(double lat1, double lon1, double lat2, double lon2) {
  double dLat = aRadianes(lat2 - lat1);
  double dLon = aRadianes(lon2 - lon1);
  double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
             std::cos(aRadianes(lat1)) * std::cos(aRadianes(lat2)) *
             std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
  double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  return R_TIERRA_KM * c;
}

int bearingGrados(double lat1, double lon1, double lat2, double lon2) {
  double phi1 = aRadianes(lat1);
  double phi2 = aRadianes(lat2);
  double dLambda = aRadianes(lon2 - lon1);
  double y = std::sin(dLambda) * std::cos(phi2);
  double x = std::cos(phi1) * std::sin(phi2) -
             std::sin(phi1) * std::cos(phi2) * std::cos(dLambda);
  double theta = std::atan2(y, x);
  double grados = std::fmod(aGrados(theta) + 360.0, 360.0);
  return static_cast<int>(std::round(grados)) % 360;
}

}  // namespace geo
