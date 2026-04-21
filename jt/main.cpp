#include <array>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>

#include "BBox.h"
#include "JT_defs.h"
#include "lzma_wrapper.h"
#include <codecvt>
#include <cstring>
#include <locale>
#include <set>
#include <string>

using namespace JT;

static uint32_t ToUint32(const uint8_t *bytes) {
  // Little Endian
  return *((uint32_t *)bytes);
}

static int32_t ToInt32(const uint8_t *bytes) {
  // Little Endian
  return *((int32_t *)bytes);
}

static int16_t ToInt16(const uint8_t *bytes) {
  // Little Endian
  return *((int16_t *)bytes);
}

static void ReadGUID(const uint8_t *buf, GUID &guid) {
  std::memcpy(guid.bytes.data(), buf, guid.bytes.size());
}

// Compare std::u16string with ASCII string
bool CompareU16StringToAscii(const std::u16string &u16str, const std::string &ascii) {
  if (u16str.length() != ascii.length())
    return false;
  for (size_t i = 0; i < ascii.length(); i++) {
    if (u16str[i] != static_cast<char16_t>(ascii[i])) {
      return false;
    }
  }
  return true;
}

// Parse MbString from buffer and convert to std::u16string
// Offset starts at beginning of mb string.
std::u16string ParseMbString(const uint8_t *buf, unsigned &offset, unsigned maxLen) {
  std::u16string result;
  if (offset + 4 > maxLen)
    return result;

  int32_t size = ToInt32(buf + offset);
  offset += 4;

  if (size < 0 || size > 10000 || offset + size * 2 > maxLen)
    return result;

  result.reserve(size);
  for (int i = 0; i < size; i++) {
    char16_t wchar = static_cast<char16_t>(ToInt16(buf + offset));
    offset += 2;
    result.push_back(wchar);
  }
  return result;
}

// Parse StringPropertyAtom and return the std::u16string value
// bytes start at element header.
std::u16string ParseStringPropertyAtom(const uint8_t *buf, unsigned offset, unsigned bufLen) {
  // Skip element header (25 bytes)
  unsigned localOffset = offset + DataElement::BYTES;
  if (localOffset >= bufLen)
    return std::u16string();
  // base property version + state flags U32 + string prop version U8.
  localOffset += 6;
  return ParseMbString(buf, localOffset, bufLen);
}

JTHeader ReadHeader(std::istream &in) {
  JTHeader header;
  in.read(header.version.data(), JTHeader::VERSION_LEN);
  in.read((char *)(&header.byteOrder), 1);
  in.read((char *)(&header.emptyField), sizeof(header.emptyField));
  in.read((char *)(&header.TOCOffset), sizeof(header.TOCOffset));
  in.read((char *)(&header.LSGSegId), sizeof(header.LSGSegId));
  return header;
}

TOCEntry ReadTOCEntry(std::istream &in) {
  TOCEntry entry;
  in.read((char *)(&entry.segId), sizeof(entry.segId));
  in.read((char *)(&entry.segOffset), sizeof(entry.segOffset));
  in.read((char *)(&entry.len), sizeof(entry.len));
  in.read((char *)(&entry.attr), sizeof(entry.attr));
  entry.type = entry.GetType();
  return entry;
}

DataSegment LoadDataSegment(std::istream &in, const TOCEntry &entry) {
  DataSegment seg;
  size_t offset = entry.segOffset;
  in.seekg(offset);
  in.read((char *)(&seg.segId), sizeof(seg.segId));
  in.read((char *)(&seg.type), sizeof(seg.type));
  in.read((char *)(&seg.len), sizeof(seg.len));
  if (seg.len > JT::DATA_SEGMENT_SIZE_LIMIT) {
    // invalid segment with empty data
    return seg;
  }
  seg.data.resize(seg.len);
  in.read((char *)(seg.data.data()), seg.len);
  return seg;
}

CompressionHeader ParseCompressionHeader(const DataSegment &seg) {
  CompressionHeader h;
  if (seg.data.size() < 9) {
    return h;
  }
  h.compressionFlag = ToUint32(seg.data.data());
  h.compressedLen = ToInt32(seg.data.data() + 4);
  h.compressionAlgorithm = seg.data[8];
  return h;
}


