#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <map>
#include <iomanip>
#include <unordered_map>

#include "JT_defs.h"
#include "lzma_wrapper.h"

using namespace JT;

// cannot read as hole because JT file design 
// did not consider data alignment requirements
struct JTHeader {
	static const unsigned VERSION_LEN = 80;
	std::array<char, VERSION_LEN> version = {};
	// only 0 little endian is handled.
	uint8_t byteOrder = 0;
	int32_t emptyField = 0;
	//byte offset from the top of the file to the start of the TOC Segment
	uint64_t TOCOffset = 0;
	// data segment ID for logical scene graph, i.e. root.
	GUID LSGSegId;
};

struct MbString {
	int size= 0;
	std::vector<uint16_t> chars;
};

JTHeader ReadHeader(std::istream& in) {
	JTHeader header;
	in.read(header.version.data(), JTHeader::VERSION_LEN);	
	in.read((char*)(&header.byteOrder), 1);
	in.read((char*)(&header.emptyField), sizeof(header.emptyField));
	in.read((char*)(&header.TOCOffset), sizeof(header.TOCOffset));
	in.read((char*)(&header.LSGSegId), sizeof(header.LSGSegId));
	return header;
}

struct TOCEntry {
	GUID segId;
	uint64_t segOffset= 0;
	uint32_t len = 0;
	uint32_t attr = 0;
	uint32_t type = 0;
	//bits 24-31 of attr.
	uint32_t GetType() const {
		return attr >> 24;
	}
};

TOCEntry ReadTOCEntry(std::istream& in) {
	TOCEntry entry;
	in.read((char*)(&entry.segId), sizeof(entry.segId));
	in.read((char*)(&entry.segOffset), sizeof(entry.segOffset));
	in.read((char*)(&entry.len), sizeof(entry.len));
	in.read((char*)(&entry.attr), sizeof(entry.attr));
	entry.type = entry.GetType();
	return entry;
}

struct DataSegment {
	GUID segId;
	uint32_t type= 0;
	uint32_t len = 0;
	std::vector<uint8_t> data;

	// map from object id to data offset.
	std::unordered_map<int, unsigned> nodesMap;
	std::unordered_map<int, unsigned> propertyAtomMap;
	
	// offset for "endOfElement" element.
	// there could be stuff after end of elements.
	unsigned endOfElementOffset = 0;;
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
	std::vector<uint8_t> data;
};

struct JTFile {
	JTHeader header;
	std::vector<TOCEntry> TOCs;
	// from GUID to TOC entry index into TOCs list.
	std::map<GUID, size_t> TOCMap;
	void BuildTOCMap();
	TOCEntry GetEntry(const GUID& guid)const {
		auto it = TOCMap.find(guid);
		if (it != TOCMap.end()) {
			return TOCs[it->second];
		}
		else {
			// invalid entry with 0 length
			return TOCEntry();
		}
	}
};

void JTFile::BuildTOCMap() {
	for (size_t i = 0; i < TOCs.size(); i++) {
		TOCMap[TOCs[i].segId] = i;
	}
}

DataSegment LoadDataSegment(std::istream& in, const TOCEntry& entry) {
	DataSegment seg;
	size_t offset = entry.segOffset;
	in.seekg(offset);
	in.read((char*)(&seg.segId), sizeof(seg.segId));
	in.read((char*)(&seg.type), sizeof(seg.type));
	in.read((char*)(&seg.len), sizeof(seg.len));
	if (seg.len > JT::DATA_SEGMENT_SIZE_LIMIT) {
		// invalid segment with empty data
		return seg;
	}
	seg.data.resize(seg.len);
	in.read((char*)(seg.data.data()), seg.len);
	return seg;
}

static uint32_t ToUint32(const uint8_t* bytes) {
	// Little Endian
	return static_cast<uint32_t>(bytes[0]) |
		(static_cast<uint32_t>(bytes[1]) << 8) |
		(static_cast<uint32_t>(bytes[2]) << 16) |
		(static_cast<uint32_t>(bytes[3]) << 24);
}

static int32_t ToInt32(const uint8_t* bytes) {
	// Little Endian
	return static_cast<int32_t>(bytes[0]) |
		(static_cast<int32_t>(bytes[1]) << 8) |
		(static_cast<int32_t>(bytes[2]) << 16) |
		(static_cast<int32_t>(bytes[3]) << 24);
}

