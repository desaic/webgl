#ifndef VEC_2_H
#define VEC_2_H

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
  Vec2<T>& operator+= (const Vec2<T>& v1) {
    v[0] += v1[0];
    v[1] += v1[1];
    return *this;
  }
private:
  T v[2];
};

typedef Vec2<float> Vec2f;
typedef Vec2<double> Vec2d;
typedef Vec2<int> Vec2i;
typedef Vec2<unsigned> Vec2u;
typedef Vec2<unsigned long long> Vec2ul;

///scalar multiplication
template <typename T>
inline Vec2<T> operator* (T s, const Vec2<T>& v) {
  return Vec2<T>(v[0] * s, v[1] * s);
}

template <typename T>
Vec2<T> operator+ (const Vec2<T>& v0, const Vec2<T>& v1)
{
  return Vec2<T>(v0[0] + v1[0], v0[1] + v1[1]);
}

template <typename T>
Vec2<T> operator- (const Vec2<T>& v0, const Vec2<T>& v1)
{
  return Vec2<T>(v0[0] - v1[0], v0[1] - v1[1]);
}

#endif