void ParseElementHeader(const uint8_t *buf, DataElement &ele) {
  ele.elementLength = ToInt32(buf);
  ReadGUID(buf + 4, ele.objectType);
  if (ele.elementLength >= 21) {
    // endOfElements do not have object id or base type .
    ele.baseType = buf[20];
    ele.objectId = ToInt32(buf + 21);
  }
}

bool ParseLateLoadedPropertyAtom(const uint8_t *buf,
                                 unsigned offset,
                                 unsigned bufLen,
                                 JT::LateLoadedPropertyAtom &atom) {
  unsigned atomOffset = offset + DataElement::BYTES;
  if (atomOffset + 6 > bufLen) {
    return false;
  }
  atomOffset += 6;
  if (atomOffset + 28 > bufLen) {
    return false;
  }

  ReadGUID(buf + atomOffset, atom.segId);
  atomOffset += 16;

  atom.segType = ToInt32(buf + atomOffset);
  atomOffset += 4;

  atom.payloadId = ToInt32(buf + atomOffset);
  return true;
}

// Parse node data after element header. Returns false if parsing fails.
bool ParseNodeInfo(const uint8_t *buf, unsigned offset, unsigned dataLen, JT::ObjectType nodeType, NodeInfo &info) {
  // @TODO not quite right.
  if (!JT::NodeTypeHasChildren(nodeType) && nodeType != JT::ObjectType::BaseNode && !IsShapeNodeType(nodeType)) {
    return true; // Not a node type, but not an error
  }

  if (offset + DataElement::BYTES > dataLen) {
    return false;
  }

  const uint8_t *nodeData = buf + offset;
  unsigned localOffset = DataElement::BYTES; // Skip element header

  // Base Node Data: Version + Node Flags + Attribute Count + Attribute IDs
  if (offset + localOffset + 9 > dataLen)
    return false;

  info.group.base.version = nodeData[localOffset];
  localOffset += 1;

  info.group.base.flags = ToUint32(nodeData + localOffset);
  localOffset += 4;

  info.group.base.attributeCount = ToInt32(nodeData + localOffset);
  localOffset += 4;

  if (info.group.base.attributeCount < 0 || info.group.base.attributeCount > 10000)
    return false;

  // Read attribute IDs
  info.group.base.attributeIds.resize(info.group.base.attributeCount);
  for (int i = 0; i < info.group.base.attributeCount; i++) {
    if (offset + localOffset + 4 > dataLen)
      return false;
    info.group.base.attributeIds[i] = ToInt32(nodeData + localOffset);
    localOffset += 4;
  }

  // Parse children based on node type
  if (nodeType == JT::ObjectType::InstanceNode) {
    // Instance Node: Version + Single Child ID
    if (offset + localOffset + 5 > dataLen)
      return false;
    localOffset += 1; // Skip instance version
    int32_t childId = ToInt32(nodeData + localOffset);
    info.group.childIds.push_back(childId);
    return true;
  }

  if (NodeTypeHasChildren(nodeType) && nodeType != JT::ObjectType::InstanceNode) {
    // Group Node Data: Version + Child Count + Child IDs
    if (offset + localOffset + 5 > dataLen)
      return false;
    localOffset += 1; // Skip group version

    int32_t childCount = ToInt32(nodeData + localOffset);
    localOffset += 4;

    if (childCount < 0 || childCount > 100000)
      return false;

    info.group.childIds.resize(childCount);
    for (int i = 0; i < childCount; i++) {
      if (offset + localOffset + 4 > dataLen)
        return false;
      info.group.childIds[i] = ToInt32(nodeData + localOffset);
      localOffset += 4;
    }
  }

  // Parse Shape Node specific data
  if (IsShapeNodeType(nodeType)) {
    // After Base Node Data comes: Version (U8) + BBox (24 bytes F32x6) + Area (F32) + ...
    // Then Vertex Shape Data: Version (U8) + Vertex Bindings (U64) + Shape LOD Element Count (I32) + LOD Object IDs
    // Skip to after group version if applicable
    if (NodeTypeHasChildren(nodeType)) {
      // Already parsed children, localOffset is after them
    }

    // Base Shape Data: Version + BBox + Area + Vertex Count Range + Node Count Range
    // Skip these for now and look for Shape LOD references
    if (offset + localOffset + 1 + 24 + 4 > dataLen)
      return true;
    localOffset += 1;  // Version
    localOffset += 24; // BBox (6 F32s)
    localOffset += 4;  // Area

    // Vertex Count Range: LOD Count + 2*LOD Count I32s (min/max per LOD)
    if (offset + localOffset + 4 > dataLen)
      return true;
    int32_t lodCount = ToInt32(nodeData + localOffset);
    localOffset += 4;
    if (lodCount < 0 || lodCount > 100)
      return true;
    localOffset += lodCount * 8; // min/max pairs

    // Node Count Range: similar structure
    if (offset + localOffset + 4 > dataLen)
      return true;
    int32_t nodeRangeCount = ToInt32(nodeData + localOffset);
    localOffset += 4;
    if (nodeRangeCount < 0 || nodeRangeCount > 100)
      return true;
    localOffset += nodeRangeCount * 8;

    // Now for Vertex Shape Node: Version + Vertex Bindings + Shape LOD Element Count + Object IDs
    if (nodeType == JT::ObjectType::VertexShapeNode) {
      if (offset + localOffset + 1 + 8 + 4 > dataLen)
        return true;
      localOffset += 1; // Version
      localOffset += 8; // Vertex Bindings (U64)

      int32_t shapeLODCount = ToInt32(nodeData + localOffset);
      localOffset += 4;

      if (shapeLODCount > 0 && shapeLODCount < 100) {
        for (int i = 0; i < shapeLODCount; i++) {
          if (offset + localOffset + 4 <= dataLen) {
            int32_t lodObjectId = ToInt32(nodeData + localOffset);
            localOffset += 4;
            // Store this as a reference - it's an object ID in the same segment
            // We'll need to look it up later to get the actual GUID if needed
            if (lodObjectId > 0) {
              // For now, just mark that this shape node has LOD references
              // The actual LOD elements should be in this same segment
            }
          }
        }
      }
    }
  }

  return true;
}