static void ReadGUID(const uint8_t* buf, GUID& guid) {
	std::memcpy(guid.bytes.data(), buf, guid.bytes.size());
}

CompressionHeader ParseCompressionHeader(const DataSegment& seg) {
	CompressionHeader h;
	if (seg.data.size() < 9) {
		return h;
	}
	h.compressionFlag = ToUint32(seg.data.data());
	h.compressedLen = ToInt32(seg.data.data() + 4);
	h.compressionAlgorithm = seg.data[8];
	return h;
}

// LSG
struct SceneGraph
{
	std::vector<DataElement> nodes;
};

void ParseElementHeader(const uint8_t* buf, DataElement& ele) {
	ele.elementLength = ToInt32(buf);
	ReadGUID(buf + 4 , ele.objectType);
	if (ele.elementLength >= 21) {
		// endOfElements do not have object id or base type .
		ele.baseType = buf[20];
		ele.objectId = ToInt32(buf + 21);
	}

}

// split data segment into elements for each object.
// stops 
// seg must be decompressed already.
// elements: output map from object id to byte offset in segment
// endMarkerStart: offset to EndOfElements element
// in segment. there could be other data after elements section.
int MapElements(const uint8_t * buf, size_t bufLen, std::unordered_map<int, unsigned> & elements, unsigned & endMarkerOffset) {
	//later combined to a map from objectId to offset into this segment's decompressed buffer.
	std::vector<std::pair<int, unsigned> > elementOffsets;
	size_t bufOffset = 0;	
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
		  //corrupted.
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
			elementOffsets.push_back({ ele.objectId, bufOffset });
		}
		// 4 bytes of size + actual length
		bufOffset += 4 + ele.elementLength;
	}
	elements = std::unordered_map<int, unsigned>();
	elements.reserve(elementOffsets.size());
	for (const auto& [key, value] : elementOffsets) {
		elements.emplace(key, value);
	}
	return ret;
}

// check for compression. 
// if compression is used, uncompress.
int Decompress(DataSegment&seg){
	bool compressionSupported = JT::IsCompressionSupported(seg.type);
	CompressionHeader h;
	bool compressionUsed = false;
	
	if (compressionSupported) {
		if (seg.data.size() < 9) {
			//corrupted.
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
		}
		else {
			return ret;
		}
	}
	
	return 0;
}


int main(int argc, char * argv[]){
	//if (argc <= 1) {
	//	return -1;
	//}
	//std::string input = argv[1];
	std::string input = "I:/foundation/b/S.jt";
	std::string outputDir = "./";
	if (argc > 2) {
		outputDir = argv[2];
	}

	std::ifstream inFile(input,std::ifstream::binary);
	JTFile jtFile;
	jtFile.header = ReadHeader(inFile);
	uint32_t TOCCount = 0;

	inFile.seekg(jtFile.header.TOCOffset);
	inFile.read((char*)(&TOCCount), sizeof(TOCCount));
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

	auto sceneData = LoadDataSegment(inFile, sceneRoot);
	Decompress(sceneData);
	unsigned endOffset = 0;
	int ret = MapElements(sceneData, sceneData.objectMap, endOffset);	
	DataElement ele;
	ParseElementHeader(sceneData.data.data() + endOffset, ele);
	auto type = GetObjectType(ele.objectType);
	std::cout << "final ele type " << ObjectTypeToString(type)<<"\n";
	sceneData.endOfElementOffset = endOffset + 4 + ele.elementLength;
	std::vector<uint8_t> lastEle;
	lastEle.insert(lastEle.begin(), sceneData.data.begin() + endOffset, sceneData.data.begin() + endOffset + 8 + ele.elementLength);
	std::vector<uint8_t> remainder;
	remainder.insert(remainder.begin(), sceneData.data.begin() + sceneData.endOfElementOffset,
		sceneData.data.begin() + sceneData.endOfElementOffset + 128);
	DataElement nextEle;
	ParseElementHeader(remainder.data(), nextEle);
	std::cout << ObjectTypeToString(GetObjectType(nextEle.objectType)) <<"\n";	
	return 0;
}
