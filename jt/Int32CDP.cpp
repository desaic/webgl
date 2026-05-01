#include "Int32CDP.h"
#include "BitReader.h"
#include <cstring>
#include <iostream>

namespace JT {

static int32_t ToInt32(const uint8_t* bytes) {
  return *((int32_t*)bytes);
}

static uint32_t ToUint32(const uint8_t* bytes) {
  return *((uint32_t*)bytes);
}

Int32CDP::DecodeResult Int32CDP::Decode(const uint8_t* buf, unsigned& offset, unsigned bufLen) {
  DecodeResult result;
  unsigned startOffset = offset;

  // Read I32: Value Count
  if (offset + 4 > bufLen) {
    std::cerr << "Int32CDP: Buffer too small for value count\n";
    return result;
  }
  int32_t valueCount = ToInt32(buf + offset);
  offset += 4;

  if (valueCount < 0 || valueCount > 100000000) {
    std::cerr << "Int32CDP: Invalid value count: " << valueCount << "\n";
    return result;
  }

  // Empty data - return success with empty vector
  if (valueCount == 0) {
    result.success = true;
    result.bytesRead = offset - startOffset;
    return result;
  }

  // Read U8: CODEC Type
  if (offset + 1 > bufLen) {
    std::cerr << "Int32CDP: Buffer too small for codec type\n";
    return result;
  }
  uint8_t codecByte = buf[offset];
  offset += 1;

  CodecType codec = static_cast<CodecType>(codecByte);

  // Handle Chopper pseudo-codec (it has different structure)
  if (codec == CodecType::Chopper) {
    result.success = DecodeChopper(buf, offset, bufLen, valueCount, result.values);
    result.bytesRead = offset - startOffset;
    return result;
  }

  // Read I32: CodeText Length (in bits)
  if (offset + 4 > bufLen) {
    std::cerr << "Int32CDP: Buffer too small for codetext length\n";
    return result;
  }
  int32_t codeTextLength = ToInt32(buf + offset);
  offset += 4;

  if (codeTextLength < 0) {
    std::cerr << "Int32CDP: Invalid codetext length: " << codeTextLength << "\n";
    return result;
  }

  // Dispatch to appropriate decoder based on codec type
  bool success = false;
  switch (codec) {
    case CodecType::Null:
      success = DecodeNull(buf, offset, bufLen, valueCount, result.values);
      break;

    case CodecType::Bitlength:
      success = DecodeBitlength(buf, offset, bufLen, valueCount, codeTextLength, result.values);
      break;

    case CodecType::Arithmetic:
      success = DecodeArithmetic(buf, offset, bufLen, valueCount, codeTextLength, result.values);
      break;

    default:
      std::cerr << "Int32CDP: Unsupported codec type: " << static_cast<int>(codecByte) << "\n";
      return result;
  }

  result.success = success;
  result.bytesRead = offset - startOffset;
  return result;
}

bool Int32CDP::DecodeNull(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                          int32_t valueCount, std::vector<int32_t>& output) {
  // Null codec: data is uncompressed, stored as VecU32
  // Number of U32 values needed to store valueCount int32 values
  int32_t u32Count = valueCount;

  if (offset + u32Count * 4 > bufLen) {
    std::cerr << "DecodeNull: Buffer too small for " << u32Count << " values\n";
    return false;
  }

  output.resize(valueCount);
  std::memcpy(output.data(), buf + offset, valueCount * sizeof(int32_t));
  offset += valueCount * 4;

  return true;
}

bool Int32CDP::DecodeBitlength(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                                int32_t valueCount, int32_t codeTextLength,
                                std::vector<int32_t>& output) {
  // Calculate number of U32 words needed for the codetext
  int32_t codeTextU32Count = (codeTextLength + 31) / 32;

  if (offset + codeTextU32Count * 4 > bufLen) {
    std::cerr << "DecodeBitlength: Buffer too small for codetext\n";
    return false;
  }

  std::cerr << "DecodeBitlength: valueCount=" << valueCount
            << " codeTextLength=" << codeTextLength << " bits"
            << " (" << codeTextU32Count << " U32 words)\n";
  std::cerr << "CodeText bytes: ";
  for (int i = 0; i < codeTextU32Count * 4 && i < 16; i++) {
    std::cerr << std::hex << (int)buf[offset + i] << " ";
  }
  std::cerr << std::dec << "\n";

  // Create bit reader for the codetext
  BitReader reader(buf + offset, codeTextU32Count * 4);
  offset += codeTextU32Count * 4;

  output.resize(valueCount);

  // Check the first bit to determine variable-width vs fixed-width encoding
  uint32_t encodingBit = reader.ReadBit();
  std::cerr << "First bit (encoding type): " << encodingBit
            << (encodingBit ? " (variable-width)" : " (fixed-width)") << "\n";

  if (encodingBit) {
    // Variable-width encoding
    return DecodeBitlengthVariable(reader, valueCount, output);
  } else {
    // Fixed-width encoding
    return DecodeBitlengthFixed(reader, valueCount, output);
  }
}

// Helper: Read variable-length nibble-encoded value (spec line 11732)
static uint32_t ReadNibbler(BitReader& reader) {
  uint32_t result = 0;
  uint32_t numNibbles = 0;
  const unsigned nibbleWidth = 4;

  uint32_t moreBits;
  do {
    uint32_t nibble = reader.ReadU32(nibbleWidth);
    nibble <<= (numNibbles * nibbleWidth);
    result |= nibble;
    moreBits = reader.ReadBit();
    numNibbles++;
  } while (moreBits);

  return result;
}

bool Int32CDP::DecodeBitlengthFixed(BitReader& reader, int32_t valueCount,
                                     std::vector<int32_t>& output) {
  // Read min and max VALUES directly using nibbler encoding (spec line 12271)
  int32_t minVal = static_cast<int32_t>(ReadNibbler(reader));
  int32_t maxVal = static_cast<int32_t>(ReadNibbler(reader));
  int32_t range = maxVal - minVal;

  std::cerr << "Fixed-width: minVal=" << minVal << " maxVal=" << maxVal << " range=" << range << "\n";

  if (range <= 0) {
    // All values are the same
    std::cerr << "All " << valueCount << " values are " << minVal << "\n";
    for (int32_t i = 0; i < valueCount; i++) {
      output[i] = minVal;
    }
    return true;
  }

  // Calculate field width needed for the range
  unsigned fieldWidth = 1;
  int32_t tempRange = range;
  while (tempRange >>= 1) {
    fieldWidth++;
  }

  std::cerr << "Decoding " << valueCount << " values with fieldWidth=" << fieldWidth << ":\n";
  for (int32_t i = 0; i < valueCount; i++) {
    output[i] = reader.ReadU32(fieldWidth) + minVal;
    if (i < 15) {
      std::cerr << "  [" << i << "] = " << output[i] << "\n";
    }
  }
  if (valueCount > 15) {
    std::cerr << "  ... (" << (valueCount - 15) << " more values)\n";
  }

  return true;
}

bool Int32CDP::DecodeBitlengthVariable(BitReader& reader, int32_t valueCount,
                                        std::vector<int32_t>& output) {
  // Read parameters
  int32_t mean = reader.ReadI32(32);
  unsigned chgWidthBits = reader.ReadU32(3);
  unsigned runLenBits = reader.ReadU32(3);

  int32_t maxDecr = -(1 << (chgWidthBits - 1));     // negative number
  int32_t maxIncr = (1 << (chgWidthBits - 1)) - 1;  // positive number

  unsigned fieldWidth = 0;
  int32_t outIdx = 0;

  while (outIdx < valueCount) {
    // Adjust field width
    int32_t changeWidth;
    do {
      changeWidth = reader.ReadI32(chgWidthBits);
      fieldWidth += changeWidth;
    } while (changeWidth == maxDecr || changeWidth == maxIncr);

    // Read run length
    uint32_t runLen = reader.ReadU32(runLenBits);

    // Decode the run
    if (fieldWidth > 0) {
      // Read data bits for the run
      for (uint32_t i = 0; i < runLen && outIdx < valueCount; i++, outIdx++) {
        output[outIdx] = reader.ReadI32(fieldWidth) + mean;
      }
    } else {
      // Use mean value for the whole run
      for (uint32_t i = 0; i < runLen && outIdx < valueCount; i++, outIdx++) {
        output[outIdx] = mean;
      }
    }
  }

  return true;
}

bool Int32CDP::DecodeArithmetic(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                                 int32_t valueCount, int32_t codeTextLength,
                                 std::vector<int32_t>& output) {
  // TODO: Implement Arithmetic codec decoder
  // This requires probability context parsing and arithmetic decoding
  std::cerr << "DecodeArithmetic: Not yet implemented\n";
  return false;
}

bool Int32CDP::DecodeChopper(const uint8_t* buf, unsigned& offset, unsigned bufLen,
                              int32_t valueCount, std::vector<int32_t>& output) {
  // Chopper pseudo-codec has different structure:
  // After codec type (4), read:
  // U8: Chop Bits
  if (offset + 1 > bufLen) {
    return false;
  }
  uint8_t chopBits = buf[offset];
  offset += 1;

  // If chopBits == 0, recursively decode
  if (chopBits == 0) {
    DecodeResult result = Decode(buf, offset, bufLen);
    if (result.success) {
      output = std::move(result.values);
    }
    return result.success;
  }

  // Read I32: Value Bias, U8: Value Span Bits
  if (offset + 5 > bufLen) {
    return false;
  }
  int32_t valueBias = ToInt32(buf + offset);
  offset += 4;
  uint8_t valueSpanBits = buf[offset];
  offset += 1;

  // Decode Chopped MSB Data (recursively)
  DecodeResult msbResult = Decode(buf, offset, bufLen);
  if (!msbResult.success || msbResult.values.size() != valueCount) {
    std::cerr << "DecodeChopper: Failed to decode MSB data\n";
    return false;
  }

  // Decode Chopped LSB Data (recursively)
  DecodeResult lsbResult = Decode(buf, offset, bufLen);
  if (!lsbResult.success || lsbResult.values.size() != valueCount) {
    std::cerr << "DecodeChopper: Failed to decode LSB data\n";
    return false;
  }

  // Reconstruct original values
  // OrigValue[i] = (LSBValue[i] | (MSBValue[i] << (ValSpanBits - ChopBits))) + ValueBias;
  output.resize(valueCount);
  int shiftAmount = valueSpanBits - chopBits;
  for (int32_t i = 0; i < valueCount; i++) {
    int32_t msb = msbResult.values[i];
    int32_t lsb = lsbResult.values[i];
    output[i] = (lsb | (msb << shiftAmount)) + valueBias;
  }

  return true;
}

} // namespace JT
