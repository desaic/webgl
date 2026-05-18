#pragma once

#include "BBox.h"
#include "TrigMesh.h"
#include "MeshConvo.h"
#include "MeshInfo.h"
#include "TrigGrid.h"

#include <string>
#include <iomanip>
#include <filesystem>

class AdapSDF;

class PackingScene {
  public:
    MeshInfo container;
    MeshInfo containerInner;
    std::vector<MeshInfo> items;
    // for each item, list of transformations
    std::vector<std::vector<Transformation> > placed;
    std::vector<int> sortedBySize;
    std::shared_ptr<AdapSDF> sdf;
    std::string outputFolder;
    float dx = 1.0f;
    MeshConvo bg;
};


void LoadPack(PackingScene & scene, const std::string & packFile);

void AddInnerContainer(PackingScene & scene);

/// @brief 
/// @param scene 
/// @param i item type index
void SavePackedMesh(const PackingScene &scene, unsigned i);

int LoadMesh(TrigMesh &m, const std::filesystem::path & p) ;