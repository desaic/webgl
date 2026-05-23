#include "Array3D.h"
#include "BBox.h"
#include "GridUtils.h"
#include <algorithm>

class BroadPhaseGrid {
  private:
    Box3f m_box;
    float m_dx = 0.1f;

    Array3D <std::vector<unsigned>> m_cells;

    // Modified to accept Box3d but compute in float precision
    void PosToCell(const Vec3f &pos, int &cx, int &cy, int &cz) const ;

  public:
    /**
     * @param worldBox Total low-res bounding arena volume.
     * @param dimX, dimY, dimZ Grid resolutions (e.g., 16x16x16 or 32x32x32).
     */
    BroadPhaseGrid(){}

    void Init(const Box3f &worldBox, float dx) {
      m_box = worldBox;
      m_dx = dx;
      m_box.vmin = AlignOriginToGrid(m_box.vmin, dx);
      Vec3f boxSize = m_box.vmax - m_box.vmin;
      Vec3u gridSize(unsigned(std::floor(boxSize[0] / dx)), unsigned(std::floor(boxSize[1] / dx)),
                     unsigned(std::floor(boxSize[2] / dx)));
      m_cells.Allocate(gridSize, {});      
    }

    Vec3f GetOrigin()const{
      return m_box.vmin;
    }
    float GetDx()const{
      return m_dx;
    }
    // Adds a finalized mesh instance into all grid cells overlapping its AABB bounds
    void Add(const Box3f &box, unsigned instId) ;

    /**
     * Queries all unique instances residing inside or near the query box expanded by maxDistance.
     *
     * @param queryBox    The current bounding box of the item being tested.
     * @param maxDistance The inflation cushion/search radius threshold.
     */
    std::vector<unsigned> GetNearby(const Box3f &queryBox, float maxDistance) const ;

    // Clears the entire grid structure to reset state between fresh packing trials
    void Clear() {
      for (auto &cell : m_cells.GetData()) {
        cell.clear();
      }
    }

    void SaveDebugMesh(const std::string & outFile) const;
};
