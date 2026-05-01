#include "BitReader.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace JT;

void TestReadBit() {
  // File bytes: 0xB3 = 0b10110011
  // Partial word (< 4 bytes): loaded as 0xB3000000 (big-endian manually)
  // Bits extracted MSB first: 1,0,1,1,0,0,1,1
  uint8_t data[] = {0xB3};
  BitReader reader(data, 1);

  assert(reader.ReadBit() == 1); // bit 7 (MSB)
  assert(reader.ReadBit() == 0); // bit 6
  assert(reader.ReadBit() == 1); // bit 5
  assert(reader.ReadBit() == 1); // bit 4
  assert(reader.ReadBit() == 0); // bit 3
  assert(reader.ReadBit() == 0); // bit 2
  assert(reader.ReadBit() == 1); // bit 1
  assert(reader.ReadBit() == 1); // bit 0 (LSB)

  std::cout << "TestReadBit: PASSED\n";
}

void TestReadU32() {
  // File bytes: 0xD6, 0x4F
  // Partial word: loaded as 0xD64F0000 (big-endian manually)
  // Bits: 11010110 01001111 00000000 00000000
  uint8_t data[] = {0xD6, 0x4F};
  BitReader reader(data, 2);

  // Read 4 bits: 0b1101 = 13
  assert(reader.ReadU32(4) == 13);

  // Read 5 bits: 0b01100 = 12
  assert(reader.ReadU32(5) == 12);

  // Read 7 bits: 0b1001111 = 79
  assert(reader.ReadU32(7) == 79);

  std::cout << "TestReadU32: PASSED\n";
}

void TestReadI32SignExtension() {
  // File bytes: 0xF0
  // Partial word: 0xF0000000
  // Bits: 11110000 00000000 00000000 00000000
  uint8_t data[] = {0xF0};
  BitReader reader(data, 1);

  // Read 4 bits: 0b1111 = -1 (with sign extension)
  int32_t val1 = reader.ReadI32(4);
  assert(val1 == -1);

  // Read 4 bits: 0b0000 = 0
  int32_t val2 = reader.ReadI32(4);
  assert(val2 == 0);

  std::cout << "TestReadI32SignExtension: PASSED\n";
}

void TestReadI32Positive() {
  // File bytes: 0x5A = 0b01011010
  // Partial word: 0x5A000000
  uint8_t data[] = {0x5A};
  BitReader reader(data, 1);

  // Read 4 bits: 0b0101 = 5 (positive, no sign extension)
  int32_t val = reader.ReadI32(4);
  assert(val == 5);

  std::cout << "TestReadI32Positive: PASSED\n";
}

void TestReadAcrossByteBoundary() {
  // File bytes: 0xCA, 0xF0
  // Partial word: 0xCAF00000
  // Bits: 11001010 11110000 00000000 00000000
  uint8_t data[] = {0xCA, 0xF0};
  BitReader reader(data, 2);

  // Read 6 bits: 0b110010 = 50
  assert(reader.ReadU32(6) == 50);

  // Read 10 bits: 0b1011110000 = 752
  assert(reader.ReadU32(10) == 752);

  std::cout << "TestReadAcrossByteBoundary: PASSED\n";
}

void TestReadAcrossWordBoundary() {
  // File bytes: 0xFF, 0xFF, 0xFF, 0xFF, 0xAA
  // First word (4 bytes): dereference → 0xFFFFFFFF (little-endian)
  //                       byte swap  → 0xFFFFFFFF (same)
  // Second word (1 byte): partial → 0xAA000000
  uint8_t data[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xAA};
  BitReader reader(data, 5);

  // Read 30 bits from first word: 30 bits of 1s
  uint32_t val1 = reader.ReadU32(30);
  assert(val1 == 0x3FFFFFFF);

  // Read 10 bits spanning word boundary:
  // 2 bits from first word: 11
  // 8 bits from second word (0xAA = 0b10101010): 10101010
  // Combined: 0b1110101010 = 938
  uint32_t val2 = reader.ReadU32(10);
  assert(val2 == 938);

  std::cout << "TestReadAcrossWordBoundary: PASSED\n";
}

void TestReadU32Or0() {
  uint8_t data[] = {0xFF};
  BitReader reader(data, 1);

  // Test with 0 bits - should return 0 without reading
  assert(reader.ReadU32Or0(0) == 0);

  // Test with actual bits - should read normally
  // First 4 bits: 0b1111 = 15
  assert(reader.ReadU32Or0(4) == 15);

  std::cout << "TestReadU32Or0: PASSED\n";
}

void TestReadI32Or0() {
  uint8_t data[] = {0xF0};
  BitReader reader(data, 1);

  // Test with 0 bits - should return 0 without reading
  assert(reader.ReadI32Or0(0) == 0);

  // Test with actual bits - should read normally
  // First 4 bits: 0b1111 = -1 with sign extension
  assert(reader.ReadI32Or0(4) == -1);

  std::cout << "TestReadI32Or0: PASSED\n";
}

void TestRead32BitValue() {
  // File bytes: 0x12, 0x34, 0x56, 0x78
  // Full word (4 bytes):
  //   - Dereference on little-endian: 0x78563412
  //   - Byte swap: 0x12345678
  //   - Read 32 bits MSB first: 0x12345678
  uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
  BitReader reader(data, 4);

  uint32_t result = reader.ReadU32(32);
  assert(result == 0x12345678);

  std::cout << "TestRead32BitValue: PASSED\n";
}

void TestByteSwapping() {
  // Verify byte swapping works correctly
  // File bytes: 0x01, 0x02, 0x03, 0x04
  // Dereference (little-endian): 0x04030201
  // Byte swap: 0x01020304
  // Read 32 bits: 0x01020304
  uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  BitReader reader(data, 4);

  uint32_t result = reader.ReadU32(32);
  assert(result == 0x01020304);

  std::cout << "TestByteSwapping: PASSED\n";
}

void TestPartialWord() {
  // File bytes: 0xAB, 0xCD (< 4 bytes)
  // Partial word: manually loaded as 0xABCD0000
  uint8_t data[] = {0xAB, 0xCD};
  BitReader reader(data, 2);

  // Read 8 bits: 0xAB
  assert(reader.ReadU32(8) == 0xAB);

  // Read 8 bits: 0xCD
  assert(reader.ReadU32(8) == 0xCD);

  // No more data
  assert(reader.ReadU32(8) == 0);

  std::cout << "TestPartialWord: PASSED\n";
}

void TestHasData() {
  uint8_t data[] = {0xFF};
  BitReader reader(data, 1);

  assert(reader.HasData() == true);

  reader.ReadU32(8); // Read all 8 bits
  assert(reader.HasData() == false);

  std::cout << "TestHasData: PASSED\n";
}

int RunBitReaderTests() {
  std::cout << "Running BitReader unit tests (32-bit words with byte swap)...\n\n";

  TestReadBit();
  TestReadU32();
  TestReadI32SignExtension();
  TestReadI32Positive();
  TestReadAcrossByteBoundary();
  TestReadAcrossWordBoundary();
  TestReadU32Or0();
  TestReadI32Or0();
  TestRead32BitValue();
  TestByteSwapping();
  TestPartialWord();
  TestHasData();

  std::cout << "\n=== All tests PASSED ===\n";

  return 0;
}
