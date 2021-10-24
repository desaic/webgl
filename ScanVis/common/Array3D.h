#ifndef ARRAY_3D_H
#define ARRAY_3D_H

#include <vector>
#include <string>
#include "Vec3.h"

///a minimal class that interprets a 1D vector as
///a 3D array
template <typename T>
class Array3D
{
	public:
    Array3D(){
		size[0] = 0;
		size[1] = 0;
    size[2] = 0;
	}

    Array3D(unsigned sizex, unsigned sizey, unsigned sizez ){
        Allocate(sizex, sizey, sizez);
    }

  void Allocate(unsigned sizex, unsigned sizey, unsigned sizez){
		size[0] = sizex;
		size[1] = sizey;
    size[2] = sizez;
    data.resize(sizex *size_t(sizey) * size_t(sizez));
	}

    void Allocate(unsigned sizex, unsigned sizey, unsigned sizez, const T& initVal){
      size[0] = sizex;
      size[1] = sizey;
      size[2] = sizez;
      data.resize(sizex * size_t(sizey) * size_t(sizez), initVal);
    }

    void Fill(const T& val){
        std::fill(data.begin(), data.end(), val);
    }
	///\param x column
	///\param y row
  inline T& operator()(unsigned int x, unsigned int y, unsigned int z){
		return data[ x + y*size[0] + z*size[0] * size[1]];
	}
	
  inline const T& operator()(unsigned int x, unsigned int y, unsigned int z) const{
    return data[x + y * size[0] + z * size[0] * size[1]];
	}

  ///@param u texture coordinate between 0-1 float
  T sample(float u, float v, float w) const{
    unsigned x = u * size[0];
    unsigned y = v * size[1];
    unsigned z = w * size[2];
    x = std::min(x, size[0] - 1);
    y = std::min(y, size[1] - 1);
    z = std::min(z, size[2] - 1);
    return (*this)(x, y, z);
  }

  const Vec3u & GetSize() const { return size; }

  std::vector<T> & GetData(){return data;}
  const std::vector<T> & GetData()const{return data;}
  T* DataPtr(){return data.data();}
  const T* DataPtr()const{return data.data();}
  bool Empty(){return data.size() == 0;}

private:
    Vec3u size;
    std::vector<T> data;
};

typedef Array3D<float> Array3Df;
typedef Array3D<unsigned char> Array3D8u;
typedef Array3D<unsigned short> Array3D16u;

template <typename T>
void flipY(Array3D<T>& vol)
{
  Vec3u size = vol.GetSize();
  for (int x = 0; x < size[0]; x++) {
    for (int y = 0; y < size[1] / 2; y++) {
      for (int z = 0; z < size[2]; z++) {
        T tmp = vol(x, y, z);
        vol(x, y, z) = vol(x, size[1] - y - 1, z);
        vol(x, size[1] - y - 1, z) = tmp;
      }
    }
  }
}

#endif
