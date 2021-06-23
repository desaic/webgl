#ifndef TRIG_ITER_2D
#define TRIG_ITER_2D
#include "Vec2.h"
#include "LineIter2D.h"

class TrigIter2D 
{
protected:
  Vec2f m_vertex[3];     ///< The 3D Transformed vertices

  int m_lower_left_idx;
  int m_upper_left_idx;
  int m_the_other_idx;

  LineIter2D m_left_ridge_iterator;
  LineIter2D m_right_ridge_iterator;

  // Types and variables for scan-convertion
  typedef enum {
	find_nonempty_scanline_state
	, in_scanline_state
	, scanline_found_state
	, finished_state
  } state_type;

  state_type m_state;

  int m_start_x;     // Screen coordinates
  int m_start_y;
  int m_end_x;
  int m_end_y;
  int m_x0, m_x1;
  int m_current_y;

protected:

  /**
  * returns the index of the vertex with the smallest y-coordinate
  * If there is a horizontal edge, the vertex with the smallest 
  * x-coordinate is chosen.
  */
  int find_lower_left_vertex_index()
  {
	int ll = 0;
	for (int i = ll + 1; i < 3; ++i) 
	{
	  if (
		(this->m_vertex[i][1] < this->m_vertex[ll][1]) 
		||
		(
		(this->m_vertex[i][1] == this->m_vertex[ll][1]) 
		&&
		(this->m_vertex[i][0] < this->m_vertex[ll][0])
		)
		)
	  {
		ll = i;
	  }
	}
	return ll;
  }

  /**
  * returns the index of the vertex with the greatest y-coordinate
  * If there is a horizontal edge, the vertex with the smallest 
  * x-coordinate is chosen.
  */
  int find_upper_left_vertex_index()
  {
	int ul = 0;
	for (int i = ul + 1; i < 3; ++i) 
	{
	  if (
		(this->m_vertex[i][1] > this->m_vertex[ul][1]) 
		||
		(
		(this->m_vertex[i][1] == this->m_vertex[ul][1]) 
		&& 
		(this->m_vertex[i][0] < this->m_vertex[ul][0])
		)
		)
	  {
		ul = i;
	  }
	}
	return ul;
  }

  LineIter2D get_left_ridge_iterator() const
  {
	typedef float real_type;

	// Let u be the vector from 'lowerleft' to 'upperleft', and let v be the vector
	// from 'lowerleft' to 'theother'. If the cross product u x v is positive then
	// the point 'theother' is to the left of u.
	Vec2f u = this->m_vertex[this->m_upper_left_idx] - this->m_vertex[this->m_lower_left_idx];
	Vec2f v = this->m_vertex[this->m_the_other_idx]  - this->m_vertex[this->m_lower_left_idx];

	real_type cross_product = u[0] * v[1] - u[1] * v[0];

	if (cross_product >= 0) 
	{
	  // Here there is no need to check for a horizontal edge, because it
	  // would be a top edge, and therefore it would not be drawn anyway.
	  // The point 'theother' is to the left of the vector u
	  return LineIter2D(this->m_vertex[this->m_lower_left_idx]
			, this->m_vertex[this->m_the_other_idx]
			, this->m_vertex[this->m_upper_left_idx]);
	}
	else 
	{
	  // The point 'theother' is to the right of the vector u
	  return LineIter2D(this->m_vertex[this->m_lower_left_idx], this->m_vertex[this->m_upper_left_idx]);
	}
  }
  
	LineIter2D get_right_ridge_iterator() const
  {
	typedef float real_type;

	// Let u be the vector from 'lowerleft' to 'upperleft', and let v be the vector
	// from 'lowerleft' to 'theother'. If the cross product u x v is negative then
	// the point 'theother' is to the right of u.

	Vec2f u = this->m_vertex[this->m_upper_left_idx] - this->m_vertex[this->m_lower_left_idx];
	Vec2f v = this->m_vertex[this->m_the_other_idx]  - this->m_vertex[this->m_lower_left_idx];

	real_type cross_product = u[0] * v[1] - u[1] * v[0];

	if (cross_product < 0) 
	{
	  // The point 'theother' is to the right of the vector u
	  // Check if there is a horizontal edge
	  if (this->m_vertex[this->m_lower_left_idx][1] != this->m_vertex[this->m_the_other_idx][1] ) 
	  {
		return LineIter2D(
		  this->m_vertex[this->m_lower_left_idx]
		, this->m_vertex[this->m_the_other_idx]
		, this->m_vertex[this->m_upper_left_idx]
		);
	  }
	  else 
	  {
		// Horizontal edge - ('lowerleft', 'theother') - throw it away
		return LineIter2D(
		  this->m_vertex[this->m_the_other_idx]
		, this->m_vertex[this->m_upper_left_idx]
		);
	  }
	}
	else 
	{
	  // The point 'theother' is to the left of the u vector
	  return LineIter2D(
		this->m_vertex[this->m_lower_left_idx]
	  , this->m_vertex[this->m_upper_left_idx]
	  );
	}
  }

