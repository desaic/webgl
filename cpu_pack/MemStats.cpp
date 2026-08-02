#include "MemStats.h"

#include "AdapSDF.h"
#include "PackingScene.h"
#include "PointSample.h"

#include <cstdio>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>

#ifdef __linux__
#include <sys/resource.h>
#endif

static size_t ReadProcStatusKb(const char *key) {
  std::ifstream in("/proc/self/status");
  if (!in.good()) {
    return 0;
  }
  std::string line;
  size_t keyLen = std::string(key).size();
  while (std::getline(in, line)) {
    if (line.compare(0, keyLen, key) != 0) {
      continue;
    }
    std::istringstream iss(line.substr(keyLen));
    size_t kb = 0;
    if (iss >> kb) {
      return kb;
    }
    return 0;
  }
  return 0;
}

size_t RssBytes() {
  return ReadProcStatusKb("VmRSS:") * 1024;
}

size_t PeakRssBytes() {
  size_t hwm = ReadProcStatusKb("VmHWM:") * 1024;
  if (hwm > 0) {
    return hwm;
  }
#ifdef __linux__
  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) == 0) {
    return size_t(ru.ru_maxrss) * 1024;
  }
#endif
  return 0;
}

std::string FormatBytes(size_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB"};
  double val = double(bytes);
  int u = 0;
  while (val >= 1024.0 && u < 3) {
    val /= 1024.0;
    u++;
  }
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f %s", val, units[u]);
  return std::string(buf);
}

static size_t MeshBytes(const TrigMesh &m) {
  return m.v.capacity() * sizeof(float) + m.t.capacity() * sizeof(unsigned)
         + m.nv.capacity() * sizeof(float) + m.nt.capacity() * sizeof(float);
}

static size_t SdfBytes(const AdapSDF *sdf) {
  if (sdf == nullptr) {
    return 0;
  }
  size_t bytes = sdf->dist.GetData().size() * sizeof(short);
  bytes += sdf->fineGrid.capacity() * sizeof(FixedGrid3D<5>);
  return bytes;
}

size_t SceneMemory::Total() const {
  return bgVox + bgFft + containerSdf + containerGrids + itemMeshes + itemSdfs
         + itemSamples + instanceMeshCache + instanceGrids + trajectories
         + broadPhase;
}

std::string SceneMemory::toString() const {
  std::ostringstream oss;
  auto row = [&oss](const char *name, size_t bytes) {
    oss << "  " << std::left << std::setw(20) << name << std::right
        << std::setw(12) << FormatBytes(bytes) << "\n";
  };
  row("bg.vox", bgVox);
  row("bg.fft", bgFft);
  row("container.sdf", containerSdf);
  row("container.grids", containerGrids);
  row("item.meshes", itemMeshes);
  row("item.sdfs", itemSdfs);
  row("item.samples", itemSamples);
  oss << "  " << std::left << std::setw(20) << "instanceMeshCache"
      << std::right << std::setw(12) << FormatBytes(instanceMeshCache) << "  ("
      << instanceMeshCacheCount << " entries)\n";
  oss << "  " << std::left << std::setw(20) << "instanceGrids" << std::right
      << std::setw(12) << FormatBytes(instanceGrids) << "  ("
      << instanceGridsCount << " entries)\n";
  row("trajectories", trajectories);
  row("broadPhase", broadPhase);
  row("total", Total());
  return oss.str();
}

SceneMemory MeasureSceneMemory(const PackingScene &scene) {
  SceneMemory m;
  m.bgVox = scene.bg.vox.GetData().size();
  m.bgFft = scene.bg.fft.GetData().size() * sizeof(std::complex<float>);
  m.containerSdf = SdfBytes(scene.sdf.get());
  m.containerGrids =
      scene.containerGrid.MemoryBytes() + scene.containerInnerGrid.MemoryBytes();

  for (size_t i = 0; i < scene.items.size(); i++) {
    const MeshInfo &item = scene.items[i];
    m.itemMeshes += MeshBytes(item.mesh);
    m.itemSdfs += SdfBytes(item.sdf.get());
    m.itemSamples += item.samples.capacity() * sizeof(SamplePoint);
  }
  m.itemMeshes += MeshBytes(scene.container.mesh);
  m.itemMeshes += MeshBytes(scene.containerInner.mesh);

  for (const auto &kv : scene.instanceMeshCache) {
    if (kv.second) {
      m.instanceMeshCache += MeshBytes(*kv.second);
    }
  }
  m.instanceMeshCacheCount = unsigned(scene.instanceMeshCache.size());
  for (const auto &kv : scene.instanceGrids) {
    if (kv.second) {
      m.instanceGrids += kv.second->MemoryBytes();
    }
  }
  m.instanceGridsCount = unsigned(scene.instanceGrids.size());

  for (size_t i = 0; i < scene.instances.size(); i++) {
    m.trajectories +=
        scene.instances[i].trajectory.capacity() * sizeof(RigidTransform);
  }
  for (size_t i = 0; i < scene.placed.size(); i++) {
    m.trajectories += scene.placed[i].capacity() * sizeof(RigidTransform);
  }
  m.broadPhase = scene.broadPhase.MemoryBytes();
  return m;
}

void ReportMemory(std::ostream &out, const std::string &title,
                  const PackingScene &scene) {
  SceneMemory m = MeasureSceneMemory(scene);
  out << "--- " << title << " ---\n";
  out << m.toString();
  out << "  " << std::left << std::setw(20) << "process rss" << std::right
      << std::setw(12) << FormatBytes(RssBytes()) << "\n";
  out << "  " << std::left << std::setw(20) << "process peak rss"
      << std::right << std::setw(12) << FormatBytes(PeakRssBytes()) << "\n";
}
