#pragma once

#include "Vec3.h"

#include <iostream>
#include <string>
#include <sstream>
#include <vector>

struct PackingStep {

    std::vector<std::string> names;

    Vec3f force;

    unsigned count = 0;
    // pack towards inside or outside of container.
    bool outwards = true;
    // prevent packing at center of container
    bool useInnerContainer = false;

    PackingStep() : force(-1, 0, 0) {}
    
    std::string toString() const;
    
    void Load(std::istream &in);
};

struct PackingPlan{
  std::vector<PackingStep> steps;
  // lists of meshes grouped by sizes.
  std::vector< std::vector<std::string> > groups;

  void Save(std::ostream & out) const;

  void Load(std::istream & in);
};
