#ifndef TET_SLICER
#define TET_SLICER

#include "Vec2.h"
#include "Vec3.h"
///computes intersection points between
/// a z plane and a tetrahedron.
/// returns no intersections if a face lies exactly on z plane.
class TetSlicer
{
public:

  typedef Vec3f  vector3_type;
  typedef float  real_type;

protected:

  vector3_type  m_p[4];  ///< Nodes of tetrahedron.

public:

	TetSlicer(
	vector3_type const & p0
	, vector3_type const & p1
	, vector3_type const & p2
	, vector3_type const & p3
	)
  {
	m_p[0] = p0;
	m_p[1] = p1;
	m_p[2] = p2;
	m_p[3] = p3;

	//--- Primitive bubble sort - ensures that tetrahedron vertices
	//--- are sorted according to z value.
	while(swap(0,1)||swap(1,2)||swap(2,3));
  };

protected:

  /**
  * Swap the order of two nodes if their z-value allows it.
  *
  * @param i   A node index.
  * @param j   A node index.
  *
  * @return   true if swapped otherwise false.
  */
  bool swap(int i,int j)
  {
	if(m_p[j][2]< m_p[i][2])
	{
	  vector3_type tmp = m_p[j];
	  m_p[j] = m_p[i];
	  m_p[i] = tmp;
	  return true;
	}
	return false;
  };

  /**
  * Computes intersection point of edge going from node i towards node j
  *
  * @param z   The z value, indicating the z-plane aginst which the edge is tested.
  * @param i   The node index of the starting node (got lowest z-value).
  * @param j   The node index of the ending node (got highest z-value).
  * @param p   Upon return this argument holds the intersection point.
  */
  void intersect(real_type const &  z, int i, int j, Vec2f & p) const
  {
	p = Vec2f(m_p[i][0], m_p[i][1]);
	Vec2f dir(m_p[j][0] - m_p[i][0], m_p[j][1] - m_p[i][1]);
	real_type s = (z-m_p[i][2])/(m_p[j][2]-m_p[i][2]);
	p += s * dir;
  };

public:

  /**
  * Compute Intersection Slice of Tetrahedron and Z-plane.
  *
  * @param z       The z-plane to slice against.
  * @param slice   Upon return holds the intersection points of the sliced tetrahedron.
  *
  * @return        The number of vertices in the sliced tetrahedron.
  */
  unsigned int intersect(real_type const & z, Vec2f slice[4]) const
  {
	if( z < m_p[0][2] || z >= m_p[3][2])
	  return 0;
	if( z<m_p[1][2])
	{
	  intersect(z,0,3, slice[0]);
	  intersect(z,0,1, slice[1]);
	  intersect(z,0,2, slice[2]);
	  return 3;
	}
	else if( z<m_p[2][2])
	{
	  intersect(z,0,3, slice[0]);
	  intersect(z,1,3, slice[1]);
	  intersect(z,1,2, slice[2]);
	  intersect(z,0,2, slice[3]);
	  return 4;
	}
	else
	{
	  intersect(z,0,3, slice[0]);
	  intersect(z,1,3, slice[1]);
	  intersect(z,2,3, slice[2]);
	  return 3;
	}
  }
};

#endif
