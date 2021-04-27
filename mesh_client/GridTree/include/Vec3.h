#ifndef VEC_3_H
#define VEC_3_H

#include <string>
#include <sstream>
#define _USE_MATH_DEFINES
#include <cmath>

template <typename T>
class Vec3{
public:
    Vec3(){
        v[0] = (T)0;
        v[1] = (T)0;
        v[2] = (T)0;
    }
    Vec3(T val) {
      v[0] = val;
      v[1] = val;
      v[2] = val;
    }
  Vec3(T v1, T v2, T v3){
	  v[0] = v1;
	  v[1] = v2;
	  v[2] = v3;
  }
  const T& operator[](unsigned i) const { return v[i]; }
  T & operator[](unsigned i){ return v[i]; }
  
  Vec3<T>& operator+= ( const Vec3<T> & v1 ){
	v[ 0 ] += v1[ 0 ];
	v[ 1 ] += v1[ 1 ];
	v[ 2 ] += v1[ 2 ];
	return *this;
  }

  Vec3<T>& operator-= (const Vec3<T>& v1) {
    v[0] -= v1[0];
    v[1] -= v1[1];
    v[2] -= v1[2];
    return *this;
  }

  Vec3<T>& operator*= (T v1) {
    v[0] *= v1;
    v[1] *= v1;
    v[2] *= v1;
    return *this;
  }

  Vec3<T>& operator/= (T v1) {
    T inv = (T)(1.0 / v1);
    v[0] *= inv;
    v[1] *= inv;
    v[2] *= inv;
    return *this;
  }

  Vec3<T> cross(const Vec3<T> & b){
	  Vec3<T> c(v[1] * b[2] - v[2] * b[1],
	           -v[0] * b[2] + v[2] * b[0],
              v[0] * b[1] - v[1] * b[0]);
	  return c;
  }
  
  T norm2() const{
	  return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  }
  
  T norm()const{
	return (T)(std::sqrt(float(norm2())));
  }
  
  void normalize(){
	T n = norm2();
	if(n>0){
		n = 1.0f/std::sqrt(n);
		v[0] *= n;
		v[1] *= n;
		v[2] *= n;
	}
  }
  
private:
  T v[3];
};

template <typename T>
std::string to_string(const Vec3<T> & v)
{
  std::stringstream ss;
  ss<<v[0]<<" "<<v[1]<<" "<<v[2];
  return ss.str();
}

template <typename T>
Vec3<T> operator+ (const Vec3<T>& v0, const Vec3<T>& v1)
{
  return Vec3<T>(v0[0] + v1[0], v0[1] + v1[1], v0[2] + v1[2]);
}

template <typename T>
Vec3<T> operator- (const Vec3<T>& v0, const Vec3<T>& v1)
{
  return Vec3<T>(v0[0] - v1[0], v0[1] - v1[1], v0[2] - v1[2]);
}

///scalar multiplication
template <typename T>
inline Vec3<T> operator* (T s, const Vec3<T>& v) {
	return Vec3<T>(v[0] * s, v[1] * s, v[2] * s);
}

template <typename T>
T Dot(const Vec3<T>& v0, const Vec3<T>& v1)
{
	return v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2];
}

typedef Vec3<float> Vec3f;
typedef Vec3<double> Vec3d;
typedef Vec3<unsigned> Vec3u;
typedef Vec3<unsigned char> Vec3uc;
typedef Vec3<int> Vec3i;
typedef Vec3<unsigned long long> Vec3ul;

#endif
