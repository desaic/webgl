#pragma once
#include <cstdint>
#include <vector>

namespace JT {

class BitReader; // Forward declaration

// Simplified Int32 Compressed Data Packet (Int32CDP) decoder
// Based on JT spec section 12.1.1 and Figure 132
class Int32CDP {
public:
  // Codec types from Table 64
  enum class CodecType : uint8_t {
    Null = 0,
    Bitlength = 1,
    Illegal = 2,
    Arithmetic = 3,
    Chopper = 4,
    MoveToFront = 5
  };

  struct DecodeResult {
    bool success = false;
    std::vector<int32_t> values;
    unsigned bytesRead = 0;
  };

  // Parse and decode Int32CDP from buffer
  // offset: starting position in buffer (will be updated)
  // bufLen: total buffer length
  static DecodeResult Decode(const uint8_t* buf, unsigned& offset, unsigned bufLen);

private:
  // Decode Null codec (uncompressed data)
  static bool DecodeNull(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                         int32_t valueCount, std::vector<int32_t>& output);

  // Decode Bitlength codec
  static bool DecodeBitlength(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                              int32_t valueCount, int32_t codeTextLength,
                              std::vector<int32_t>& output);

  // Decode Arithmetic codec
  static bool DecodeArithmetic(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                               int32_t valueCount, int32_t codeTextLength,
                               std::vector<int32_t>& output);

  // Decode Chopper pseudo-codec
  static bool DecodeChopper(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                            int32_t valueCount, std::vector<int32_t>& output);

  // Helper functions for Bitlength codec
  static bool DecodeBitlengthFixed(BitReader& reader, int32_t valueCount,
                                   std::vector<int32_t>& output);
  static bool DecodeBitlengthVariable(BitReader& reader, int32_t valueCount,
                                      std::vector<int32_t>& output);
};

} // namespace JT
