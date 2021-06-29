///@file iterate through pixels of a 2D line 
/// source:
/// https://github.com/erleben/OpenTissue/blob/master/OpenTissue/core/geometry/scan_conversion

#ifndef LINE_ITER_2D
#define LINE_ITER_2D

#include "Vec2.h"
#include <cmath>

class LineIter2D
{
protected:

// Is the iterator data valid
bool m_valid;

// An RigdeIterator might have more than one edge. We have hardwired this implementation to have atmost two edges.
bool      m_has_multiple_edges;
Vec2i m_next_vertex;

// Local variables concerned with the vertices
Vec2i m_start_vertex;
Vec2i m_end_vertex;

int m_start_x;
int m_start_y;

int m_end_x;
int m_end_y;

int  m_current_x;
int  m_current_y;

int  m_dx;
int  m_step_x;

int  m_dy;
int  m_step_y;

int  m_denominator;
int  m_accumulator;

public:

bool          valid() const { return this->m_valid;          }
int               x() const { return this->m_current_x;      }
int               y() const { return this->m_current_y;      }

public:

  LineIter2D()
  : m_valid(false)
{}

virtual ~LineIter2D(){}

public:

  LineIter2D(
  Vec2i const & v1
  , Vec2i const & v2
  )
  : m_has_multiple_edges(false)
{
  this->initialize(v1,v2);
}

  LineIter2D(
  Vec2i const & v1
  , Vec2i const & v2
  , Vec2i const & v3
  )
  : m_has_multiple_edges(true)
  , m_next_vertex(v3)
{
  this->initialize(v1,v2);
}

  LineIter2D(LineIter2D const& iter)
{  
  (*this) = iter; 
}

  LineIter2D const & operator=(LineIter2D const & iter)
{
  this->m_valid              = iter.m_valid;
  this->m_has_multiple_edges = iter.m_has_multiple_edges;
  this->m_next_vertex        = iter.m_next_vertex;
  this->m_start_vertex       = iter.m_start_vertex;
  this->m_end_vertex         = iter.m_end_vertex;
  this->m_start_x            = iter.m_start_x;
  this->m_start_y            = iter.m_start_y;
  this->m_end_x              = iter.m_end_x;
  this->m_end_y              = iter.m_end_y;
  this->m_current_x          = iter.m_current_x;
  this->m_current_y          = iter.m_current_y;
  this->m_dx                 = iter.m_dx;
  this->m_step_x             = iter.m_step_x;
  this->m_dy                 = iter.m_dy;
  this->m_step_y             = iter.m_step_y;
  this->m_denominator        = iter.m_denominator;
  this->m_accumulator        = iter.m_accumulator;
  return *this;
}

public:

bool operator()() const {  return this->m_valid; }

bool operator++()
{
  this->m_valid = this->update_vertex();
  return this->m_valid;
}

protected:

void initialize(Vec2i const & v1, Vec2i const & v2)
{
  this->m_valid = true;

  // We scan-convert from low y-values to high y-values
  // Interchange the end points if necessary.
  // That secures that Dy is always positive.
  if (v1[1] <= v2[1]) {
	  this->m_start_vertex   = v1;
	  this->m_end_vertex     = v2;
  }else{
	  this->m_start_vertex   = v2;
	  this->m_end_vertex     = v1;
  }
  this->m_end_vertex[1] ++;
  if (v1[1] == v2[1]) {
  	// Horizontal edge_type, throw it away, because we draw triangles/polygons - not edges.
	  // Horizontal Edges in triangles/polygons are thrown away, because the left and right 
	  // edges will handle Xstart and Xstop. See Foley.
	  this->m_valid = false;
  }

  this->m_start_x = int( this->m_start_vertex[0] );
  this->m_start_y = int( this->m_start_vertex[1] );
  this->m_end_x   = int( this->m_end_vertex[0]  );
  this->m_end_y   = int( this->m_end_vertex[1]  );

  this->m_dx     = (this->m_end_x >= this->m_start_x) ? (this->m_end_x - this->m_start_x) : (this->m_start_x - this->m_end_x);
  this->m_step_x = (this->m_end_x >= this->m_start_x) ? 1 :-1;

  this->m_dy     = this->m_end_y - this->m_start_y;
  this->m_step_y = 1;

  // Initialize the Accumulator (the numerator)
  // If it is an edge with a positive slope, let the the Accumulator be equal to Dx
  // that will make a right shift the first time, which must be done.
  // Else, initialize the Accumulator to +1, because if a and d are integers
  // then a >= d ==> a + 1 > d, and that is what is needed if we only have 
  // the the > operator
  this->m_accumulator = (0 <= this->m_step_x) ? this->m_dy : 1;
  this->m_denominator = this->m_dy;

  this->m_current_x = this->m_start_x;
  this->m_current_y = this->m_start_y;

  this->m_valid = (this->m_start_y == this->m_end_y) ? false : true;
}

bool update_vertex()
{
  if (this->m_current_y >= this->m_end_y) 
  {
	this->m_valid = false;
  }
  else 
  {
	this->m_accumulator += this->m_dx;
	while (this->m_accumulator > this->m_denominator) 
	{
	  this->m_current_x += this->m_step_x;
	  this->m_accumulator -= this->m_denominator;
	}
	this->m_current_y += this->m_step_y;
  }
  if (this->m_current_y >= this->m_end_y) 
  {
	if (!this->m_has_multiple_edges)
	  this->m_valid = false;
	else 
	{
	  this->initialize(
		this->m_end_vertex
		, this->m_next_vertex     
		);
	  this->m_has_multiple_edges = false;
	}
  }
  return this->m_valid;
}
};

#endif