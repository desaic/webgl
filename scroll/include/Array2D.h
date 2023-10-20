#ifndef ARRAY_2D_H
#define ARRAY_2D_H

#include <vector>
#include <string>
#include "Vec2.h"

///a minimal class that interprets a 1D vector as
///a 2D array
template <typename T>
class Array2D
{
	public:
    Array2D(){
		size[0] = 0;
		size[1] = 0;
	}

    Array2D(unsigned width, unsigned height){
        Allocate(width, height);
    }

    void Allocate(unsigned width, unsigned height){
		size[0] = width;
		size[1] = height;
    data.resize(width*size_t(height));
	}

    void Allocate(unsigned width, unsigned height, const T& initVal){
        size[0] = width;
        size[1] = height;
        data.resize(width*size_t(height), initVal);
    }

    void Fill(const T& val){
        std::fill(data.begin(), data.end(), val);
    }
	///\param x column
	///\param y row
    T& operator()(unsigned int x, unsigned int y){
		return data[x+y*size[0]];
	}
	
    const T& operator()(unsigned int x, unsigned int y) const{
		return data[x+y*size[0]];
	}

    const Vec2u & GetSize() const { return size; }
    void SetSize(unsigned w, unsigned h){
        size[0] = w;
        size[1] = h;
    }
    std::vector<T> & GetData(){return data;}
    const std::vector<T> & GetData()const{return data;}
    T* DataPtr(){return data.data();}
    const T* DataPtr()const{return data.data();}

    bool Empty(){return data.size() == 0;}
private:
    Vec2u size;
    std::vector<T> data;
};

typedef Array2D<float> Array2Df;
typedef Array2D<unsigned char> Array2D8u;
typedef Array2D<unsigned short> Array2D16u;

#endif
