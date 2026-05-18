#ifndef POCKETFFT_3DF_H
#define POCKETFFT_3DF_H
#include <complex>
#include <vector>
namespace pocketfft {
using shape_t = std::vector<size_t>;
/// @brief specialized 3d forward fft for real float input
/// @tparam T
/// @param shape_in
/// @param data_in
/// @param data_out
/// @param nthreads
void r2c_3df(const shape_t& shape_in, const float* data_in, std::complex<float>* data_out,
             size_t nthreads = 1);
void r2c_3df_zaxis(const shape_t& shape_in, const float* data_in, std::complex<float>* data_out,
                   size_t nthreads = 1);

void c2c_3df_yaxis(const shape_t& shape_in, const std::complex<float>* data_in,
                   std::complex<float>* data_out, bool forward, size_t nthreads = 1);
void c2c_3df_xaxis(const shape_t& shape_in, const std::complex<float>* data_in,
                   std::complex<float>* data_out, bool forward, size_t nthreads = 1);

/// @brief specialized 3d backward fft for complex float input and real output
/// @tparam T
/// @param shapef shape of the real output 
/// @param data_in complex numbers where z axis only has (Zf/2+1).
/// @param data_out real output
/// @param nthreads
void c2r_3df(const shape_t& shapef, const std::complex<float>* data_in, float* data_out,
             size_t nthreads = 1);

void c2r_3df_zaxis(const shape_t& shapef, const std::complex<float>* data_in, float* data_out,
                   size_t nthreads = 1);

}  // namespace pocketfft
#endif