#include <complex>
#include <cmath>
#include <vector>
#include <iostream>
#include "pocketfft_hdronly.h"

using namespace std;
using namespace pocketfft;

template<typename T> void make_test_vec(vector<complex<T>> &v)
  {
  for (size_t i = 0; i < v.size(); i++) {
    float a = i / float(v.size());
    a = 0.25f - (0.5f - a) * (0.5f - a);
    v[i] = complex<T>(a,a*2 );
  }
  }

template<typename T1, typename T2> long double l2err
  (const vector<T1> &v1, const vector<T2> &v2)
  {
  long double sum1=0, sum2=0;
  for (size_t i=0; i<v1.size(); ++i)
    {
    long double dr = v1[i].real()-v2[i].real(),
                di = v1[i].imag()-v2[i].imag();
    long double t1 = sqrt(dr*dr+di*di), t2 = abs(v1[i]);
    sum1 += t1*t1;
    sum2 += t2*t2;
    }
  return sqrt(sum1/sum2);
  }

template<typename T1, typename T2> float maxDiff
(const vector<T1>& v1, const vector<T2>& v2)
{
  long double maxDiff = 0;
  size_t maxDiffIndex = 0;
  for (size_t i = 1; i < v1.size()-1; ++i)
  {
    long double dr = v1[i].real() - v2[i].real(),
      di = v1[i].imag() - v2[i].imag();
    float m = std::max(std::abs(dr), std::abs(di));
    if (m > maxDiff) {
      maxDiff = m;
      maxDiffIndex = i;
    }
  }
  std::cout << maxDiffIndex << "\n";
  std::cout << v1[maxDiffIndex].real() << "\n";
  return maxDiff;
}


int main()
  {  
  shape_t shape{ 512,256,256 };
    stride_t stridef(shape.size()), strided(shape.size());
    size_t tmpf=sizeof(complex<float>),
           tmpd=sizeof(complex<double>);
    for (int i=shape.size()-1; i>=0; --i)
      {
      stridef[i]=tmpf;
      tmpf*=shape[i];
      strided[i]=tmpd;
      tmpd*=shape[i];
      }
    size_t ndata=1;
    for (size_t i=0; i<shape.size(); ++i)
      ndata*=shape[i];

    vector<complex<float>> dataf(ndata);
    vector<complex<double>> datad(ndata);
    make_test_vec(dataf);
    for (size_t i=0; i<ndata; ++i)
      {
      datad[i] = dataf[i];
      }
    shape_t axes;
    for (size_t i=0; i<shape.size(); ++i)
      axes.push_back(i);
    auto resd = datad;
    auto resf = dataf;
    std::cout << "fft double\n";
    c2c(shape, strided, strided, axes, FORWARD,
        datad.data(), resd.data(), 1.);
    std::cout << "fft float\n";
    c2c(shape, stridef, stridef, axes, FORWARD,
        dataf.data(), resf.data(), 1.f);
    float maxd = maxDiff(resd, resf);
//    c2c(shape, stridel, stridel, axes, POCKETFFT_BACKWARD,
//        resl.data(), resl.data(), 1.L/ndata);
    cout << l2err(resd, resf) << endl;
    cout << "max diff " << maxd << "\n";
  }