// Helper to convert u16 to utf8
std::string to_utf8(const std::u16string &u16) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  return convert.to_bytes(u16);
}

void SceneGraph::PrintHierarchy(std::ostream &out) {
  // Per JT 10.0 spec section 6: "The first Graph Element in a LSG Segment
  // should always be a Partition Node" - this is the root
  if (nodesMap.empty()) {
    out << "Empty scene graph\n";
    return;
  }

  if (segmentData == nullptr) {
    out << "Error: No segment data available\n";
    return;
  }

  const uint8_t *buf = segmentData->data.data();
  unsigned bufLen = segmentData->data.size();

  // Find root node - the first element at offset 0 (should be Partition Node)
  if (bufLen < DataElement::BYTES) {
    out << "Error: Segment data too small\n";
    return;
  }

  DataElement rootEle;
  ParseElementHeader(buf, rootEle);
  int rootId = rootEle.objectId;

  JT::ObjectType rootType = JT::GetObjectType(rootEle.objectType);

  // Traverse hierarchy starting from root
  std::function<void(int, int, const std::string &)> printNode;
  printNode = [&](int nodeId, int depth, const std::string &indent) {
    auto it = nodesMap.find(nodeId);
    if (it == nodesMap.end()) {
      return;
    }

    unsigned offset = it->second;
    DataElement ele;
    ParseElementHeader(buf + offset, ele);
    JT::ObjectType objType = JT::GetObjectType(ele.objectType);

    // Parse node data and children
    NodeInfo nodeInfo;
    ParseNodeInfo(buf, offset, bufLen, objType, nodeInfo);

    // attributes are material lighting texture
    for (int attrId : nodeInfo.group.base.attributeIds) {
      // attributes are lumped into nodes map for the file we are given.
      auto attrIt = nodesMap.find(attrId);
      if (attrIt != nodesMap.end()) {
        unsigned attrOffset = attrIt->second;
        DataElement attrEle;
        ParseElementHeader(buf + attrOffset, attrEle);
        JT::ObjectType attrType = JT::GetObjectType(attrEle.objectType);
        // std::cout<<"attr type "<<JT::ObjectTypeToString(attrType)<<"\n";
        // @TODO do stuff if needed
      }
    }

    out << indent << "{\n";

    // Look up node properties from property table
    auto propIt = propertyTable.table.find(nodeId);
    bool first = true;
    std::map<std::string, std::string> props;
    if (propIt != propertyTable.table.end()) {

      // Search for CAD_PART_NAME and JT_PROP_NAME properties
      for (const auto &[keyId, valueId] : propIt->second) {
        auto keyAtomIt = propertyAtomMap.find(keyId);
        if (keyAtomIt == propertyAtomMap.end()) {
          continue;
        }
        std::u16string keyName = ParseStringPropertyAtom(buf, keyAtomIt->second, bufLen);
        DataElement propEle;
        auto valueAtomIt = propertyAtomMap.find(valueId);
        if (valueAtomIt != propertyAtomMap.end()) {
          ParseElementHeader(buf + valueAtomIt->second, propEle);
          if (JT::GetObjectType(propEle.objectType) == JT::ObjectType::StringPropertyAtom) {
            std::u16string stringProp = ParseStringPropertyAtom(buf, valueAtomIt->second, bufLen);
            props[to_utf8(keyName)] = to_utf8(stringProp);
          }
        }
      }
    }
    std::set<std::string> excludeKeys = {"JT_PMI_CONTENT_HINT",
                                         "JT_PROP_MEASUREMENT_UNITS",
                                         "JT_PROP_MEASUREMENT_UNITS::",
                                         "ud_PDM_DOC_PART::",
                                         "ud_PDM_INST_KOGR::",
                                         "__PLM_LEAFCOMPONENT",
                                         "ud_PDM_INST_PNGUID::",
                                         "ud_PDM_COMMENT::",
                                         "ud_PDM_AI::",
                                         "ud_PDM_DOC_TYPE::",
                                         "ud_PDM_ZI::",
                                         "ud_PDM_ALTERNATIVE::",
                                         "ud_PDM_DOC_MATURITY::",
                                         "ud_PDM_DOC_FORMAT::",
                                         "ud_PDM_STATUS::",
                                         "ud_PDM_DOC_ID::",
                                         "Angular",
                                         "CAD_CENTER_OF_GRAVITY::",
                                         "CAD_MASS::",
                                         "CAD_MASS_UNITS::",
                                         "CAD_MOMENT_OF_INERTIA::",
                                         "CAD_SURFACE_AREA::",
                                         "CAD_VOLUME::",
                                         "Chordal",
                                         "JT_PROP_ORIGINATING_BREPTYPE",
                                         "LODLabel0::",
                                         "LODLabel1::",
                                         "LODLabel2::",
                                         "SegLength",
                                         "_nTrisLODs"};
    for (auto const &[key, val] : props) {
      if (excludeKeys.contains(key)) {
        continue;
      }
      if (!first) {
        out << ",\n";
      }
      first = false;
      out << indent << "\"" << key << "\": \"" << val << "\"";
    }

    if (!first) {
      out << ",\n";
    }
    if (nodeInfo.group.childIds.size() > 0) {
      out << indent << "\"children\":[\n";
      bool firstChild = true;
      // Traverse children
      for (int childId : nodeInfo.group.childIds) {
        if (!firstChild) {
          out << ",\n";
        }
        firstChild = false;
        printNode(childId, depth + 1, indent + "  ");
      }
      out << "\n" << indent << "]";
    }

    out << "\n" << indent << "}";
  };

  printNode(rootId, 0, "");
}

