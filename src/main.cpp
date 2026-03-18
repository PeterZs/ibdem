// main.cpp
// Phase 1: static visualisation of three-point bending setup
// Three beam scales (Low / Middle / High) displayed side-by-side to
// reproduce the scale-consistency comparison shown in Fig. 5.

#ifdef __APPLE__
  #include <GLUT/glut.h>
  #include <OpenGL/gl.h>
  #include <OpenGL/glu.h>
#else
  #ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
  #endif
  #include <GL/glut.h>
  #include <GL/gl.h>
  #include <GL/glu.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <algorithm>

#include "Particle.h"
#include "Bond.h"
#include "HexPacking.h"

// ---------------------------------------------------------------------------
// Window / viewport constants
// ---------------------------------------------------------------------------
static const int NUM_SCALES = 3;
static const int VP_W       = 400; // width  of one viewport (pixels)
static const int VP_H       = 450; // height of one viewport (pixels)
static const int WIN_W      = VP_W * NUM_SCALES;
static const int WIN_H      = VP_H;

// ---------------------------------------------------------------------------
// Beam configurations (Table 2, BDEM rows)
// Same physical dimensions L x H x W = 1.0 x 0.15 x 0.12 m; only r varies.
// Expected particle counts: Low ~530, Middle ~1800, High ~18000
// ---------------------------------------------------------------------------
static const BeamConfig SCALE_CFG[NUM_SCALES] = {
  // label    L      H      W      r       density  E      nu   tauC
  { 1.0f, 0.15f, 0.12f, 0.018f, 1000.0f, 1e7f, 0.3f, 3e4f, "Low"    },
  { 1.0f, 0.15f, 0.12f, 0.011f, 1000.0f, 1e7f, 0.3f, 3e4f, "Middle" },
  { 1.0f, 0.15f, 0.12f, 0.005f, 1000.0f, 1e7f, 0.3f, 3e4f, "High"   },
};

// ---------------------------------------------------------------------------
// Per-scale simulation data
// ---------------------------------------------------------------------------
struct ScaleData {
  BeamConfig            cfg;
  std::vector<Particle> particles;
  std::vector<Bond>     bonds;
};

static ScaleData g_scales[NUM_SCALES];

// ---------------------------------------------------------------------------
// Helper: draw a null-terminated C string at the current raster position
// ---------------------------------------------------------------------------
static void drawString(const char* s) {
  for (; *s != '\0'; ++s)
    glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *s);
}

// ---------------------------------------------------------------------------
// Render one beam scale into its viewport
// ---------------------------------------------------------------------------
static void drawScale(int index) {
  const ScaleData&  sd  = g_scales[index];
  const BeamConfig& cfg = sd.cfg;

  // -- Viewport (left edge depends on column index) --
  glViewport(index * VP_W, 0, VP_W, VP_H);

  // -- Orthographic projection that fits the beam with 5 % margin --
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  float xMargin = cfg.L * 0.05f;
  float xMin    = -xMargin;
  float xMax    =  cfg.L + xMargin;
  float xRange  = xMax - xMin;

  // Preserve aspect ratio: compute yRange from viewport aspect
  float aspect = (float)VP_H / (float)VP_W;
  float yRange  = xRange * aspect;
  float yCentre = cfg.H * 0.5f;

  glOrtho(xMin, xMax,
          yCentre - yRange * 0.5f, yCentre + yRange * 0.5f,
          -1.0f,  1.0f);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Pixels per world unit (used to scale point / line sizes)
  float worldToPixel = (float)VP_W / xRange;

  // -- Background (per-viewport clear via scissor) --
  glEnable(GL_SCISSOR_TEST);
  glScissor(index * VP_W, 0, VP_W, VP_H);
  glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);

  // -- Draw bonds as thin lines (XY projection) --
  {
    float lw = std::max(0.5f, cfg.r * worldToPixel * 0.3f);
    glLineWidth(lw);
    glColor3f(0.45f, 0.50f, 0.55f);
    glBegin(GL_LINES);
    for (const auto& b : sd.bonds) {
      if (b.broken) continue;
      const glm::vec3& pi = sd.particles[b.i].pos;
      const glm::vec3& pj = sd.particles[b.j].pos;
      glVertex2f(pi.x, pi.y);
      glVertex2f(pj.x, pj.y);
    }
    glEnd();
  }

  // -- Draw particles as points (GL_POINTS, size proportional to radius) --
  float ptSize = std::max(1.5f, 2.0f * cfg.r * worldToPixel);
  glPointSize(ptSize);
  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Regular particles (gray)
  glBegin(GL_POINTS);
  glColor4f(0.70f, 0.72f, 0.75f, 0.90f);
  for (const auto& p : sd.particles) {
    if (!p.isSupport && !p.isLoad)
      glVertex2f(p.pos.x, p.pos.y);
  }
  glEnd();

  // Support particles (blue)
  glPointSize(std::max(3.0f, ptSize * 1.4f));
  glBegin(GL_POINTS);
  glColor4f(0.25f, 0.50f, 0.95f, 1.00f);
  for (const auto& p : sd.particles) {
    if (p.isSupport)
      glVertex2f(p.pos.x, p.pos.y);
  }
  glEnd();

  // Load particles (red)
  glBegin(GL_POINTS);
  glColor4f(0.95f, 0.25f, 0.20f, 1.00f);
  for (const auto& p : sd.particles) {
    if (p.isLoad)
      glVertex2f(p.pos.x, p.pos.y);
  }
  glEnd();

  glDisable(GL_BLEND);
  glDisable(GL_POINT_SMOOTH);
  glPointSize(1.0f);

  // -- Labels (scale name + particle count) --
  glColor3f(0.95f, 0.95f, 0.95f);
  float labelX = xMin + xRange * 0.02f;
  float labelY = yCentre + yRange * 0.46f;
  glRasterPos2f(labelX, labelY);

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s  (%d particles, %d bonds)",
    cfg.label,
    (int)sd.particles.size(),
    (int)sd.bonds.size());
  drawString(buf);

  // -- Legend (small coloured squares + text) --
  float legX  = xMin + xRange * 0.02f;
  float legY0 = yCentre - yRange * 0.42f;
  float legDy = yRange  * 0.04f;

  glPointSize(8.0f);

  glBegin(GL_POINTS);
  glColor3f(0.70f, 0.72f, 0.75f);
  glVertex2f(legX, legY0);
  glEnd();
  glColor3f(0.90f, 0.90f, 0.90f);
  glRasterPos2f(legX + xRange * 0.025f, legY0 - legDy * 0.1f);
  drawString("particle");

  glBegin(GL_POINTS);
  glColor3f(0.25f, 0.50f, 0.95f);
  glVertex2f(legX, legY0 + legDy);
  glEnd();
  glColor3f(0.90f, 0.90f, 0.90f);
  glRasterPos2f(legX + xRange * 0.025f, legY0 + legDy * 0.9f);
  drawString("support (fixed)");

  glBegin(GL_POINTS);
  glColor3f(0.95f, 0.25f, 0.20f);
  glVertex2f(legX, legY0 + 2.0f * legDy);
  glEnd();
  glColor3f(0.90f, 0.90f, 0.90f);
  glRasterPos2f(legX + xRange * 0.025f, legY0 + 1.9f * legDy);
  drawString("load point");

  glPointSize(1.0f);
}

