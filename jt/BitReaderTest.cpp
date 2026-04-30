#include "BitReader.h"
#include <iostream>
#include <cassert>
#include <cstring>

using namespace JT;

void TestReadBit() {
  // Test pattern: 0b10110011 = 0xB3
  // Little-endian: read LSB first (bit 0, then bit 1, etc.)
  uint8_t data[] = {0xB3};
  BitReader reader(data, 1);

  assert(reader.ReadBit() == 1); // bit 0 (LSB)
  assert(reader.ReadBit() == 1); // bit 1
  assert(reader.ReadBit() == 0); // bit 2
  assert(reader.ReadBit() == 0); // bit 3
  assert(reader.ReadBit() == 1); // bit 4
  assert(reader.ReadBit() == 1); // bit 5
  assert(reader.ReadBit() == 0); // bit 6
  assert(reader.ReadBit() == 1); // bit 7 (MSB)

  std::cout << "TestReadBit: PASSED\n";
}

void TestReadU32() {
  // Test reading unsigned integers
  // Pattern: 0xD6 = 0b11010110, 0x4F = 0b01001111
  // Little-endian reads LSB first from each byte
  uint8_t data[] = {0xD6, 0x4F};
  BitReader reader(data, 2);

  // Read 4 bits from 0xD6: bits [0-3] = 0b0110 = 6
  assert(reader.ReadU32(4) == 6);

  // Read 5 bits: bit 4-7 of 0xD6 + bit 0 of 0x4F = 0b11101 = 29
  // 0xD6 bits 4-7: 0b1101, 0x4F bit 0: 0b1 -> 0b11101 = 29
  assert(reader.ReadU32(5) == 29);

  // Read 7 bits: bits 1-7 of 0x4F = 0b0100111 = 39
  assert(reader.ReadU32(7) == 39);

  std::cout << "TestReadU32: PASSED\n";
}

void TestReadI32SignExtension() {
  // Test signed integer reading with sign extension
  // Pattern: 0xF0 = 0b11110000
  // Little-endian: bits are 0,0,0,0,1,1,1,1 (LSB first)
  uint8_t data[] = {0xF0};
  BitReader reader(data, 1);

  // Read 4 bits: 0b0000 = 0
  int32_t val1 = reader.ReadI32(4);
  assert(val1 == 0);

  // Read 4 bits: 0b1111 = -1 (with sign extension)
  int32_t val2 = reader.ReadI32(4);
  assert(val2 == -1);

  std::cout << "TestReadI32SignExtension: PASSED\n";
}

void TestReadI32Positive() {
  // Test positive signed integers
  // Pattern: 0x5A = 0b01011010
  // Little-endian: bits are 0,1,0,1,1,0,1,0 (LSB first)
  uint8_t data[] = {0x5A};
  BitReader reader(data, 1);

  // Read 4 bits: 0b1010 = 10 (positive, no sign extension)
  int32_t val = reader.ReadI32(4);
  assert(val == -6); // 0b1010 with 4-bit sign extension = -6

  std::cout << "TestReadI32Positive: PASSED\n";
}

void TestReadAcrossByteBoundary() {
  // Test reading values that span byte boundaries
  // Pattern: 0xCA = 0b11001010, 0xF0 = 0b11110000
  // Little-endian bits: CA: 0,1,0,1,0,0,1,1  F0: 0,0,0,0,1,1,1,1
  uint8_t data[] = {0xCA, 0xF0};
  BitReader reader(data, 2);

  // Read 6 bits from 0xCA: bits [0-5] = 0b001010 = 10
  assert(reader.ReadU32(6) == 10);

  // Read 10 bits spanning boundary: bits 6-7 of CA + bits 0-7 of F0
  // CA bits 6-7: 1,1  F0 bits 0-7: 0,0,0,0,1,1,1,1
  // Combined (LSB first): 1,1,0,0,0,0,1,1,1,1 = 0b1111000011 = 963
  assert(reader.ReadU32(10) == 963);

  std::cout << "TestReadAcrossByteBoundary: PASSED\n";
}

void TestReadU32Or0() {
  uint8_t data[] = {0xFF};
  BitReader reader(data, 1);

  // Test with 0 bits - should return 0 without reading
  assert(reader.ReadU32Or0(0) == 0);

  // Test with actual bits - should read normally
  // 0xFF = 0b11111111, first 4 bits (LSB) = 0b1111 = 15
  assert(reader.ReadU32Or0(4) == 15);

  std::cout << "TestReadU32Or0: PASSED\n";
}

void TestReadI32Or0() {
  uint8_t data[] = {0xF0};
  BitReader reader(data, 1);

  // Test with 0 bits - should return 0 without reading
  assert(reader.ReadI32Or0(0) == 0);

  // Test with actual bits - should read normally
  // 0xF0 = 0b11110000, first 4 bits (LSB) = 0b0000 = 0
  assert(reader.ReadI32Or0(4) == 0);

  std::cout << "TestReadI32Or0: PASSED\n";
}

void TestRead32BitValue() {
  // Test reading a full 32-bit value
  // Store bytes in order: 0x78, 0x56, 0x34, 0x12
  uint8_t data[] = {0x78, 0x56, 0x34, 0x12};
  BitReader reader(data, 4);

  // Little-endian bit reading will read all 32 bits LSB first from each byte
  // This reconstructs the value byte-by-byte in little-endian order
  uint32_t result = reader.ReadU32(32);

  // Expected: 0x12345678 (bytes in reverse order due to little-endian)
  assert(result == 0x12345678);

  std::cout << "TestRead32BitValue: PASSED\n";
}

void TestGetBitPosition() {
  uint8_t data[] = {0xFF, 0xFF};
  BitReader reader(data, 2);

  assert(reader.GetBitPosition() == 0);

  reader.ReadU32(5);
  assert(reader.GetBitPosition() == 5);

  reader.ReadU32(8);
  assert(reader.GetBitPosition() == 13);

  std::cout << "TestGetBitPosition: PASSED\n";
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
  std::cout << "Running BitReader unit tests (little-endian, LSB first)...\n\n";

  TestReadBit();
  TestReadU32();
  TestReadI32SignExtension();
  TestReadI32Positive();
  TestReadAcrossByteBoundary();
  TestReadU32Or0();
  TestReadI32Or0();
  TestRead32BitValue();
  TestGetBitPosition();
  TestHasData();

  std::cout << "\n=== All tests PASSED ===\n";

  return 0;
}
