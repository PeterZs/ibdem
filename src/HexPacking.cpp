#include "HexPacking.h"

#include <cmath>
#include <tuple>
#include <unordered_map>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Quaternion that rotates the unit vector 'from' onto 'to'.
// Both vectors are assumed to be normalised.
static glm::quat rotationBetween(const glm::vec3& from, const glm::vec3& to) {
  float cosA = glm::clamp(glm::dot(from, to), -1.0f, 1.0f);
  if (cosA > 0.9999f) {
    // Vectors are parallel -- identity quaternion
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
  }
  if (cosA < -0.9999f) {
    // Vectors are anti-parallel -- rotate 180 deg around a perpendicular axis
    glm::vec3 perp = glm::cross(from, glm::vec3(0.0f, 1.0f, 0.0f));
    if (glm::length(perp) < 1e-4f)
      perp = glm::cross(from, glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::angleAxis(glm::pi<float>(), glm::normalize(perp));
  }
  glm::vec3 axis = glm::normalize(glm::cross(from, to));
  return glm::angleAxis(std::acos(cosA), axis);
}

// ---------------------------------------------------------------------------
// generateBeam
// ---------------------------------------------------------------------------

std::vector<Particle> generateBeam(const BeamConfig& cfg) {
  std::vector<Particle> particles;

  const float r = cfg.r;
  // X: uniform spacing 2r along the beam axis
  const float dx = 2.0f * r;
  // Y: row spacing for 2-D triangular packing  dy = r * sqrt(3)
  const float dy = r * std::sqrt(3.0f);
  // Z: column spacing 2r; odd Y-rows are offset by r
  const float dz = 2.0f * r;

  // Particle volume and mass
  const float vol  = (4.0f / 3.0f) * glm::pi<float>() * r * r * r;
  const float mass = cfg.density * vol;

  // Small epsilon to include particles exactly on the boundary
  const float eps = r * 1e-3f;

  for (int ix = 0; ; ++ix) {
    float x = ix * dx;
    if (x > cfg.L + eps) break;

    for (int iy = 0; ; ++iy) {
      float y = iy * dy;
      if (y > cfg.H + eps) break;

      // Alternate rows offset by r along Z for triangular (hex) packing
      float zOff = (iy % 2 == 0) ? 0.0f : r;

      for (int iz = 0; ; ++iz) {
        float z = iz * dz + zOff;
        if (z > cfg.W + eps) break;

        Particle p;
        p.pos     = glm::vec3(x, y, z);
        p.rot     = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity
        p.vel     = glm::vec3(0.0f);
        p.angVel  = glm::vec3(0.0f);
        p.mass    = mass;
        p.radius  = r;
        p.fixed     = false;
        p.isSupport = false;
        p.isLoad    = false;

        particles.push_back(p);
      }
    }
  }

  return particles;
}

// ---------------------------------------------------------------------------
// generateBonds  (spatial hash grid, O(n) expected cost)
// ---------------------------------------------------------------------------

std::vector<Bond> generateBonds(
    const std::vector<Particle>& particles,
    float thresholdFactor)
{
  if (particles.empty()) return {};

  const float r         = particles[0].radius;
  const float threshold = thresholdFactor * r;
  const float cellSize  = threshold;

  // ---- Build spatial hash grid ----
  // Key: (cx, cy, cz) cell index triple
  struct CellKey {
    int x, y, z;
    bool operator==(const CellKey& o) const {
      return x == o.x && y == o.y && z == o.z;
    }
  };
  struct CellHash {
    size_t operator()(const CellKey& k) const {
      size_t h = 2166136261u;
      auto mix = [&](int v) {
        h ^= static_cast<size_t>(v);
        h *= 16777619u;
      };
      mix(k.x); mix(k.y); mix(k.z);
      return h;
    }
  };

  std::unordered_map<CellKey, std::vector<int>, CellHash> grid;
  grid.reserve(particles.size());

  for (int i = 0; i < (int)particles.size(); ++i) {
    CellKey key {
      (int)std::floor(particles[i].pos.x / cellSize),
      (int)std::floor(particles[i].pos.y / cellSize),
      (int)std::floor(particles[i].pos.z / cellSize)
    };
    grid[key].push_back(i);
  }

  // ---- Find bonded pairs ----
  std::vector<Bond> bonds;
  const glm::vec3 xAxis(1.0f, 0.0f, 0.0f);

  for (int i = 0; i < (int)particles.size(); ++i) {
    const glm::vec3& pi = particles[i].pos;
    int cx = (int)std::floor(pi.x / cellSize);
    int cy = (int)std::floor(pi.y / cellSize);
    int cz = (int)std::floor(pi.z / cellSize);

    // Check all 27 neighbouring cells (including own cell)
    for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -1; dy <= 1; ++dy)
    for (int dz = -1; dz <= 1; ++dz) {
      auto it = grid.find({cx + dx, cy + dy, cz + dz});
      if (it == grid.end()) continue;

      for (int j : it->second) {
        if (j <= i) continue; // store each bond once

        glm::vec3 d    = particles[j].pos - pi;
        float     dist = glm::length(d);

        if (dist < threshold && dist > 1e-8f) {
          Bond b;
          b.i      = i;
          b.j      = j;
          b.l0     = dist;
          b.d0     = d / dist;
          b.broken = false;
          // q0 maps (1,0,0) -> d0; used in bend/twist energy
          b.q0 = rotationBetween(xAxis, b.d0);
          bonds.push_back(b);
        }
      }
    }
  }

  return bonds;
}

// ---------------------------------------------------------------------------
// tagBoundaryParticles
// ---------------------------------------------------------------------------

void tagBoundaryParticles(std::vector<Particle>& particles, const BeamConfig& cfg) {
  const float r            = cfg.r;
  const float searchRadius = 3.0f * r; // sphere around the ideal support/load point

  // Ideal positions for the three-point bending setup
  const glm::vec2 supp1XZ(0.10f * cfg.L, 0.5f * cfg.W); // left  support in XZ
  const glm::vec2 supp2XZ(0.90f * cfg.L, 0.5f * cfg.W); // right support in XZ
  const glm::vec2 loadXZ (0.50f * cfg.L, 0.5f * cfg.W); // load  point  in XZ

  for (auto& p : particles) {
    const glm::vec2 xz(p.pos.x, p.pos.z);

    // Support particles: bottom row AND within horizontal search radius
    if (p.pos.y < 2.0f * r) {
      if (glm::length(xz - supp1XZ) < searchRadius ||
          glm::length(xz - supp2XZ) < searchRadius) {
        p.isSupport = true;
        p.fixed     = true;
      }
    }

    // Load particles: top row AND within horizontal search radius
    if (p.pos.y > cfg.H - 2.0f * r) {
      if (glm::length(xz - loadXZ) < searchRadius) {
        p.isLoad = true;
      }
    }
  }
}
