#pragma once
#include <cstdint>
#include <vector>

namespace JT {

// Bit-level reader using 32-bit word buffering (matches JT spec pseudo-code)
// Reads bits MSB first from 32-bit words
class BitReader {
public:
  BitReader(const uint8_t* data, size_t dataLen)
    : data_(data), dataLen_(dataLen), bytePos_(0), bitsLoaded_(0), bitsBuf_(0) {
    // Load first 32-bit word
    if (dataLen_ >= 4) {
      loadNextWord();
    }
  }

  // Read a single bit (returns 0 or 1)
  // Reads MSB first from the 32-bit buffer
  uint32_t ReadBit() {
    if (bitsLoaded_ == 0) {
      loadNextWord();
      if (bitsLoaded_ == 0) {
        return 0; // End of data
      }
    }

    uint32_t bit = bitsBuf_ >> 31;
    bitsBuf_ <<= 1;
    bitsLoaded_--;
    return bit;
  }

  // Read N bits as unsigned integer (up to 32 bits)
  // Matches spec: uOut = _uVal >> (32 - n)
  uint32_t ReadU32(unsigned numBits) {
    if (numBits == 0 || numBits > 32) {
      return 0;
    }

    uint32_t result;

    if (bitsLoaded_ >= numBits) {
      // Enough bits in buffer
      result = bitsBuf_ >> (32 - numBits);
      bitsBuf_ <<= numBits;
      bitsLoaded_ -= numBits;
    } else if (bitsLoaded_ == 0) {
      // Buffer empty, load new word
      loadNextWord();
      result = bitsBuf_ >> (32 - numBits);
      bitsBuf_ <<= numBits;
      bitsLoaded_ = (bitsLoaded_ >= numBits) ? (bitsLoaded_ - numBits) : 0;
    } else {
      // Need bits from current buffer + next word
      uint32_t headBits = bitsLoaded_;
      uint32_t tailBits = numBits - headBits;

      result = bitsBuf_ >> (32 - numBits);
      loadNextWord();
      result |= bitsBuf_ >> (32 - tailBits);
      bitsBuf_ <<= tailBits;
      bitsLoaded_ = (bitsLoaded_ >= tailBits) ? (bitsLoaded_ - tailBits) : 0;
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

  // Get current position in bits (approximate)
  size_t GetBitPosition() const {
    return bytePos_ * 8 - bitsLoaded_;
  }

  // Check if we have more data to read
  bool HasData() const {
    return bitsLoaded_ > 0 || bytePos_ < dataLen_;
  }

private:
  // Load next 32-bit word from data buffer
  void loadNextWord() {
    if (bytePos_ >= dataLen_) {
      bitsBuf_ = 0;
      bitsLoaded_ = 0;
      return;
    }

    // Read 4 bytes and interpret as uint32_t
    uint32_t word = 0;
    unsigned bytesToRead = (dataLen_ - bytePos_) >= 4 ? 4 : (dataLen_ - bytePos_);

    if (bytesToRead == 4) {
      // Read 4 bytes directly as uint32_t, then byte swap
      // We need bytes in sequential file order for MSB-first bit reading
      word = *((const uint32_t*)(data_ + bytePos_));
    } else {
      // Partial word - build manually in big-endian order
      for (unsigned i = 0; i < bytesToRead; i++) {
        word = (word << 8) | data_[bytePos_ + i];
      }
      // Shift to align MSB
      word <<= (4 - bytesToRead) * 8;
    }

    bitsBuf_ = word;
    bitsLoaded_ = bytesToRead * 8;
    bytePos_ += bytesToRead;
  }

  const uint8_t* data_;
  size_t dataLen_;
  size_t bytePos_;          // Current byte position in data
  uint32_t bitsLoaded_;     // Number of bits remaining in bitsBuf_
  uint32_t bitsBuf_;        // 32-bit buffer, bits extracted from MSB
};

} // namespace JT
