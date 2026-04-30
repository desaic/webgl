#pragma once
#include <cstdint>
#include <vector>

namespace JT {

// Simple bit-level reader for decoding compressed data
class BitReader {
public:
  BitReader(const uint8_t* data, size_t dataLen)
    : data_(data), dataLen_(dataLen), bytePos_(0), bitPos_(0), currentByte_(0) {
    if (dataLen > 0) {
      currentByte_ = data[0];
    }
  }

  // Read a single bit (returns 0 or 1)
  // Little-endian: reads LSB first (bit 0, then bit 1, etc.)
  uint32_t ReadBit() {
    if (bytePos_ >= dataLen_) {
      return 0; // End of data
    }

    uint32_t bit = (currentByte_ >> bitPos_) & 1;
    bitPos_++;

    if (bitPos_ >= 8) {
      bitPos_ = 0;
      bytePos_++;
      if (bytePos_ < dataLen_) {
        currentByte_ = data_[bytePos_];
      }
    }

    return bit;
  }

  // Read N bits as unsigned integer (up to 32 bits)
  // Little-endian: first bit read becomes LSB of result
  uint32_t ReadU32(unsigned numBits) {
    if (numBits == 0 || numBits > 32) {
      return 0;
    }

    uint32_t result = 0;
    for (unsigned i = 0; i < numBits; i++) {
      result |= (ReadBit() << i);
    }
    return result;
  }

  // Read N bits as signed integer (up to 32 bits)
  int32_t ReadI32(unsigned numBits) {
    if (numBits == 0 || numBits > 32) {
      return 0;
    }

    uint32_t unsignedVal = ReadU32(numBits);

    // Sign extend if the high bit is set
    if (numBits < 32 && (unsignedVal & (1U << (numBits - 1)))) {
      // Set all higher bits to 1 for negative numbers
      unsignedVal |= (0xFFFFFFFFU << numBits);
    }

    return static_cast<int32_t>(unsignedVal);
  }

  // Read N bits as unsigned (returns 0 if numBits is 0)
  uint32_t ReadU32Or0(unsigned numBits) {
    return numBits ? ReadU32(numBits) : 0;
  }

  // Read N bits as signed (returns 0 if numBits is 0)
  int32_t ReadI32Or0(unsigned numBits) {
    return numBits ? ReadI32(numBits) : 0;
  }

  // Get current position in bits
  size_t GetBitPosition() const {
    return bytePos_ * 8 + bitPos_;
  }

  // Check if we have more data to read
  bool HasData() const {
    return bytePos_ < dataLen_;
  }

private:
  const uint8_t* data_;
  size_t dataLen_;
  size_t bytePos_;
  unsigned bitPos_;  // 0-7
  uint8_t currentByte_;
};

} // namespace JT
