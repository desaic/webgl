#define LZMA_API_STATIC
#include "lzma.h"
#include "lzma_wrapper.h"

#include <stdexcept>

int DecompressLZMA(const unsigned char* input, size_t input_len, std::vector<unsigned char>& output)
{
  lzma_stream strm = LZMA_STREAM_INIT;
  lzma_ret ret = lzma_stream_decoder(&strm, UINT64_MAX, 0);
  if (ret != LZMA_OK) {
    //failed to init lzma lib
    return -1;
  }
  
  strm.next_in = input;
  strm.avail_in = input_len;

  // temporary buffer for chunked decompression  
  std::vector<uint8_t> temp_buf(1u<<20);
  
  try {
    do {
      strm.next_out = temp_buf.data();
      strm.avail_out = temp_buf.size();

      // LZMA_FINISH: we have provided all input
      ret = lzma_code(&strm, LZMA_FINISH);

      // Calculate how much was actually written to temp_buf
      size_t decoded_this_iter = temp_buf.size() - strm.avail_out;
      output.insert(output.end(), temp_buf.begin(), temp_buf.begin() + decoded_this_iter);
      
      if (ret == LZMA_MEM_ERROR) {
        throw std::runtime_error("LZMA: Out of memory");
      }
      if (ret == LZMA_DATA_ERROR || ret == LZMA_FORMAT_ERROR) {
        throw std::runtime_error("LZMA: Corrupted or invalid data");
      }
      if (ret != LZMA_OK && ret != LZMA_STREAM_END) {
        throw std::runtime_error("LZMA: Internal error");
      }

    } while (ret != LZMA_STREAM_END);

  }
  catch (...) {
    
  }

  lzma_end(&strm);
  if (ret == LZMA_STREAM_END) {
    return 0;
  }
  return ret;
}