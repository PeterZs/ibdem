#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// State of a single spherical discrete element.
struct Particle {
  glm::vec3 pos;     // position (m)
  glm::quat rot;     // orientation as unit quaternion  q = (w, x, y, z), identity = (1,0,0,0)
  glm::vec3 vel;     // translational velocity (m/s)
  glm::vec3 angVel;  // angular velocity in world frame (rad/s)
  float mass;        // mass (kg)
  float radius;      // sphere radius (m)

  // Boundary condition flags
  bool fixed;        // true  -> position & rotation are constrained (support point)
  bool isSupport;    // true  -> render as support (blue)
  bool isLoad;       // true  -> render as load application point (red)
};
