#ifndef KOHERON_PHASE_NOISE_TRACKING_LOCK_HPP
#define KOHERON_PHASE_NOISE_TRACKING_LOCK_HPP

#include <cmath>

class TrackingLock {
  public:
    void reset() { initialized = false; locked = false; }

    bool update(double error_hz, double alpha, double tolerance_hz) {
        if (!initialized) {
            filtered_error = error_hz;
            initialized = true;
        } else {
            filtered_error += alpha * (error_hz - filtered_error);
        }
        // Evaluate convergence at the loop bandwidth, with hysteresis.
        const double threshold = (locked ? 2.0 : 1.0) * tolerance_hz;
        locked = std::abs(filtered_error) < threshold;
        return locked;
    }

  private:
    double filtered_error = 0.0;
    bool initialized = false;
    bool locked = false;
};

#endif