// split data segment into elements for each object.
// stops
// seg must be decompressed already.
// elements: output map from object id to byte offset in segment
// endMarkerOffset: offset to EndOfElements element
// in segment. there could be other data after elements section.

/// @TODO switch back to data segment for first arg, use a starting offset for 2nd arg.
int MapElements(const DataSegment &seg,
                size_t startOffset,
                std::unordered_map<int, unsigned> &elements,
                unsigned &endMarkerOffset) {
  // later combined to a map from objectId to offset into this segment's decompressed buffer.
  std::vector<std::pair<int, unsigned>> elementOffsets;
  size_t bufOffset = startOffset;
  size_t bufLen = seg.data.size();
  const uint8_t *buf = seg.data.data();
  int ret = 0;
  while (bufOffset < bufLen) {
    size_t elementOffset = bufOffset;
    DataElement ele;
    // @TODO: no need to store data elements in a list.
    ParseElementHeader(buf + bufOffset, ele);
    size_t remainingBytes = bufLen - bufOffset;
    if (ele.elementLength > remainingBytes) {
      // went past end of elements
      ret = -1;
      break;
    }
    if (ele.elementLength == 0) {
      // corrupted.
      ret = -2;
      break;
    }
    JT::ObjectType objType = JT::GetObjectType(ele.objectType);
    if (objType == JT::ObjectType::EndOfElements) {
      endMarkerOffset = bufOffset;
      // end marker is not added to object map
      break;
    }
    if (objType != JT::ObjectType::Unknown) {
      elementOffsets.push_back({ele.objectId, unsigned(bufOffset)});
    }
    // 4 bytes of size + actual length
    bufOffset += 4 + ele.elementLength;
  }
  elements = std::unordered_map<int, unsigned>();
  elements.reserve(elementOffsets.size());
  for (const auto &[key, value] : elementOffsets) {
    elements.emplace(key, value);
  }
  return ret;
}