  bool find_nonempty_scanline()
  {
	while (m_left_ridge_iterator()) 
	{
	  this->m_start_x   = this->m_left_ridge_iterator.x();
	  this->m_end_x    = this->m_right_ridge_iterator.x();

	  this->m_current_y = this->m_left_ridge_iterator.y();

	  if (this->m_start_x < this->m_end_x)
	  {
		this->m_state = in_scanline_state;
		return this->valid();
	  }
	  else 
	  {
		++(this->m_left_ridge_iterator);
		++(this->m_right_ridge_iterator);
		this->m_state = find_nonempty_scanline_state;
	  }
	}
	// We ran our of left_ridge_iterator...
	return this->valid();
  }

public:

  bool valid() const { return this->m_left_ridge_iterator.valid();  }
  int     x0() const { return this->m_start_x;  }
	int     x1() const { return this->m_end_x; }
  int      y() const { return this->m_current_y;  }

public:

  virtual ~TrigIter2D(){}

  /**
  * 
  * vertex coordinates are assumed to be transformed into screen
  * space of the camera. Normals can be given in whatever coordinate
  * frame one wants. Just remember that normals are lineary
  * interpolated in screen space.
  * 
  */
	TrigIter2D(
		Vec2f const & v1
	, Vec2f const & v2
	, Vec2f const & v3
	) 
  {
	this->initialize(v1,v2,v3);
  }

	TrigIter2D(TrigIter2D const & iter)  {    (*this) = iter;  }

	TrigIter2D const & operator=(TrigIter2D const & iter)
  {
	this->m_vertex[0] = iter.m_vertex[0];
	this->m_vertex[1] = iter.m_vertex[1];
	this->m_vertex[2] = iter.m_vertex[2];

	this->m_lower_left_idx = iter.m_lower_left_idx;
	this->m_upper_left_idx = iter.m_upper_left_idx;
	this->m_the_other_idx  = iter.m_the_other_idx;

	this->m_left_ridge_iterator  = iter.m_left_ridge_iterator;
	this->m_right_ridge_iterator = iter.m_right_ridge_iterator;

	this->m_state = iter.m_state;

	this->m_start_x   = iter.m_start_x;
	this->m_start_y   = iter.m_start_y;
	this->m_end_x    = iter.m_end_x;
	this->m_end_y    = iter.m_end_y;
	this->m_x0 = iter.m_x0;
	this->m_x1 = iter.m_x1;
	this->m_current_y = iter.m_current_y;

	return *this;
  }

  /**
  * 
  *
  * @return   If there are any more fragments left then the
  *           return value is true otherwise it is false.
  */
  bool operator()() const  {    return this->m_left_ridge_iterator.valid();  }

  /**
  * Advances the iterator to the next fragment. Initially the
  * fragement iterator points to the first fragment (if any).
  * 
  */
  bool operator++()
  {
	if (this->m_state == find_nonempty_scanline_state) 
	{
	  if (this->find_nonempty_scanline()) 
	  {
		this->m_state = scanline_found_state;
	  }
	}
	if (this->m_state == in_scanline_state) 
	{

		++(this->m_left_ridge_iterator);
		++(this->m_right_ridge_iterator);
		if (this->find_nonempty_scanline()) 
		{
		  this->m_state = in_scanline_state;
		}
	}
	if (this->m_state == scanline_found_state) 
	{
	  this->m_state = in_scanline_state;
	}
	return this->valid();
  }

public:

  /**
  * 
  * vertex coordinates are assumed to be transformed into screen
  * space of the camera. Normals can be given in whatever coordinate
  * frame one wants. Just remember that normals are lineary
  * interpolated in screen space.
  * 
  */
  void initialize(
		Vec2f const & v1
	, Vec2f const & v2
	, Vec2f const & v3
	)
  {
	this->m_vertex[0] = Vec2f(std::round(v1[0]), std::round(v1[1]));
	this->m_vertex[1] = Vec2f(std::round(v2[0]), std::round(v2[1]));
	this->m_vertex[2] = Vec2f(std::round(v3[0]), std::round(v3[1]));

	this->m_lower_left_idx = this->find_lower_left_vertex_index();
	this->m_upper_left_idx = this->find_upper_left_vertex_index();

	if(this->m_lower_left_idx == this->m_upper_left_idx)
	{
	  // We must have encountered a degenerate case! Either a point or horizontal line
	  int a = (this->m_lower_left_idx + 1) % 3;
	  int b = (a + 1) % 3;
	  this->m_upper_left_idx = (this->m_vertex[a][0] < this->m_vertex[b][0]) ? a : b;
	}

	this->m_the_other_idx  = 3 - (this->m_lower_left_idx + this->m_upper_left_idx);

	this->m_left_ridge_iterator  = this->get_left_ridge_iterator();
	this->m_right_ridge_iterator = this->get_right_ridge_iterator();

	this->m_state = find_nonempty_scanline_state;
	this->find_nonempty_scanline();
  }

};

#endif