// ---------------------------------------------------------------------------
// GLUT callbacks
// ---------------------------------------------------------------------------

static void display() {
  // Clear the whole window first
  glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  // Draw each scale
  for (int i = 0; i < NUM_SCALES; ++i)
    drawScale(i);

  // -- Full-window overlay: separators + title --
  glViewport(0, 0, WIN_W, WIN_H);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0.0, WIN_W, 0.0, WIN_H, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Vertical separator lines between viewports
  glColor3f(0.30f, 0.30f, 0.35f);
  glLineWidth(2.0f);
  glBegin(GL_LINES);
  for (int i = 1; i < NUM_SCALES; ++i) {
    glVertex2f((float)(i * VP_W), 0.0f);
    glVertex2f((float)(i * VP_W), (float)WIN_H);
  }
  glEnd();
  glLineWidth(1.0f);

  // Window title text
  glColor3f(1.0f, 1.0f, 1.0f);
  glRasterPos2f(WIN_W * 0.5f - 200.0f, WIN_H - 18.0f);
  drawString("Implicit BDEM -- Three-Point Bending Scale Consistency (Fig. 5)");

  glutSwapBuffers();
}

static void reshape(int w, int h) {
  // Viewport setup is done per-panel in drawScale(); nothing to do here.
  (void)w; (void)h;
}

static void keyboard(unsigned char key, int /*x*/, int /*y*/) {
  if (key == 27 || key == 'q' || key == 'Q') // ESC or Q: quit
    std::exit(0);
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------

static void initScales() {
  for (int i = 0; i < NUM_SCALES; ++i) {
    ScaleData& sd = g_scales[i];
    sd.cfg = SCALE_CFG[i];

    std::printf("Generating scale %d (%s) r=%.4f ...\n",
      i, sd.cfg.label, sd.cfg.r);

    sd.particles = generateBeam(sd.cfg);
    tagBoundaryParticles(sd.particles, sd.cfg);
    sd.bonds     = generateBonds(sd.particles);

    std::printf("  particles: %d   bonds: %d\n",
      (int)sd.particles.size(), (int)sd.bonds.size());
  }
  std::printf("Initialisation complete.\n");
  std::printf("Press ESC or Q to quit.\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
  glutInitWindowSize(WIN_W, WIN_H);
  glutInitWindowPosition(100, 100);
  glutCreateWindow("Implicit BDEM -- Fig. 5 Scale Consistency");

  glutDisplayFunc(display);
  glutReshapeFunc(reshape);
  glutKeyboardFunc(keyboard);

  initScales();

  glutMainLoop();
  return 0;
}