// check for compression.
// if compression is used, uncompress.
int Decompress(DataSegment &seg) {
  bool compressionSupported = JT::IsCompressionSupported(seg.type);
  CompressionHeader h;
  bool compressionUsed = false;

  if (compressionSupported) {
    if (seg.data.size() < 9) {
      // corrupted.
      return -1;
    }
    h = ParseCompressionHeader(seg);
    compressionUsed = h.compressionFlag == 3 && h.compressionAlgorithm == 3;
  }
  if (compressionUsed) {
    if (h.compressedLen + 9 > seg.data.size()) {
      // unexpected. compressed data larger than segment size.
      return -2;
    }
    const unsigned payloadOffset = 9;
    size_t len = size_t(h.compressedLen);
    std::vector<uint8_t> decomp;
    int ret = DecompressLZMA(seg.data.data() + payloadOffset, len, decomp);
    if (ret == 0) {
      seg.data = decomp;
    } else {
      return ret;
    }
  }

  return 0;
}

int ParsePropertyTable(const DataSegment &seg, unsigned startByte, PropertyTable &tab) {
  const uint8_t *buf = seg.data.data() + startByte;
  size_t localOffset = 0;
  tab.version = ToInt16(buf);
  tab.size = ToInt32(buf + 2);
  localOffset += 6;
  size_t tableLen = seg.data.size() - startByte;

  for (size_t i = 0; i < tab.size; i++) {
    int elementId = ToInt32(buf + localOffset);
    localOffset += 4;
    int keyId = ToInt32(buf + localOffset);
    if (keyId == 0) {
      localOffset += 4;
      continue;
    }
    PropertyTable::ElementProperties properties;
    while (keyId != 0) {
      keyId = ToInt32(buf + localOffset);
      localOffset += 4;
      if (keyId == 0) {
        break;
      }
      int valueId = ToInt32(buf + localOffset);
      properties.push_back({keyId, valueId});
      localOffset += 4;
    }
    tab.table[elementId] = properties;
  }
  return 0;
}

int ParseScene(const DataSegment &seg, SceneGraph &scene) {
  // Store reference to segment data for hierarchy traversal
  scene.segmentData = &seg;

  unsigned endOffset = 0;
  unsigned bufLen = seg.data.size();
  int ret = MapElements(seg, 0, scene.nodesMap, endOffset);
  DataElement ele;
  ParseElementHeader(seg.data.data() + endOffset, ele);
  auto type = GetObjectType(ele.objectType);
  std::cout << "final ele type " << ObjectTypeToString(type) << "\n";
  scene.endOfElementOffset = endOffset + 4 + ele.elementLength;
  unsigned propertyEnd = 0;
  ret = MapElements(seg, scene.endOfElementOffset, scene.propertyAtomMap, propertyEnd);

  ParseElementHeader(seg.data.data() + propertyEnd, ele);
  scene.endOfPropertiesOffset = propertyEnd + ele.elementLength + 4;
  ParsePropertyTable(seg, scene.endOfPropertiesOffset, scene.propertyTable);
  return 0;
}

