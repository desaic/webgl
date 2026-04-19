#include "JT_defs.h"
#include <cstdint>
#include <map>

namespace JT {

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
                  uint8_t b8) {
    return GUID{{static_cast<uint8_t>(d1 & 0xFF), static_cast<uint8_t>((d1 >> 8) & 0xFF),
                 static_cast<uint8_t>((d1 >> 16) & 0xFF), static_cast<uint8_t>((d1 >> 24) & 0xFF),
                 static_cast<uint8_t>(d2 & 0xFF), static_cast<uint8_t>((d2 >> 8) & 0xFF),
                 static_cast<uint8_t>(d3 & 0xFF), static_cast<uint8_t>((d3 >> 8) & 0xFF), b1, b2, b3, b4, b5, b6, b7,
                 b8}};
  }

  // Core LSG Nodes needed to find meshes [cite: 1121]
  const GUID GUID_PartitionNode =
      CreateGUID(0x10dd103e, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97);
  const GUID GUID_PartNode = CreateGUID(0xce357244, 0x38fb, 0x11d1, 0xa5, 0x6, 0x0, 0x60, 0x97, 0xbd, 0xc6, 0xe1);
  const GUID GUID_InstanceNode = CreateGUID(0x10dd102a, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97);

  // Shape LOD Elements (The actual mesh data) [cite: 1122]
  const GUID GUID_VertexShapeLOD =
      CreateGUID(0x10dd10b0, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97);
  const GUID GUID_TriStripShapeLOD =
      CreateGUID(0x10dd10ab, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97);
  const GUID GUID_PolylineShapeLOD =
      CreateGUID(0x10dd10a1, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97);
  const GUID GUID_PointSetShapeLOD =
      CreateGUID(0x98134716, 0x0011, 0x0818, 0x19, 0x98, 0x08, 0x00, 0x09, 0x83, 0x5d, 0x5a);

  ObjectType GetObjectType(const GUID &guid) {
    static const std::map<GUID, ObjectType> mapper = {
        {CreateGUID(0xffffffff, 0xffff, 0xffff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff),
         ObjectType::EndOfElements},
        // Nodes
        {CreateGUID(0x10dd1035, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97), ObjectType::BaseNode},
        {CreateGUID(0x10dd101b, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97), ObjectType::GroupNode},
        {CreateGUID(0x10dd102a, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::InstanceNode},
        {CreateGUID(0x10dd102c, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97), ObjectType::LODNode},
        {CreateGUID(0xce357245, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1),
         ObjectType::MetaDataNode},
        {CreateGUID(0xd239e7b6, 0xdd77, 0x4289, 0xa0, 0x7d, 0xb0, 0xee, 0x79, 0xf7, 0x94, 0x94),
         ObjectType::NullShapeNode},
        {CreateGUID(0xce357244, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1), ObjectType::PartNode},
        {CreateGUID(0x10dd103e, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::PartitionNode},
        {CreateGUID(0x10dd104c, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::RangeLODNode},
        {CreateGUID(0x10dd10f3, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::SwitchNode},
        // Shape Nodes
        {CreateGUID(0x10dd1059, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::BaseShapeNode},
        {CreateGUID(0x98134716, 0x0010, 0x0818, 0x19, 0x98, 0x08, 0x00, 0x09, 0x83, 0x5d, 0x5a),
         ObjectType::PointSetShapeNode},
        {CreateGUID(0x10dd1048, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::PolygonSetShapeNode},
        {CreateGUID(0x10dd1046, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::PolylineSetShapeNode},
        {CreateGUID(0xe40373c1, 0x1ad9, 0x11d3, 0x9d, 0xaf, 0x00, 0xa0, 0xc9, 0xc7, 0xdd, 0xc2),
         ObjectType::PrimitiveSetShapeNode},
        {CreateGUID(0x10dd1077, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::TriStripSetShapeNode},
        {CreateGUID(0x10dd107f, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::VertexShapeNode},
        // Attributes
        {CreateGUID(0x10dd1001, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::BaseAttributeData},
        {CreateGUID(0x10dd1014, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::DrawStyleAttribute},
        {CreateGUID(0x10dd1083, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::GeometricTransformAttribute},
        {CreateGUID(0x10dd1028, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::InfiniteLightAttribute},
        {CreateGUID(0x10dd1096, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::LightSetAttribute},
        {CreateGUID(0x10dd10c4, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::LinestyleAttribute},
        {CreateGUID(0x10dd1030, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::MaterialAttribute},
        {CreateGUID(0x10dd1045, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::PointLightAttribute},
        {CreateGUID(0x8d57c010, 0xe5cb, 0x11d4, 0x84, 0x0e, 0x00, 0xa0, 0xd2, 0x18, 0x2f, 0x9d),
         ObjectType::PointstyleAttribute},
        {CreateGUID(0x10dd1073, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::TextureImageAttribute},
        {CreateGUID(0xaa1b831d, 0x6e47, 0x4fee, 0xa8, 0x65, 0xcd, 0x7e, 0x1f, 0x2f, 0x39, 0xdc),
         ObjectType::TextureCoordinateGenerator},
        // Mapping
        {CreateGUID(0xa3cfb921, 0xbdeb, 0x48d7, 0xb3, 0x96, 0x8b, 0x8d, 0x0e, 0xf4, 0x85, 0xa0),
         ObjectType::MappingPlane},
        {CreateGUID(0x3e70739d, 0x8cb0, 0x41ef, 0x84, 0x5c, 0xa1, 0x98, 0xd4, 0x00, 0x3b, 0x3f),
         ObjectType::MappingCylinder},
        {CreateGUID(0x72475fd1, 0x2823, 0x4219, 0xa0, 0x6c, 0xd9, 0xe6, 0xe3, 0x9a, 0x45, 0xc1),
         ObjectType::MappingSphere},
        {CreateGUID(0x92f5b094, 0x6499, 0x4d2d, 0x92, 0xaa, 0x60, 0xd0, 0x5a, 0x44, 0x32, 0xcf),
         ObjectType::MappingTriPlanar},
        // Properties
        {CreateGUID(0x10dd104b, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::BasePropertyAtom},
        {CreateGUID(0xce357246, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1),
         ObjectType::DatePropertyAtom},
        {CreateGUID(0x10dd102b, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::IntegerPropertyAtom},
        {CreateGUID(0x10dd1019, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::FloatingPointPropertyAtom},
        {CreateGUID(0xe0b05be5, 0xfbbd, 0x11d1, 0xa3, 0xa7, 0x00, 0xaa, 0x00, 0xd1, 0x09, 0x54),
         ObjectType::LateLoadedPropertyAtom},
        {CreateGUID(0x10dd1004, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::JTObjectReferencePropertyAtom},
        {CreateGUID(0x10dd106e, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::StringPropertyAtom},
        // Segments
        {CreateGUID(0x873a70c0, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::JTBRepElement},
        {CreateGUID(0xce357249, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1),
         ObjectType::PMIManagerMetaData},
        {CreateGUID(0xce357247, 0x38fb, 0x11d1, 0xa5, 0x06, 0x00, 0x60, 0x97, 0xbd, 0xc6, 0xe1),
         ObjectType::PropertyProxyMetaData},
        // Shape LODs
        {CreateGUID(0x3e637aed, 0x2a89, 0x41f8, 0xa9, 0xfd, 0x55, 0x37, 0x37, 0x03, 0x96, 0x82),
         ObjectType::NullShapeLOD},
        {CreateGUID(0x98134716, 0x0011, 0x0818, 0x19, 0x98, 0x08, 0x00, 0x09, 0x83, 0x5d, 0x5a),
         ObjectType::PointSetShapeLOD},
        {CreateGUID(0x10dd10a1, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::PolylineSetShapeLOD},
        {CreateGUID(0xe40373c2, 0x1ad9, 0x11d3, 0x9d, 0xaf, 0x00, 0xa0, 0xc9, 0xc7, 0xdd, 0xc2),
         ObjectType::PrimitiveSetShapeElement},
        {CreateGUID(0x10dd10ab, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::TriStripSetShapeLOD},
        {CreateGUID(0x10dd109f, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::PolygonSetLOD},
        {CreateGUID(0x10dd10b0, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::VertexShapeLOD},
        // Precise Geometry
        {CreateGUID(0x873a70e0, 0x2ac9, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::XTBRepElement},
        {CreateGUID(0x873a70d0, 0x2ac8, 0x11d1, 0x9b, 0x6b, 0x00, 0x80, 0xc7, 0xbb, 0x59, 0x97),
         ObjectType::WireframeRepElement},
        {CreateGUID(0xf338a4af, 0xd7d2, 0x41c5, 0xbc, 0xf2, 0xc5, 0x5a, 0x88, 0xb2, 0x1e, 0x73),
         ObjectType::JTULPElement},
        {CreateGUID(0xd67f8ea8, 0xf524, 0x4879, 0x92, 0x8c, 0x4c, 0x3a, 0x56, 0x1f, 0xb9, 0x3a),
         ObjectType::JTLWPAElement},
        {CreateGUID(0x869c7d53, 0xccb0, 0x451b, 0xb2, 0x03, 0xd1, 0x42, 0x81, 0x56, 0x14, 0x56),
         ObjectType::StepBRepElement}};

    auto it = mapper.find(guid);
    return (it != mapper.end()) ? it->second : ObjectType::Unknown;
  }

  const char *ObjectTypeToString(ObjectType type) {
    switch (type) {
    case ObjectType::Unknown:
      return "Unknown";
    case ObjectType::EndOfElements:
      return "EndOfElements";

      // Node Elements
    case ObjectType::BaseNode:
      return "BaseNode";
    case ObjectType::GroupNode:
      return "GroupNode";
    case ObjectType::InstanceNode:
      return "InstanceNode";
    case ObjectType::LODNode:
      return "LODNode";
    case ObjectType::MetaDataNode:
      return "MetaDataNode";
    case ObjectType::NullShapeNode:
      return "NullShapeNode";
    case ObjectType::PartNode:
      return "PartNode";
    case ObjectType::PartitionNode:
      return "PartitionNode";
    case ObjectType::RangeLODNode:
      return "RangeLODNode";
    case ObjectType::SwitchNode:
      return "SwitchNode";

      // Shape Node Elements
    case ObjectType::BaseShapeNode:
      return "BaseShapeNode";
    case ObjectType::PointSetShapeNode:
      return "PointSetShapeNode";
    case ObjectType::PolygonSetShapeNode:
      return "PolygonSetShapeNode";
    case ObjectType::PolylineSetShapeNode:
      return "PolylineSetShapeNode";
    case ObjectType::PrimitiveSetShapeNode:
      return "PrimitiveSetShapeNode";
    case ObjectType::TriStripSetShapeNode:
      return "TriStripSetShapeNode";
    case ObjectType::VertexShapeNode:
      return "VertexShapeNode";

      // Attributes
    case ObjectType::BaseAttributeData:
      return "BaseAttributeData";
    case ObjectType::DrawStyleAttribute:
      return "DrawStyleAttribute";
    case ObjectType::GeometricTransformAttribute:
      return "GeometricTransformAttribute";
    case ObjectType::InfiniteLightAttribute:
      return "InfiniteLightAttribute";
    case ObjectType::LightSetAttribute:
      return "LightSetAttribute";
    case ObjectType::LinestyleAttribute:
      return "LinestyleAttribute";
    case ObjectType::MaterialAttribute:
      return "MaterialAttribute";
    case ObjectType::PointLightAttribute:
      return "PointLightAttribute";
    case ObjectType::PointstyleAttribute:
      return "PointstyleAttribute";
    case ObjectType::TextureImageAttribute:
      return "TextureImageAttribute";
    case ObjectType::TextureCoordinateGenerator:
      return "TextureCoordinateGenerator";

      // Mapping Elements
    case ObjectType::MappingPlane:
      return "MappingPlane";
    case ObjectType::MappingCylinder:
      return "MappingCylinder";
    case ObjectType::MappingSphere:
      return "MappingSphere";
    case ObjectType::MappingTriPlanar:
      return "MappingTriPlanar";

      // Properties
    case ObjectType::BasePropertyAtom:
      return "BasePropertyAtom";
    case ObjectType::DatePropertyAtom:
      return "DatePropertyAtom";
    case ObjectType::IntegerPropertyAtom:
      return "IntegerPropertyAtom";
    case ObjectType::FloatingPointPropertyAtom:
      return "FloatingPointPropertyAtom";
    case ObjectType::LateLoadedPropertyAtom:
      return "LateLoadedPropertyAtom";
    case ObjectType::JTObjectReferencePropertyAtom:
      return "JTObjectReferencePropertyAtom";
    case ObjectType::StringPropertyAtom:
      return "StringPropertyAtom";

      // Precise Geometry / Segments
    case ObjectType::JTBRepElement:
      return "JTBRepElement";
    case ObjectType::PMIManagerMetaData:
      return "PMIManagerMetaData";
    case ObjectType::PropertyProxyMetaData:
      return "PropertyProxyMetaData";

      // Shape LOD Elements
    case ObjectType::NullShapeLOD:
      return "NullShapeLOD";
    case ObjectType::PointSetShapeLOD:
      return "PointSetShapeLOD";
    case ObjectType::PolylineSetShapeLOD:
      return "PolylineSetShapeLOD";
    case ObjectType::PrimitiveSetShapeElement:
      return "PrimitiveSetShapeElement";
    case ObjectType::TriStripSetShapeLOD:
      return "TriStripSetShapeLOD";
    case ObjectType::PolygonSetLOD:
      return "PolygonSetLOD";
    case ObjectType::VertexShapeLOD:
      return "VertexShapeLOD";

      // Special Precise Formats
    case ObjectType::XTBRepElement:
      return "XTBRepElement";
    case ObjectType::WireframeRepElement:
      return "WireframeRepElement";
    case ObjectType::JTULPElement:
      return "JTULPElement";
    case ObjectType::JTLWPAElement:
      return "JTLWPAElement";
    case ObjectType::StepBRepElement:
      return "StepBRepElement";

    default:
      return "UNKNOWN_OBJECT_TYPE";
    }
  }

  // Helper to determine if a node type can have children
  bool NodeTypeHasChildren(JT::ObjectType type) {
    switch (type) {
    case JT::ObjectType::GroupNode:
    case JT::ObjectType::PartitionNode:
    case JT::ObjectType::MetaDataNode:
    case JT::ObjectType::LODNode:
    case JT::ObjectType::RangeLODNode:
    case JT::ObjectType::SwitchNode:
    case JT::ObjectType::PartNode:
    case JT::ObjectType::InstanceNode: // Special case: single child
      return true;
    default:
      return false;
    }
  }

  const char *SegmentTypeToString(SegmentType type) {
    switch (type) {
    case SegmentType::Invalid:
      return "Invalid";
    case SegmentType::LogicalSceneGraph:
      return "LogicalSceneGraph";
    case SegmentType::JTBRep:
      return "JTBRep";
    case SegmentType::PMIData:
      return "PMIData";
    case SegmentType::MetaData:
      return "MetaData";
    case SegmentType::Shape:
      return "Shape";
    case SegmentType::ShapeLOD0:
      return "ShapeLOD0";
    case SegmentType::ShapeLOD1:
      return "ShapeLOD1";
    case SegmentType::ShapeLOD2:
      return "ShapeLOD2";
    case SegmentType::ShapeLOD3:
      return "ShapeLOD3";
    case SegmentType::ShapeLOD4:
      return "ShapeLOD4";
    case SegmentType::ShapeLOD5:
      return "ShapeLOD5";
    case SegmentType::ShapeLOD6:
      return "ShapeLOD6";
    case SegmentType::ShapeLOD7:
      return "ShapeLOD7";
    case SegmentType::ShapeLOD8:
      return "ShapeLOD8";
    case SegmentType::ShapeLOD9:
      return "ShapeLOD9";
    case SegmentType::XTBRep:
      return "XTBRep";
    case SegmentType::WireframeRep:
      return "WireframeRep";
    case SegmentType::ULP:
      return "ULP";
    case SegmentType::STT:
      return "STT";
    case SegmentType::LWPA:
      return "LWPA";
    case SegmentType::MultiXTBRep:
      return "MultiXTBRep";
    case SegmentType::InfoSegment:
      return "InfoSegment";
    case SegmentType::AECShape:
      return "AECShape";
    case SegmentType::StepBRep:
      return "StepBRep";
    default:
      return "UNKNOWN_SEGMENT_TYPE";
    }
  }
    // Check if a node type is a Shape Node
    bool IsShapeNodeType(ObjectType type) {
        switch (type) {
        case JT::ObjectType::BaseShapeNode:
        case JT::ObjectType::VertexShapeNode:
        case JT::ObjectType::TriStripSetShapeNode:
        case JT::ObjectType::PolylineSetShapeNode:
        case JT::ObjectType::PointSetShapeNode:
        case JT::ObjectType::PolygonSetShapeNode:
        case JT::ObjectType::PrimitiveSetShapeNode:
        case JT::ObjectType::NullShapeNode:
            return true;
        default:
            return false;
        }
    }

  }
