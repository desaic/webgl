#pragma once
#include <cstdint>

enum class SegmentType : int32_t {
  Invalid = 0,
  LogicalSceneGraph = 1,
  JTBRep = 2,
  PMIData = 3,
  MetaData = 4,
  Shape = 6,
  ShapeLOD0 = 7,
  ShapeLOD1 = 8,
  ShapeLOD2 = 9,
  ShapeLOD3 = 10,
  ShapeLOD4 = 11,
  ShapeLOD5 = 12,
  ShapeLOD6 = 13,
  ShapeLOD7 = 14,
  ShapeLOD8 = 15,
  ShapeLOD9 = 16,
  XTBRep = 17,
  WireframeRep = 18,
  ULP = 20,
  STT = 23,
  LWPA = 24,
  MultiXTBRep = 30,
  InfoSegment = 31,
  AECShape = 32,
  StepBRep = 33
};

/**
 * Utility to check the "Compression" (Yes/No) column from the spec.
 * Returns true if the segment type supports compression.
 */
constexpr bool IsCompressionSupported(uint32_t typeVal) {
  SegmentType type = SegmentType(typeVal);
  switch (type) {
  case SegmentType::LogicalSceneGraph:
  case SegmentType::JTBRep:
  case SegmentType::PMIData:
  case SegmentType::MetaData:
  case SegmentType::XTBRep:
  case SegmentType::WireframeRep:
  case SegmentType::ULP:
  case SegmentType::STT:
  case SegmentType::LWPA:
  case SegmentType::MultiXTBRep:
  case SegmentType::InfoSegment:
  case SegmentType::AECShape:
  case SegmentType::StepBRep:
    return true;
  default:
    return false;
  }
}

// a jt file can be multiple gigs but a 
// single segment within it cannot be due to
// int32 segment size limit
const int DATA_SEGMENT_SIZE_LIMIT = (~(1 << 31));