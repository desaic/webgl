///@file pocket fft specialized for 3d float real input
#include "pocketfft_3df.h"

#include "pocketfft_hdronly.h"

namespace pocketfft {

void r2c_3df(const shape_t& shape_in, const float* data_in, std::complex<float>* data_out,
             size_t nthreads) {
  // check input dimension
  if (shape_in.size() != 3 || shape_in[0] == 0) return;
  r2c_3df_zaxis(shape_in, data_in, data_out, nthreads);

  shape_t shape_out(shape_in);
  shape_out[2] = shape_in[2] / 2 + 1;
  bool forward = true;
  c2c_3df_yaxis(shape_out, data_out, data_out, forward, nthreads);
  c2c_3df_xaxis(shape_out, data_out, data_out, forward, nthreads);
}

// casts uint8 to float first without allocating
void r2c_3d8u(const shape_t &shape_in, const uint8_t *data_in, std::complex<float> *data_out, size_t nthreads) {
  // check input dimension
  if (shape_in.size() != 3 || shape_in[0] == 0)
    return;
  r2c_3df_zaxis(shape_in, data_in, data_out, nthreads);

  shape_t shape_out(shape_in);
  shape_out[2] = shape_in[2] / 2 + 1;
  bool forward = true;
  c2c_3df_yaxis(shape_out, data_out, data_out, forward, nthreads);
  c2c_3df_xaxis(shape_out, data_out, data_out, forward, nthreads);
}

template <typename INPUT_T>
void r2c_3df_zaxis(const shape_t &shape_in, const INPUT_T *data_in, std::complex<float> *data_out,
                   size_t nthreads) {
  auto plan = detail::get_plan<detail::pocketfft_r<float>>(shape_in[2]);

  size_t yChunkSize = shape_in[1] / nthreads;
  size_t numChunks = shape_in[1] / yChunkSize + (shape_in[1] % yChunkSize > 0);  
  std::vector<std::thread> threads(numChunks);
  for (size_t i = 0; i < threads.size(); i++) {
    unsigned y0 = i * yChunkSize;
    unsigned y1 = y0 + yChunkSize;
    if (y1 > shape_in[1]) {
      y1 = shape_in[1];
    }
    threads[i] = std::thread([y0, y1, &shape_in, data_in, data_out, &plan] {
      std::vector<float> tmp(shape_in[2]);
      const size_t stridez = shape_in[0] * shape_in[1];
      const bool forward = true;
      for (size_t y = y0; y < y1; y++) {
        for (size_t x = 0; x < shape_in[0]; x++) {
          for (size_t z = 0; z < shape_in[2]; z++) {
            tmp[z] = float(data_in[z * stridez + y * shape_in[0] + x]);
          }
          plan->exec(tmp.data(), 1.0f, forward);
          data_out[y * shape_in[0] + x] = std::complex<float>(tmp[0], 0);
          for (size_t z = 1; z < shape_in[2] / 2; z++) {
            data_out[z * stridez + y * shape_in[0] + x] =
                std::complex<float>(tmp[2 * z - 1], tmp[2 * z]);
          }
          if (shape_in[2] % 2 == 0) {
            size_t z = shape_in[2] / 2;
            data_out[z * stridez + y * shape_in[0] + x] = std::complex<float>(tmp[2 * z - 1], 0);
          } else {
            size_t z = shape_in[2] / 2;
            data_out[z * stridez + y * shape_in[0] + x] =
                std::complex<float>(tmp[2 * z - 1], tmp[2 * z]);
          }
        }
      }
      });
  }
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
}

template void
r2c_3df_zaxis<float>(const shape_t &shape_in, const float *data_in, std::complex<float> *data_out, size_t nthreads);
template void
r2c_3df_zaxis<uint8_t>(const shape_t &shape_in, const uint8_t *data_in, std::complex<float> *data_out, size_t nthreads);

void c2c_3df_yaxis(const shape_t& shape_in, const std::complex<float>* data_in,
                   std::complex<float>* data_out, bool forward, size_t nthreads) {
  auto plan = detail::get_plan<detail::pocketfft_c<float>>(shape_in[1]);
  //divide work among threads along z
  size_t zChunkSize = shape_in[2] / nthreads;
  size_t numChunks = shape_in[2] / zChunkSize + (shape_in[2] % zChunkSize > 0);
  std::vector<std::thread> threads(numChunks);
  for (size_t i = 0; i < threads.size(); i++) {
    unsigned z0 = i * zChunkSize;
    unsigned z1 = z0 + zChunkSize;
    if (z1 > shape_in[2]) {
      z1 = shape_in[2];
    }
    threads[i] = std::thread([z0, z1, &shape_in, data_in, data_out, &plan, forward] {
      std::vector<std::complex<float> > tmp(shape_in[1]);
      const size_t stridez = shape_in[0] * shape_in[1];      
      for (size_t z = z0; z < z1; z++) {
        for (size_t x = 0; x < shape_in[0]; x++) {
          for (size_t y = 0; y < shape_in[1]; y++) {
            tmp[y] = data_in[z * stridez + y * shape_in[0] + x];
          }
          plan->exec<float>((detail::cmplx<float>*)tmp.data(), 1.0f, forward);
          for (size_t y = 0; y < shape_in[1]; y++) {
            data_out[z * stridez + y * shape_in[0] + x] = tmp[y];
          }
        }
      }
    });
  }
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
}

void c2c_3df_xaxis(const shape_t& shape_in, const std::complex<float>* data_in,
                   std::complex<float>* data_out, bool forward, size_t nthreads) {
  auto plan = detail::get_plan<detail::pocketfft_c<float>>(shape_in[0]);
  // divide work among threads along z
  size_t zChunkSize = shape_in[2] / nthreads;
  size_t numChunks = shape_in[2] / zChunkSize + (shape_in[2] % zChunkSize > 0);
  std::vector<std::thread> threads(numChunks);
  for (size_t i = 0; i < threads.size(); i++) {
    unsigned z0 = i * zChunkSize;
    unsigned z1 = z0 + zChunkSize;
    if (z1 > shape_in[2]) {
      z1 = shape_in[2];
    }
    threads[i] = std::thread([z0, z1, &shape_in, data_in, data_out, &plan, forward] {      
      const size_t stridez = shape_in[0] * shape_in[1];
      for (size_t z = z0; z < z1; z++) {
        for (size_t y = 0; y < shape_in[1]; y++) {
          plan->exec<float>((detail::cmplx<float>*)&(data_in[z * stridez + y * shape_in[0]]), 1.0f,
                            forward);
        }
      }
    });
  }
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
}

void c2r_3df(const shape_t& shapef, const std::complex<float>* data_in, float* data_out,
             size_t nthreads) {
  // check input dimension
  if (shapef.size() != 3 || shapef[0] == 0) return;
  shape_t shapec(shapef);
  shapec[2] = shapef[2] / 2 + 1;
  const bool backward = false;
  std::vector<std::complex<float>> tmp(shapec[0] * shapec[1] * shapec[2]);
  c2c_3df_yaxis(shapec, data_in, tmp.data(), backward, nthreads);
  c2c_3df_xaxis(shapec, tmp.data(), tmp.data(), backward, nthreads);

  c2r_3df_zaxis(shapef, tmp.data(), data_out, nthreads);
}

void c2r_3df_zaxis(const shape_t& shapef, const std::complex<float>* data_in, float* data_out,
                   size_t nthreads) {
  auto plan = detail::get_plan<detail::pocketfft_r<float>>(shapef[2]);

  size_t yChunkSize = shapef[1] / nthreads;
  size_t numChunks = shapef[1] / yChunkSize + (shapef[1] % yChunkSize > 0);
  std::vector<std::thread> threads(numChunks);
  for (size_t i = 0; i < threads.size(); i++) {
    unsigned y0 = i * yChunkSize;
    unsigned y1 = y0 + yChunkSize;
    if (y1 > shapef[1]) {
      y1 = shapef[1];
    }
    threads[i] = std::thread([y0, y1, &shapef, data_in, data_out, &plan] {
      std::vector<float> tmp(shapef[2]);
      const size_t stridez = shapef[0] * shapef[1];
      const bool backward = false;
      for (size_t y = y0; y < y1; y++) {
        for (size_t x = 0; x < shapef[0]; x++) {
          tmp[0] = data_in[y * shapef[0] + x].real();
          for (size_t z = 1; z < shapef[2]-1; z+=2) {
            tmp[z] = data_in[((z + 1) / 2) * stridez + y * shapef[0] + x].real();
            tmp[z + 1] = data_in[((z + 1) / 2) * stridez + y * shapef[0] + x].imag();
          }
          size_t z = shapef[2] - 1;
          if (shapef[2] % 2 == 0) {
            tmp[z] = data_in[(shapef[2] / 2) * stridez + y * shapef[0] + x].real();
          }
          plan->exec(tmp.data(), 1.0f, backward);          
          for (size_t z = 0; z < shapef[2]; z++) {
            data_out[z * stridez + y * shapef[0] + x] = tmp[z];
          }
        }
      }
    });
  }
  for (size_t i = 0; i < threads.size(); i++) {
    threads[i].join();
  }
}

}  // namespace pocketfft