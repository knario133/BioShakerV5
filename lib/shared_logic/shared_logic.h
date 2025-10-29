#ifndef SHARED_LOGIC_H
#define SHARED_LOGIC_H

// ============================
// Constantes Compartidas
// ============================
const double SPR_CMD = 3200; // Steps per revolution for RPM to SPS conversion

/**
 * @brief Converts RPM (Revolutions Per Minute) to SPS (Steps Per Second).
 *
 * @param rpm The speed in RPM.
 * @return double The speed in SPS.
 */
inline double rpm2sps(double rpm) {
    return (rpm / 60.0) * SPR_CMD;
}

#endif // SHARED_LOGIC_H
