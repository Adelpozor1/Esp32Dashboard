#pragma once

namespace geo {

// Distancia great-circle entre dos puntos (fórmula haversine), en kilómetros.
double distanciaKm(double lat1, double lon1, double lat2, double lon2);

// Rumbo inicial desde (lat1,lon1) hacia (lat2,lon2), en grados 0..359 (0 = norte).
int bearingGrados(double lat1, double lon1, double lat2, double lon2);

}  // namespace geo