// debugging helper
void LoadSegmentFromDisk(const std::string &segBinFile, DataSegment &seg) {
  std::ifstream file(segBinFile, std::ios::binary | std::ios::ate);
  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  // 2. Pre-allocate the vector to avoid reallocations
  seg.data.resize(size);

  // 3. Read the entire file into the vector's underlying data
  file.read(reinterpret_cast<char *>(seg.data.data()), size);
}

void DebugLSGLoading() {
  DataSegment seg;
  LoadSegmentFromDisk("I:/foundation/b/seg.bin", seg);
  SceneGraph scene;
  ParseScene(seg, scene);

  std::ofstream debugOut("I:/foundation/b/hier.txt");
  scene.PrintHierarchy(debugOut);
}

// debugging function
void ExportShapeSegment(const JTFile & jtFile, TOCEntry & entry){
  std::string outDir = "/media/desaic/ssd2/data/b/out/";
  DataSegment seg = LoadDataSegment(jtFile.file, entry);
  std::cout << seg.len <<"\n";
  if(seg.type <7 || seg.type > 12){
    //not a shape lod
    return;
  }
  
}

void visitShapeNodes(const SceneGraph &scene,
                     int nodeId,
                     int depth,
                     const JTFile &jtFile,
                     std::ostream &out) {
  auto it = scene.nodesMap.find(nodeId);
  if (it == scene.nodesMap.end()) {
    return;
  }

  const uint8_t *buf = scene.segmentData->data.data();
  unsigned bufLen = scene.segmentData->data.size();

  unsigned offset = it->second;
  DataElement ele;
  ParseElementHeader(buf + offset, ele);
  JT::ObjectType objType = JT::GetObjectType(ele.objectType);

  // Check if this is a TriStripSetShapeNode
  if (objType == JT::ObjectType::TriStripSetShapeNode || objType == JT::ObjectType::PartNode) {
    out << "Found TriStripSetShapeNode:\n";
    out << "  Object ID: " << nodeId << "\n";

    // Look up node name and LOD references from property table
    auto propIt = scene.propertyTable.table.find(nodeId);
    if (propIt != scene.propertyTable.table.end()) {
      for (const auto &[keyId, valueId] : propIt->second) {
        auto keyAtomIt = scene.propertyAtomMap.find(keyId);
        if (keyAtomIt == scene.propertyAtomMap.end()) {
          continue;
        }
        unsigned keyOffset = keyAtomIt->second;
        DataElement keyEle;
        ParseElementHeader(buf + keyOffset, keyEle);
        JT::ObjectType keyType = JT::GetObjectType(keyEle.objectType);

        if (keyType != JT::ObjectType::StringPropertyAtom) {
          continue;
        }
        std::u16string keyName = ParseStringPropertyAtom(buf, keyOffset, bufLen);
        auto valueAtomIt = scene.propertyAtomMap.find(valueId);
        if (valueAtomIt == scene.propertyAtomMap.end()) {
          continue;
        }
        DataElement valEle;
        ParseElementHeader(buf + valueAtomIt->second, valEle);
        JT::ObjectType valType = JT::GetObjectType(valEle.objectType);
        if (valType == JT::ObjectType::StringPropertyAtom) {
          std::u16string valueStr = ParseStringPropertyAtom(buf, valueAtomIt->second, bufLen);
          out << to_utf8(keyName) << ": " << to_utf8(valueStr) << "\n";
        } else if (valType == JT::ObjectType::LateLoadedPropertyAtom) {
          JT::LateLoadedPropertyAtom atom;
          if (!ParseLateLoadedPropertyAtom(buf, valueAtomIt->second, bufLen, atom)) {
            continue;
          }

          out << "  Late Loaded Property:\n";
          out << "    Segment Type: " << JT::SegmentTypeToString(JT::SegmentType(atom.segType)) << "\n";
          out << "    Payload Object ID: " << atom.payloadId << "\n";

          // Look up segment in GUID map
          auto entry = jtFile.GetEntry(atom.segId);
          if (entry.valid()) {
            ExportShapeSegment(jtFile, entry);
          } else {
            out << "    WARNING: Segment GUID not found in TOC!\n";
          }
        }
      }
      out << "\n";
    }
  }
  // Recursively visit children
  NodeInfo nodeInfo;
  if (ParseNodeInfo(buf, offset, bufLen, objType, nodeInfo)) {
    for (int childId : nodeInfo.group.childIds) {
      visitShapeNodes(scene, childId, depth + 1, jtFile, out);
    }
  }
}

