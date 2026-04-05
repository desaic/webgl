#include <vector>
#include <cstddef>

//@return 0 on success. error code on error.
int DecompressLZMA(const unsigned char * input, size_t input_len, std::vector<unsigned char>&output);