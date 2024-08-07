
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"

#include "ImageIO.h"
#include "cad_app.h"
#include "CanvasApp.h"
#include "VoxApp.h"
#include "UIConf.h"
#include "UILib.h"
#include "TriangulateContour.h"

void VoxApp() {
  UILib ui;
  ConnectorVox app;
  app.Init(&ui);
  ui.SetFontsDir("./fonts");

  int statusLabel = ui.AddLabel("status");

  ui.Run();

  const unsigned PollFreqMs = 20;

  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

void TestCDT() {
  TrigMesh mesh;
  std::vector<float> v(8);
  mesh = TriangulateLoop(v.data(), v.size() / 2);
  mesh.SaveObj("F:/dump/loop_trigs.obj");
}

void CadApp() {
  UILib ui;
  cad_app app;
  app.Init(&ui);
  ui.SetFontsDir("./fonts");
  int statusLabel = ui.AddLabel("status");
  ui.Run();

  const unsigned PollFreqMs = 20;

  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

void MakeImageHtml(const std::string& dir) {
  std::string header =
      "<!DOCTYPE html>"
      "<html lang = \"en\">"
      "<head>"
      "<meta charset = \"UTF-8\">"
      "<meta name = \"viewport\" content =\"width=device-width, "
      "initial-scale=1.0\">"
      "<title> Offsets</title>"
      "</head>"
      "<body>"
      "<h1> Offsets</h1>"
      "<table><thead><tr>\n"
      "<th> Name</th>"
      "<th> Image</th>"
      "<th> dx</th>"
      "<th> dy</th>\n"
      "</tr>"
      "</thead>"
      "<tbody>";
  std::ofstream out(dir + "/index.html");

  std::string footer =
      "</tbody>"
      "</table>"
      "</body>"
      "</html>";
  out << header << "\n";

  std::ifstream in(dir + "/offsets.txt");
  std::vector<std::string> files;
  std::vector<Vec2i> offsets;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string file;
    iss >> file;
    Vec2i o;
    iss >> o[0] >> o[1];
    files.push_back(file);
    offsets.push_back(o);
  }
  for (size_t i = 0; i < files.size(); i++) {
    out << "<tr>\n";
    out << "<td>" << files[i] << "</td>\n";
    out << "<td><img src=";
    out << "\"./" + files[i] + "\"";
    out << "width=\"105\"";
    out << "height=\"230\"";
    out << "</td>\n";
    out << "<td>" << offsets[i][0] << "</td>\n";
    out << "<td>" << offsets[i][1] << "</td>\n";
    out << "</tr>\n";
  }
  out << footer << "\n";
}

void GenerateHtmls() {
  std::string dataRoot = "H:/ml/diamond_offset/data/";
  std::string folders[7] = {dataRoot + "v7_0724/",   
    dataRoot + "v7_1012_1/",
    dataRoot + "v7_1012_2/", 
    dataRoot + "v7_0213/",
    dataRoot + "v7_0213_2/", 
    dataRoot + "v8_0305/",
    dataRoot + "v8_0306/"};
  for (size_t i = 0; i < 7; i++) {
    MakeImageHtml(folders[i]);
  }
}

std::vector<std::vector<Vec2u> > ReadEdgeCoords(const std::string& file) {
  std::ifstream edgeIn(file);
  std::string line;
  std::vector<Vec2u> coords;
  while (std::getline(edgeIn, line)) {
    std::istringstream iss(line);
    Vec2u v;
    iss >> v[0] >> v[1];
    coords.push_back(v);
  }
  const size_t NUM_EDGES = 16;
  size_t edgeLen = coords.size() / NUM_EDGES;
  std::vector<std::vector<Vec2u> > edges(NUM_EDGES);
  size_t j = 0;
  for (size_t e = 0; e < edges.size(); e++) {
    edges[e].resize(edgeLen);
    for (size_t i = 0; i < edges[e].size(); i++) {
      edges[e][i] = coords[j];
      j++;
    }
  }
  return edges;
}
struct DiamondFeatures {
  /// <summary>
  /// valley depths. larger value for deeper valleys.
  /// </summary>
  std::array<std::vector<float>, 4> f;

  std::array<std::array<std::vector<Vec2u>, 4>, 4> edges;
  // profile for each edge
  // computed from depth pixels around edges.
  std::array<std::array<std::vector<float>, 4>, 4> edgeProf;

  void FlipOddCurves() {
    for (size_t d = 0; d < edgeProf.size(); d++) {
      for (size_t e = 1; e < edgeProf[d].size(); e += 2) {
        std::reverse(edgeProf[d][e].begin(), edgeProf[d][e].end());
      }
    }
  }
};

std::vector<float> averageCurve(unsigned featWidth,
                                const std::vector<Vec2u>& centers,
                                const Array2D8u& image) {
  std::vector<float>avg(featWidth, 0);
  Vec2u size = image.GetSize();
  for (size_t i = 0; i < centers.size(); i++) {
    for (int x = 0; x < int(featWidth); x++) {
      int srcx = x - int(featWidth / 2) + int(centers[i][0]);
      if (srcx < 0) {
        srcx = 0;
      }
      if (srcx >= int(size[0])) {
        srcx=size[0]-1;      
      }
      uint8_t val = image(unsigned(srcx), centers[i][1]);
      avg[x] += val;
    }
  }
  for (size_t i = 0; i < avg.size(); i++) {
    avg[i] /= centers.size();
  }
  return avg;
}

DiamondFeatures GetDiamondFeatures(
    const std::vector<std::vector<Vec2u> >& edgeCoords,
    const Array2D8u& image) {
  const size_t NUM_DIAMONDS = 4;
  const size_t EDGE_PER_DIAMOND = 4;
  const unsigned FEAT_WIDTH = 20;
  DiamondFeatures feat;
  for (size_t d = 0; d < NUM_DIAMONDS; d++) {
    for (size_t e = 0; e < EDGE_PER_DIAMOND; e++) {
      size_t i = d * EDGE_PER_DIAMOND + e;
      const std::vector<Vec2u>& coords = edgeCoords[i];
      feat.edges[d][e] = coords;
      feat.edgeProf[d][e] = averageCurve(FEAT_WIDTH, coords, image);
    }
  }
  feat.FlipOddCurves();
  return feat;
}

struct DataPoint {
  std::string name;
  std::vector<float> x;
  float y=0;
  std::string ToString() const { 
    std::ostringstream oss;
    oss << name << " ";
    oss << x.size() << " ";
    for (size_t i = 0; i < x.size(); i++) {
      oss << x[i] << " ";
    }
    oss << y;
    return oss.str();
  }
};

std::vector<std::string> split(const std::string& str, char delimiter) {
  std::vector<std::string> tokens;
  std::stringstream ss(str);
  std::string token;

  while (std::getline(ss, token, delimiter)) {
    tokens.push_back(token);
  }

  return tokens;
}

void MakeXData(const DiamondFeatures& feat, DataPoint& dp) {
  size_t width = feat.edgeProf[0][0].size();
  size_t numDiamonds = feat.edgeProf.size();
  size_t numEdges = feat.edgeProf[0].size();
  dp.x.resize(width * numDiamonds * numEdges, 0);
  unsigned j = 0;
  for (size_t d = 0; d < numDiamonds; d++) {
    for (size_t e = 0; e < numEdges; e++) {
      size_t i = d * numDiamonds + e;
      for (size_t i = 0; i < feat.edgeProf[d][e].size(); i++) {
        dp.x[j] = feat.edgeProf[d][e][i];
        j++;
      }
    }
  }
}
  
void MakeYData(const DiamondFeatures& feat, DataPoint& dp) {
  size_t width = feat.edgeProf[0][0].size();
  size_t numDiamonds = feat.edgeProf.size();
  size_t numEdges = feat.edgeProf[0].size();
  dp.x.resize(width * numDiamonds * numEdges, 0);
  unsigned j = 0;
  unsigned edgeIdx[4] = {0, 2, 1, 3};
  for (size_t d = 0; d < numDiamonds; d++) {
    for (size_t e = 0; e < numEdges; e++) {
      unsigned ei = edgeIdx[e];
      size_t i = d * numDiamonds + ei;
      for (size_t i = 0; i < feat.edgeProf[d][ei].size(); i++) {
        dp.x[j] = feat.edgeProf[d][ei][i];
        j++;
      }
    }
  }
}

void MakeXYPair(const std::string&dir) {
  std::ifstream offsetFile(dir + "/offsets.txt");
  std::vector<std::string> files;
  std::vector<Vec2i> offsets;
  std::string line;
  while (std::getline(offsetFile, line)) {
    std::istringstream iss(line);
    std::string file;
    Vec2i o;
    iss >> file >> o[0] >> o[1];
    files.push_back(file);
    offsets.push_back(o);
  }
  std::ofstream out(dir + "/train.txt");
  for (size_t i = 0; i < files.size(); i++) {
    std::string imagePath = dir + "/" + files[i];
    Array2D8u image;
    LoadPngGrey(imagePath, image);
    std::string prefix = files[i].substr(0, files[i].size() - 4);
    std::string edgeFile = dir + "/" + prefix + "_edge.txt";
    std::vector < std::vector<Vec2u> > edges = ReadEdgeCoords(edgeFile);
    DiamondFeatures feat = GetDiamondFeatures(edges, image);

    std::vector<std::string> tokens = split(dir, '/');

    DataPoint dp;
    dp.name = tokens.back() + "_x";
    MakeXData(feat, dp);
    dp.y = offsets[i][0];
    out << dp.ToString() << "\n";
    dp.name = tokens.back() + "_y";
    MakeYData(feat, dp);
    dp.y = offsets[i][1];
    out << dp.ToString() << "\n";
  }
}

void MakeXYPairs() {
  std::string dataRoot = "H:/ml/diamond_offset/data/";
  std::string folders[7] = {dataRoot + "v7_0213/",   dataRoot + "v7_0213_2/",
                            dataRoot + "v7_0724/",   dataRoot + "v7_1012_1/",
                            dataRoot + "v7_1012_2/", dataRoot + "v8_0305/",
                            dataRoot + "v8_0306/"};
  for (size_t i = 0; i < 7; i++) {
    MakeXYPair(folders[i]);
  }
}

void RunCanvasApp() {
  
    UILib ui;
    CanvasApp app;
    app.conf._confFile = "./canvas_conf.json";
    app.Init(&ui);
    ui.SetFontsDir("./fonts");

    int statusLabel = ui.AddLabel("status");

    ui.Run();

    const unsigned PollFreqMs = 20;

    while (ui.IsRunning()) {
      app.Refresh();
      std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
    }
  
}

int main(int argc, char**argv) {
  //MakeXYPairs();
    //RunCanvasApp();
    //return 0;
  if (argc > 1 && argv[1][0] == 'c') {
    CadApp();
  } else {
    VoxApp();
  }
  return 0;
}
