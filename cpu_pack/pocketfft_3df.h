#ifndef POCKETFFT_3DF_H
#define POCKETFFT_3DF_H

#include <complex>
#include <vector>
#include <cstdint>

namespace pocketfft {
  // x y z sizes.
  using shape_t = std::vector<size_t>;

  /// @brief specialized 3d forward fft for real float input  
  /// @param shape_in
  /// @param data_in
  /// @param data_out
  /// @param nthreads
  void r2c_3df(const shape_t &shape_in, const float *data_in, std::complex<float> *data_out, size_t nthreads = 1);

  // data_out is allocated such that complex size z is N/2 + 1. because fft of real signal is symmetric so only
  // need to store half of the complex coefficients. 
  void r2c_3d8u(const shape_t &shape_in, const uint8_t *data_in, std::complex<float> *data_out, size_t nthreads);

  template <typename INPUT_T>
  void r2c_3df_zaxis(const shape_t &shape_in, const INPUT_T *data_in, std::complex<float> *data_out, size_t nthreads);
  extern template void
  r2c_3df_zaxis<float>(const shape_t &shape_in, const float *data_in, std::complex<float> *data_out, size_t nthreads);
  extern template void r2c_3df_zaxis<uint8_t>(const shape_t &shape_in,
                                       const uint8_t *data_in,
                                       std::complex<float> *data_out,
                                       size_t nthreads);

  void c2c_3df_yaxis(const shape_t &shape_in,
                     const std::complex<float> *data_in,
                     std::complex<float> *data_out,
                     bool forward,
                     size_t nthreads = 1);
  void c2c_3df_xaxis(const shape_t &shape_in,
                     const std::complex<float> *data_in,
                     std::complex<float> *data_out,
                     bool forward,
                     size_t nthreads = 1);

  /// @brief specialized 3d backward fft for complex float input and real output  
  /// @param shapef shape of the real output
  /// @param data_in complex numbers where z axis only has (Zf/2+1).
  /// @param data_out real output
  /// @param nthreads
  void c2r_3df(const shape_t &shapef, const std::complex<float> *data_in, float *data_out, size_t nthreads = 1);

  void c2r_3df_zaxis(const shape_t &shapef, const std::complex<float> *data_in, float *data_out, size_t nthreads = 1);

} // namespace pocketfft

#endif
