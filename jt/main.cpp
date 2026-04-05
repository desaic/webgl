#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <map>
#include "JT_defs.h"
#include "lzma_wrapper.h"

struct GUID {
	std::array<uint8_t, 16> bytes = {};
	
	bool operator<(const GUID& other) const {
		return bytes < other.bytes;
	}
};

// cannot read as hole because JT file design 
// did not consider data alignment requirements
struct JTHeader {
	static const unsigned VERSION_LEN = 80;
	char version[VERSION_LEN];
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
	in.read(header.version, JTHeader::VERSION_LEN);	
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
	uint32_t len;
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
	if (seg.len > DATA_SEGMENT_SIZE_LIMIT) {
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

CompressionHeader ParseCompressionHeader(const DataSegment& seg) {
	CompressionHeader h;
	if (seg.data.size() < 9) {
		return h;
	}
	h.compressionFlag = ToUint32(seg.data.data());
	h.compressedLen = int(ToUint32(seg.data.data() + 4));
	h.compressionAlgorithm = seg.data[8];
	return h;
}

// split data segment into elements for each object.
// after splitting the data segment's data can be deleted to save RAM.
// seg must be decompressed already.
std::vector<DataElement> ParseElements(const DataSegment & seg) {

}

// check for compression. 
// if compression is used, uncompress.
int Decompress(DataSegment&seg){
	bool compressionSupported = IsCompressionSupported(seg.type);
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
	if (SegmentType(sceneRoot.type) != SegmentType::LogicalSceneGraph) {
		std::cout << "wrong type for scene root.\n";
		return -3;
	}

	auto sceneData = LoadDataSegment(inFile, sceneRoot);
	Decompress(sceneData);	

	return 0;
}
