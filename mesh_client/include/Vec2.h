#ifndef VEC_2_H
#define VEC_2_H

#include <sstream>

template <typename T>
class Vec2{
public:
    Vec2(){
        v[0] = (T)0;
        v[1] = (T)0;
    }
  Vec2(T v1, T v2){
	  v[0] = v1;
	  v[1] = v2;
  }
  const T&operator[](unsigned i) const { return v[i]; }
  T & operator[](unsigned i){ return v[i]; }
private:
  T v[2];
};

template <typename T>
std::string to_string(const Vec2<T> & v)
{
  std::stringstream ss;
  ss<<v[0]<<" "<<v[1]<<" ";
  return ss.str();
}

typedef Vec2<float> Vec2f;
typedef Vec2<double> Vec2d;
typedef Vec2<unsigned> Vec2u;
typedef Vec2<unsigned long long> Vec2ul;

#endif
