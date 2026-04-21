#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <fstream>

#include "BBox.h"

namespace JT {

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
  inline constexpr int DATA_SEGMENT_SIZE_LIMIT = (~(1 << 31));

  struct GUID {
      std::array<uint8_t, 16> bytes = {};

      bool operator<(const GUID &other) const {
        return bytes < other.bytes;
      }
  };

  // Helper to create GUIDs from the spec format
  GUID CreateGUID(uint32_t d1,
                  uint16_t d2,
                  uint16_t d3,
                  uint8_t b1,
                  uint8_t b2,
                  uint8_t b3,
                  uint8_t b4,
                  uint8_t b5,
                  uint8_t b6,
                  uint8_t b7,
                  uint8_t b8);

  enum class ObjectType {
    Unknown = 0,
    EndOfElements,
    // Node Elements
    BaseNode,
    GroupNode,
    InstanceNode,
    LODNode,
    MetaDataNode,
    NullShapeNode,
    PartNode,
    PartitionNode,
    RangeLODNode,
    SwitchNode,
    // Shape Node Elements
    BaseShapeNode,
    PointSetShapeNode,
    PolygonSetShapeNode,
    PolylineSetShapeNode,
    PrimitiveSetShapeNode,
    TriStripSetShapeNode,
    VertexShapeNode,
    // Attributes
    BaseAttributeData,
    DrawStyleAttribute,
    GeometricTransformAttribute,
    InfiniteLightAttribute,
    LightSetAttribute,
    LinestyleAttribute,
    MaterialAttribute,
    PointLightAttribute,
    PointstyleAttribute,
    TextureImageAttribute,
    TextureCoordinateGenerator,
    // Mapping Elements
    MappingPlane,
    MappingCylinder,
    MappingSphere,
    MappingTriPlanar,
    // Properties
    BasePropertyAtom,
    DatePropertyAtom,
    IntegerPropertyAtom,
    FloatingPointPropertyAtom,
    LateLoadedPropertyAtom,
    JTObjectReferencePropertyAtom,
    StringPropertyAtom,
    // Precise Geometry / Segments
    JTBRepElement,
    PMIManagerMetaData,
    PropertyProxyMetaData,
    // Shape LOD Elements
    NullShapeLOD,
    PointSetShapeLOD,
    PolylineSetShapeLOD,
    PrimitiveSetShapeElement,
    TriStripSetShapeLOD,
    PolygonSetLOD,
    VertexShapeLOD,
    // Special Precise Formats
    XTBRepElement,
    WireframeRepElement,
    JTULPElement,
    JTLWPAElement,
    StepBRepElement
  };

  // cannot read as whole because JT file design
  // did not consider data alignment requirements
  struct JTHeader {
      static const unsigned VERSION_LEN = 80;
      std::array<char, VERSION_LEN> version = {};
      // only 0 little endian is handled.
      uint8_t byteOrder = 0;
      int32_t emptyField = 0;
      // byte offset from the top of the file to the start of the TOC Segment
      uint64_t TOCOffset = 0;
      // data segment ID for logical scene graph, i.e. root.
      GUID LSGSegId;
  };

  struct TOCEntry {
      GUID segId;
      uint64_t segOffset = 0;
      uint32_t len = 0;
      uint32_t attr = 0;
      uint32_t type = 0;
      // bits 24-31 of attr.
      uint32_t GetType() const {
        return attr >> 24;
      }
      bool valid()const{
        return len > 0;
      }
  };

  struct DataSegment {
      GUID segId;
      uint32_t type = 0;
      uint32_t len = 0;
      static const unsigned BYTES = 24;
      std::vector<uint8_t> data;
  };

  struct CompressionHeader {
      uint32_t compressionFlag = 0;
      int compressedLen = 0;
      uint8_t compressionAlgorithm = 0;
  };

  struct DataElement {
      // length in bytes of the Element
      // invalid when length is 0.
      // includes everything except the 4 byte element length.
      int elementLength = 0;
      GUID objectType;
      uint8_t baseType = 0;
      int objectId = 0;
      const static unsigned BYTES = 25;
      std::vector<uint8_t> data;
  };

