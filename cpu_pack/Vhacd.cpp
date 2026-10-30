#include "Vhacd.h"

#include "Log.h"
#include "MeshInfo.h"
#include "Stopwatch.h"

#define ENABLE_VHACD_IMPLEMENTATION 1
#include "vhacd/VHACD.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

namespace {

struct Hull {
  std::vector<VHACD::Vertex> v;
  std::vector<VHACD::Triangle> t;
};

void SaveHullFiles(const std::string &base, const std::vector<Hull> &hulls) {
  std::ofstream obj(base + ".obj");
  std::ofstream txt(base + ".hulls.txt");
  obj << std::setprecision(7);
  txt << std::setprecision(7);
  txt << "hulls " << hulls.size() << char(10);
  size_t offset = 1;
  for (size_t k = 0; k < hulls.size(); k++) {
    const Hull &h = hulls[k];
    obj << "o hull_" << k << char(10);
    txt << "hull " << h.v.size() << " " << h.t.size() << char(10);
    for (const VHACD::Vertex &p : h.v) {
      obj << "v " << p.mX << " " << p.mY << " " << p.mZ << char(10);
      txt << p.mX << " " << p.mY << " " << p.mZ << char(10);
    }
    for (const VHACD::Triangle &t : h.t) {
      obj << "f " << t.mI0 + offset << " " << t.mI1 + offset << " " << t.mI2 + offset << char(10);
      txt << t.mI0 << " " << t.mI1 << " " << t.mI2 << char(10);
    }
    offset += h.v.size();
  }
}

}  // namespace

bool RunVhacd(const PackingConfig &cfg) {
  const fs::path srcDir(cfg.MeshDir());
  const std::string outDir = cfg.OutputFolder();
  std::error_code ec;
  fs::create_directories(outDir, ec);
  if (!fs::is_directory(srcDir)) {
    LOGI("not a directory: " << srcDir.string() << char(10));
    return false;
  }
  VHACD::IVHACD::Parameters params;
  params.m_maxConvexHulls = cfg.vhacdMaxHulls;
  params.m_resolution = cfg.vhacdResolution;
  params.m_minimumVolumePercentErrorAllowed = cfg.vhacdErrorPct;
  params.m_maxRecursionDepth = cfg.vhacdDepth;
  params.m_maxNumVerticesPerCH = cfg.vhacdMaxVerts;
  params.m_asyncACD = false;
  if (cfg.vhacdFill == "raycast") params.m_fillMode = VHACD::FillMode::RAYCAST_FILL;
  else if (cfg.vhacdFill == "surface") params.m_fillMode = VHACD::FillMode::SURFACE_ONLY;
  else params.m_fillMode = VHACD::FillMode::FLOOD_FILL;
  LOGI("vhacd: max hulls " << params.m_maxConvexHulls << ", resolution " << params.m_resolution
                           << ", volume error " << params.m_minimumVolumePercentErrorAllowed
                           << " %, fill " << cfg.vhacdFill << ", depth " << params.m_maxRecursionDepth << ", max verts per hull "
                           << params.m_maxNumVerticesPerCH << char(10));

  std::vector<fs::path> files;
  for (const auto &entry : fs::directory_iterator(srcDir)) {
    if (!entry.is_regular_file()) continue;
    std::string ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    if (ext != ".obj" && ext != ".stl") continue;
    if (entry.path().stem().string().ends_with("_m")) continue;
    if (!cfg.vhacdOnly.empty() &&
        ("," + cfg.vhacdOnly + ",").find("," + entry.path().stem().string() + ",") == std::string::npos) continue;
    files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());

  VHACD::IVHACD *vhacd = VHACD::CreateVHACD();
  Utils::Stopwatch total;
  total.Start();
  unsigned failed = 0;
  for (const fs::path &p : files) {
    TrigMesh mesh;
    if (LoadMesh(mesh, p) != 0 || mesh.t.empty()) {
      LOGI(p.filename().string() << ": could not load" << char(10));
      failed++;
      continue;
    }
    Utils::Stopwatch clock;
    clock.Start();
    vhacd->Clean();
    if (!vhacd->Compute(mesh.v.data(), uint32_t(mesh.v.size() / 3), mesh.t.data(),
                        uint32_t(mesh.t.size() / 3), params)) {
      LOGI(p.filename().string() << ": compute failed" << char(10));
      failed++;
      continue;
    }
    std::vector<Hull> hulls;
    for (uint32_t k = 0; k < vhacd->GetNConvexHulls(); k++) {
      VHACD::IVHACD::ConvexHull ch;
      if (!vhacd->GetConvexHull(k, ch)) continue;
      hulls.push_back({ch.m_points, ch.m_triangles});
    }
    if (hulls.empty()) {
      LOGI(p.filename().string() << ": no hulls" << char(10));
      failed++;
      continue;
    }
    SaveHullFiles(outDir + p.stem().string(), hulls);
    size_t verts = 0;
    for (const Hull &h : hulls) verts += h.v.size();
    LOGI(std::left << std::setw(26) << p.filename().string() << " hulls " << hulls.size() << ", verts " << verts
                   << ", " << clock.ElapsedMS() << " ms" << char(10));
  }
  vhacd->Release();
  LOGI("vhacd: " << files.size() - failed << " of " << files.size() << " meshes in "
                 << total.ElapsedMS() / 1000.0 << " s, saved to " << outDir << char(10));
  return failed == 0;
}
