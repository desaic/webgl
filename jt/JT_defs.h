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
#include "Vec3.h"

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

  // Convert GUID to hex string format
  std::string GUIDToString(const GUID &guid);

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
      // size of base + version + bindings
      static const unsigned BYTES = 10;
  };

  // Vertex Shape LOD Bindings bit flags (Table 48 in JT spec)
  namespace VertexBindings {
    // Vertex Coordinate Bindings (Bits 1-3)
    constexpr uint64_t VERTEX_COORD_2_COMPONENT = 0x0001ULL; // Bit 1
    constexpr uint64_t VERTEX_COORD_3_COMPONENT = 0x0002ULL; // Bit 2
    constexpr uint64_t VERTEX_COORD_4_COMPONENT = 0x0004ULL; // Bit 3
    constexpr uint64_t VERTEX_COORD_MASK = 0x0007ULL;

    // Normal Binding (Bit 4)
    constexpr uint64_t NORMAL_BINDING = 0x0008ULL; // Bit 4

    // Colour Bindings (Bits 5-6)
    constexpr uint64_t COLOUR_3_COMPONENT = 0x0010ULL; // Bit 5
    constexpr uint64_t COLOUR_4_COMPONENT = 0x0020ULL; // Bit 6
    constexpr uint64_t COLOUR_MASK = 0x0030ULL;

    // Vertex Flag Binding (Bit 7)
    constexpr uint64_t VERTEX_FLAG_BINDING = 0x0040ULL; // Bit 7

    // Texture Coordinate 0 Bindings (Bits 9-12)
    constexpr uint64_t TEX_COORD_0_1_COMPONENT = 0x0100ULL; // Bit 9
    constexpr uint64_t TEX_COORD_0_2_COMPONENT = 0x0200ULL; // Bit 10
    constexpr uint64_t TEX_COORD_0_3_COMPONENT = 0x0400ULL; // Bit 11
    constexpr uint64_t TEX_COORD_0_4_COMPONENT = 0x0800ULL; // Bit 12
    constexpr uint64_t TEX_COORD_0_MASK = 0x0F00ULL;

    // Texture Coordinate 1 Bindings (Bits 13-16)
    constexpr uint64_t TEX_COORD_1_1_COMPONENT = 0x1000ULL; // Bit 13
    constexpr uint64_t TEX_COORD_1_2_COMPONENT = 0x2000ULL; // Bit 14
    constexpr uint64_t TEX_COORD_1_3_COMPONENT = 0x4000ULL; // Bit 15
    constexpr uint64_t TEX_COORD_1_4_COMPONENT = 0x8000ULL; // Bit 16
    constexpr uint64_t TEX_COORD_1_MASK = 0xF000ULL;

    // Texture Coordinate 2 Bindings (Bits 17-20)
    constexpr uint64_t TEX_COORD_2_1_COMPONENT = 0x10000ULL; // Bit 17
    constexpr uint64_t TEX_COORD_2_2_COMPONENT = 0x20000ULL; // Bit 18
    constexpr uint64_t TEX_COORD_2_3_COMPONENT = 0x40000ULL; // Bit 19
    constexpr uint64_t TEX_COORD_2_4_COMPONENT = 0x80000ULL; // Bit 20
    constexpr uint64_t TEX_COORD_2_MASK = 0xF0000ULL;

    // Texture Coordinate 3 Bindings (Bits 21-24)
    constexpr uint64_t TEX_COORD_3_1_COMPONENT = 0x100000ULL; // Bit 21
    constexpr uint64_t TEX_COORD_3_2_COMPONENT = 0x200000ULL; // Bit 22
    constexpr uint64_t TEX_COORD_3_3_COMPONENT = 0x400000ULL; // Bit 23
    constexpr uint64_t TEX_COORD_3_4_COMPONENT = 0x800000ULL; // Bit 24
    constexpr uint64_t TEX_COORD_3_MASK = 0xF00000ULL;

    // Texture Coordinate 4 Bindings (Bits 25-28)
    constexpr uint64_t TEX_COORD_4_1_COMPONENT = 0x1000000ULL; // Bit 25
    constexpr uint64_t TEX_COORD_4_2_COMPONENT = 0x2000000ULL; // Bit 26
    constexpr uint64_t TEX_COORD_4_3_COMPONENT = 0x4000000ULL; // Bit 27
    constexpr uint64_t TEX_COORD_4_4_COMPONENT = 0x8000000ULL; // Bit 28
    constexpr uint64_t TEX_COORD_4_MASK = 0xF000000ULL;

    // Texture Coordinate 5 Bindings (Bits 29-32)
    constexpr uint64_t TEX_COORD_5_1_COMPONENT = 0x10000000ULL; // Bit 29
    constexpr uint64_t TEX_COORD_5_2_COMPONENT = 0x20000000ULL; // Bit 30
    constexpr uint64_t TEX_COORD_5_3_COMPONENT = 0x40000000ULL; // Bit 31
    constexpr uint64_t TEX_COORD_5_4_COMPONENT = 0x80000000ULL; // Bit 32
    constexpr uint64_t TEX_COORD_5_MASK = 0xF0000000ULL;

    // Texture Coordinate 6 Bindings (Bits 33-36)
    constexpr uint64_t TEX_COORD_6_1_COMPONENT = 0x100000000ULL; // Bit 33
    constexpr uint64_t TEX_COORD_6_2_COMPONENT = 0x200000000ULL; // Bit 34
    constexpr uint64_t TEX_COORD_6_3_COMPONENT = 0x400000000ULL; // Bit 35
    constexpr uint64_t TEX_COORD_6_4_COMPONENT = 0x800000000ULL; // Bit 36
    constexpr uint64_t TEX_COORD_6_MASK = 0xF00000000ULL;

    // Texture Coordinate 7 Bindings (Bits 37-40)
    constexpr uint64_t TEX_COORD_7_1_COMPONENT = 0x1000000000ULL; // Bit 37
    constexpr uint64_t TEX_COORD_7_2_COMPONENT = 0x2000000000ULL; // Bit 38
    constexpr uint64_t TEX_COORD_7_3_COMPONENT = 0x4000000000ULL; // Bit 39
    constexpr uint64_t TEX_COORD_7_4_COMPONENT = 0x8000000000ULL; // Bit 40
    constexpr uint64_t TEX_COORD_7_MASK = 0xF000000000ULL;

    // Auxiliary Vertex Field Binding (Bit 63)
    constexpr uint64_t AUXILIARY_VERTEX_FIELD = 0x4000000000000000ULL; // Bit 63
  }

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

  struct TopoMeshLODData{
    uint8_t version = 0;
    //U32: Vertex Records Object ID
    uint32_t objectId = 0;
    static const unsigned BYTES = 5;
  };

  // Topologically Compressed Rep Data (Figure 92 in JT spec)
  struct TopologicallyCompressedRepData {
    // Topological information (22 fields)
    std::vector<int32_t> faceDegrees[8];           // VecI32{Int32CDP} x8: degrees of faces (per context group)
    std::vector<int32_t> vertexValences;        // VecI32{Int32CDP}: valences of vertices
    std::vector<int32_t> vertexGroups;          // VecI32{Int32CDP}: face group number for each dual vertex
    std::vector<int32_t> vertexFlags;           // VecI32{Int32CDP, Lag1}: 0=original, 1=cover face
    std::vector<int32_t> faceAttributeMasks[8]; // VecI32{Int32CDP} x8: face attribute bit vectors (32 LSBs for all, 32 MSBs for group 8)
    std::vector<uint32_t> highDegreeFaceAttributeMasks; // VecU32: remaining face attribute bit vectors
    std::vector<int32_t> splitFaceSyms;         // VecI32{Int32CDP, Lag1}: split face symbols
    std::vector<int32_t> splitFacePositions;    // VecI32{Int32CDP}: split face positions
    uint32_t compositeHash = 0;                 // U32: hash of topological data

    // Vertex Records (Figure 93)
    uint64_t vertexBindings = 0;                // U64: binding information (if coordinate bindings present)
    // Quantization Parameters would go here
    int32_t numTopologicalVertices = 0;         // I32: number of unique vertex coordinates
    int32_t numVertexAttributes = 0;            // I32: number of vertex attribute records (if numTopoVts > 0)

    // Compressed arrays (actual formats defined in compression section)
    std::vector<uint8_t> compressedVertexCoords;      // if Coordinate Bindings
    std::vector<uint8_t> compressedVertexNormals;     // if Normal Bindings
    std::vector<uint8_t> compressedVertexColors;      // if Colour Bindings
    std::vector<uint8_t> compressedTexCoords[8];      // if Tex Coord n Bindings (n=0..7)
    std::vector<uint8_t> compressedVertexFlagArray;   // if vertex flag Bindings
    std::vector<uint8_t> compressedAuxiliaryFields;   // if AuxField Bindings
  };

  // Topologically Compressed Vertex Records (Figure 93 in JT spec)
  // Simplified storage of decompressed vertex data
  struct TopologicallyCompressedVertexRecords {
    uint64_t vertexBindings = 0;                // U64: binding flags from VertexBindings namespace
    int32_t numTopologicalVertices = 0;         // Number of unique vertex coordinates
    int32_t numVertexAttributes = 0;            // Number of vertex attribute records

    // Decompressed vertex data (simplified storage)
    std::vector<Vec3f> vertexCoords;            // Decompressed vertex coordinates (if Coordinate Bindings)
    std::vector<Vec3f> vertexNormals;           // Decompressed vertex normals (if Normal Bindings)
    std::vector<Vec3f> vertexColors;            // Decompressed vertex colors (if Colour Bindings) - RGB or RGBA as Vec3f
    // Texture coordinates ignored for now
    std::vector<uint8_t> vertexFlags;           // Decompressed vertex flags (if vertex flag Bindings)
    // Auxiliary fields ignored for now

    // Check if specific binding is present
    bool hasCoordinates() const {
      return (vertexBindings & (VertexBindings::VERTEX_COORD_2_COMPONENT |
                                VertexBindings::VERTEX_COORD_3_COMPONENT |
                                VertexBindings::VERTEX_COORD_4_COMPONENT)) != 0;
    }
    bool hasNormals() const {
      return (vertexBindings & VertexBindings::NORMAL_BINDING) != 0;
    }
    bool hasColors() const {
      return (vertexBindings & VertexBindings::COLOUR_MASK) != 0;
    }
    bool hasFlags() const {
      return (vertexBindings & VertexBindings::VERTEX_FLAG_BINDING) != 0;
    }
  };

  // TopoMesh Topologically Compressed LOD Data (Figure 91)
  struct TopologicallyCompressed{
    TopoMeshLODData topoLOD;
    uint8_t version = 0;
    TopologicallyCompressedRepData repData;
    TopologicallyCompressedVertexRecords vertices;
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