  struct RangeI32 {
      int min = 0;
      int max = 0;
  };

  struct BaseNodeData {
      uint8_t version = 0;
      uint32_t flags = 0;
      int32_t attributeCount = 0;
      std::vector<int> attributeIds;
  };

  struct GroupNodeData {
      BaseNodeData base;
      uint8_t version = 0;
      int32_t childCount = 0;
      std::vector<int> childIds;
  };

  struct NodeInfo {
      GroupNodeData group;
      std::u16string name;
      std::u16string jtPropName;
  };

  struct PartitionNode {
      GroupNodeData group;
      int partitionFlags = 0;
      std::u16string fileName;
      Box3f transformedBox;
      float area = 0.0f;
      RangeI32 vertexCount;
      RangeI32 polyCount;
      Box3f untransformedBox;
  };

  struct BasePropertyAtomData {
      uint8_t version = 0;
      uint32_t flags = 0;
      const static unsigned BYTES = 5;
  };

  struct LateLoadedPropertyAtom {
      BasePropertyAtomData base;
      uint8_t version = 0;
      GUID segId;
      int segType = 0;
      int payloadId = 0;
      int reserved = 0;
  };

  struct BaseShapeData {
      DataElement element;
      uint8_t version = 0;
      Box3f untransformedBox;
      float area = 0.0f;
      RangeI32 vertexCountRange;
      RangeI32 nodeCountRange;
      RangeI32 polyCountRange;
      uint32_t size = 0;
      float compressionLevel = 1.0f;
  };

  struct VertexShapeData {
      BaseNodeData base;
      uint8_t version = 0;
      uint64_t binding = 0;
  };
  struct BaseShapeLODData {
      uint8_t version = 0;
  };
  struct VertexShapeLODData {
      BaseShapeLODData base;
      uint8_t version = 0;
      uint64_t bindings = 0;
  };

  struct ShapeLODSegment {
      DataSegment header;
      DataElement element;
  };

  struct TriStripSetShapeLOD {
      DataElement element;
      VertexShapeLODData vertices;
      uint8_t version = 0;
  };

  class JTFile {
    public:
    JTFile(std::ifstream & fileIn):file(fileIn){}
      JTHeader header;
      std::vector<TOCEntry> TOCs;
      // from GUID to TOC entry index into TOCs list.
      std::map<GUID, size_t> TOCMap;
      std::ifstream &file;
      void BuildTOCMap() {
        for (size_t i = 0; i < TOCs.size(); i++) {
          TOCMap[TOCs[i].segId] = i;
        }
      }
      TOCEntry GetEntry(const GUID &guid) const {
        auto it = TOCMap.find(guid);
        if (it != TOCMap.end()) {
          return TOCs[it->second];
        } else {
          // invalid entry with 0 length
          return TOCEntry();
        }
      }
  };

  struct PropertyTable {
      short version = 0;
      // int because spec says so.
      int size = 0;
      using ElementProperties = std::vector<std::pair<int, int>>;
      std::unordered_map<int, ElementProperties> table;
  };

  // LSG
  class SceneGraph {
    public:
    SceneGraph(){}
      void PrintHierarchy(std::ostream &out);
      // map from object id to data offset.
      std::unordered_map<int, unsigned> nodesMap;
      std::unordered_map<int, unsigned> propertyAtomMap;
      PropertyTable propertyTable;
      // offset for "endOfElement" element.
      // there could be stuff after end of elements.
      unsigned endOfElementOffset = 0;
      unsigned endOfPropertiesOffset = 0;
      // Store reference to decompressed segment data for hierarchy traversal
      // NOTE: The DataSegment must remain valid for the lifetime of SceneGraph
      // or at least until PrintHierarchy is called
      const DataSegment *segmentData = nullptr;
  };

  // Check if a node type is a Shape Node
  bool IsShapeNodeType(JT::ObjectType type);

  bool IsShapeSegType(unsigned type);
  
  ObjectType GetObjectType(const GUID &guid);

  const char *ObjectTypeToString(ObjectType type);

  // Helper to determine if a node type can have children
  bool NodeTypeHasChildren(JT::ObjectType type);

  const char *SegmentTypeToString(SegmentType type);

} // namespace JT