// Walk through scene graph and identify all Tri-Strip Set Shape Nodes
// Write information about mesh location to output file
void IdentifyShapes(const SceneGraph &scene,
                    const JTFile & jtFile,
                    const std::string &outputFile) {
  if (scene.segmentData == nullptr) {
    std::cout << "Error: No segment data available\n";
    return;
  }

  const uint8_t *buf = scene.segmentData->data.data();
  unsigned bufLen = scene.segmentData->data.size();

  std::ofstream out(outputFile);
  if (!out.is_open()) {
    std::cout << "Error: Could not open output file " << outputFile << "\n";
    return;
  }

  // Start traversal from the root (first element)
  if (bufLen >= DataElement::BYTES) {
    DataElement rootEle;
    ParseElementHeader(buf, rootEle);
    visitShapeNodes(scene, rootEle.objectId, 0, jtFile, out);
  }

  out.close();
  std::cout << "Shape information written to " << outputFile << "\n";
}

int main(int argc, char *argv[]) {
  //   DebugLSGLoading();
  //   return 0;
  // if (argc <= 1) {
  //	return -1;
  // }
  // std::string input = argv[1];
  // std::string input = "I:/foundation/b/S.jt";
  std::string input = "/media/desaic/ssd2/data/b/S.jt";
  // std::string outputDir = "I:/foundation/b/out/";
  std::string outputDir = "/media/desaic/ssd2/data/b/out/";
  std::string sceneCache = "/media/desaic/ssd2/data/b/scene_seg.bin";
  if (argc > 2) {
    outputDir = argv[2];
  }

  std::ifstream inFile(input, std::ifstream::binary);
  JTFile jtFile(inFile);
  jtFile.header = ReadHeader(inFile);
  uint32_t TOCCount = 0;

  inFile.seekg(jtFile.header.TOCOffset);
  inFile.read((char *)(&TOCCount), sizeof(TOCCount));
  size_t SANITY_SIZE = 10000000;
  if (TOCCount > SANITY_SIZE) {
    std::cout << "too many TOC entries. maybe unsupported file format.\n";
    return -1;
  }
  jtFile.TOCs.resize(TOCCount);
  for (size_t i = 0; i < jtFile.TOCs.size(); i++) {
    jtFile.TOCs[i] = ReadTOCEntry(inFile);
  }
  jtFile.BuildTOCMap();
  TOCEntry sceneRoot = jtFile.GetEntry(jtFile.header.LSGSegId);
  if (sceneRoot.len == 0) {
    std::cout << "Error: cannot find data for scene graph root.\n";
    return -2;
  }
  if (JT::SegmentType(sceneRoot.type) != JT::SegmentType::LogicalSceneGraph) {
    std::cout << "wrong type for scene root.\n";
    return -3;
  }

  DataSegment sceneData;
  SceneGraph scene;
  bool debugging = true;
  if (debugging) {
    LoadSegmentFromDisk(sceneCache, sceneData);
  } else {
    sceneData = LoadDataSegment(inFile, sceneRoot);
    Decompress(sceneData);
    bool cacheSceneGraph = false;
    if (cacheSceneGraph) {
      std::ofstream sceneOut;
      sceneOut.open(sceneCache, std::ofstream::binary);
      sceneOut.write((const char *)sceneData.data.data(), sceneData.data.size());
    }
  }
  ParseScene(sceneData, scene);
  // Identify all shapes and write mesh location info
  IdentifyShapes(scene, jtFile, outputDir + "shapes.txt");

  return 0;
}
