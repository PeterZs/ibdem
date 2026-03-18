#pragma once

#include <vector>
#include "Particle.h"
#include "Bond.h"

// Configuration for one beam instance.
struct BeamConfig {
  float L;           // beam length along X (m)
  float H;           // beam height along Y (m)
  float W;           // beam width  along Z (m)
  float r;           // particle radius (m)
  float density;     // material density (kg/m^3)
  float E;           // Young's modulus (Pa)
  float nu;          // Poisson's ratio
  float tauC;        // shear fracture threshold (Pa)
  const char* label; // display name ("Low", "Middle", "High")
};

// Generate spherical particles packed on a 2-D triangular lattice in the Y-Z
// cross-section, repeated along the X (beam) axis with spacing 2r.
// This approximates hexagonal close packing as used in Section 3.4.
std::vector<Particle> generateBeam(const BeamConfig& cfg);

// Generate bonds between all particle pairs whose centre-to-centre distance
// is less than thresholdFactor * r.  Uses a spatial hash grid for O(n) cost.
// Default threshold 2.1r captures all nearest neighbours in the lattice (dist = 2r).
std::vector<Bond> generateBonds(
  const std::vector<Particle>& particles,
  float thresholdFactor = 2.1f);

// Mark support and load particles for the three-point bending test:
//   Support 1 : near (0.10 L, 0,   0.5 W)  -- bottom-left
//   Support 2 : near (0.90 L, 0,   0.5 W)  -- bottom-right
//   Load      : near (0.50 L, H,   0.5 W)  -- top-centre
void tagBoundaryParticles(std::vector<Particle>& particles, const BeamConfig& cfg);
