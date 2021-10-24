#include "TrigMesh.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#ifdef _WIN32
#define NOMINMAX //Stop errors with std::max
#include <windows.h>
#endif
#include <stdio.h>
#include <cstdlib>
#include <utility>
#include <map>
#include <sstream>
#include <string.h>
#include "FileUtil.hpp"

TrigMesh::Vector3s operator+(const TrigMesh::Vector3s & a, const TrigMesh::Vector3s & b)
{
	TrigMesh::Vector3s ret;
	ret[0] = a[0] + b[0];
	ret[1] = a[1] + b[1];
	ret[2] = a[2] + b[2];
	return ret;
}

TrigMesh::Vector3s operator-(const TrigMesh::Vector3s & a, const TrigMesh::Vector3s & b)
{
	TrigMesh::Vector3s ret;
	ret[0] = a[0] - b[0];
	ret[1] = a[1] - b[1];
	ret[2] = a[2] - b[2];
	return ret;
}

inline TrigMesh::Vector3s operator*(float a, const TrigMesh::Vector3s & b)
{
	TrigMesh::Vector3s ret;
	ret[0] = a - b[0];
	ret[1] = a - b[1];
	ret[2] = a - b[2];
	return ret;
}

inline TrigMesh::Vector3s cross(const TrigMesh::Vector3s & a, const TrigMesh::Vector3s & b)
{
	TrigMesh::Vector3s ret;
	ret[0] = a[1]*b[2] - b[1]*a[2];
	ret[1] = -a[0]*b[2] + b[0]*a[2];
	ret[2] = a[0]*b[1] - b[0]*a[1];
	return ret;
}

inline TrigMesh::Vector3s normalize(TrigMesh::Vector3s & a)
{
	float norm = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
	a[0] /= norm;
	a[1] /= norm;
	a[2] /= norm;
	return a;
}

//make a plane in [0,1]x[0,1].
void makePlane(TrigMesh * m, int nx, int ny)
{
	int nTrig = nx * ny * 2;
	int nVert = (nx + 1)*(ny + 1);
	m->v.resize(nVert);
	m->t.resize(nTrig);
	//make verts.
	int cnt = 0;
	for (int i = 0; i <= nx; i++) {
		for (int j = 0; j <= ny; j++) {
			m->v[cnt] = TrigMesh::Vector3s({i/(float)nx, 0, j / float(ny) });
			cnt++;
		}
	}
	cnt = 0;
	for (int i = 0; i < nx; i++) {
		for (int j = 0; j < ny; j++) {
			int vidx = i*(ny + 1) + j;
			m->t[cnt][0] = vidx;
			m->t[cnt][1] = vidx + 1;
			m->t[cnt][2] = vidx + 2 + ny;
			cnt++;

			m->t[cnt][0] = vidx;
			m->t[cnt][1] = vidx + 2 + ny;
			m->t[cnt][2] = vidx + 1 + ny;
			cnt++;
		}
	}
}

void makeCube(TrigMesh & m, const TrigMesh::Vector3s & mn,
    const TrigMesh::Vector3s & mx)
{
  TrigMesh::Vector3s ss = mx - mn;
  m=UNIT_CUBE;
  for(unsigned int ii = 0;ii<m.v.size();ii++){
	  for (int j = 0; j < 3; j++) {
		  m.v[ii][j] = mn[j] + ss[j] * m.v[ii][j];
	  }
  }
}

void TrigMesh::append(const TrigMesh & m)
{
  size_t offset = v.size();
  size_t ot = t.size();
  v.insert(v.end(),m.v.begin(),m.v.end());
  t.insert(t.end(),m.t.begin(), m.t.end());
  for(size_t ii = ot;ii<t.size();ii++){
    for(int jj = 0 ;jj<3;jj++){
      t[ii][jj] += (int)offset;
    }
  }
}

TrigMesh & TrigMesh::operator= (const TrigMesh& m)
{
  v = m.v;
  t = m.t;
  n = m.n;
  return *this;
}

///@brief cube [0,1]^3
TrigMesh::Vector3s CUBE_VERT[8]={
	{0, 0, 0},
	{1, 0, 0},
	{1, 1, 0},
	{0, 1, 0},
	{0, 0, 1},
	{1, 0, 1},
	{1, 1, 1},
	{0, 1, 1}
};

TrigMesh::Vector3i CUBE_TRIG[12]={
	{0, 3, 1},
  {1, 3, 2},  //z=0face
  { 7, 4, 5 },
  { 7, 5, 6 },  //z=1 face
  {5, 4, 0},
  {5, 0, 1},  //y=0 face
  {3, 6, 2},
  {3, 7, 6},  //y=1 face
  {4, 3, 0},
  {4, 7, 3},  //x=0 face
  { 6, 5, 1 },
  { 1, 2, 6 },  //x=1 face
 };

TrigMesh UNIT_CUBE(CUBE_VERT,CUBE_TRIG);

TrigMesh::TrigMesh():v(0),t(0){}

TrigMesh::TrigMesh(const std::vector<Vector3s> &_v,
    const std::vector<Vector3i> &_t):v(_v),t(_t)
{
  compute_norm();
}

TrigMesh::TrigMesh(const Vector3s * _v,
  const Vector3i * _t)
{
  v.assign(_v,_v+8);
  t.assign(_t,_t+12);
  
  compute_norm();
}

void TrigMesh::save(std::ostream & out, std::vector<Vector3s> * vert)
{
  std::string vTok("v");
  std::string fTok("f");
  std::string texTok("vt");
  char bslash='/';
  std::string tok;
  if(vert==0){
    vert = &v;
  }
  for(size_t ii=0;ii<vert->size();ii++){
    out<<vTok<<" "<<(*vert)[ii][0]<<" "<<(*vert)[ii][1]<<" "<<(*vert)[ii][2]<<"\n";
  }
  if(tex.size()>0){
    for(size_t ii=0;ii<tex.size();ii++){
      out<<texTok<<" "<<tex[ii][0]<<" "<<tex[ii][1]<<"\n";
    }
    for(size_t ii=0;ii<t.size();ii++){
      out<<fTok<<" "<<t[ii][0]+1<<bslash<<texId[ii][0]+1<<" "
      <<t[ii][1]+1<<bslash<<texId[ii][1]+1<<" "
      <<t[ii][2]+1<<bslash<<texId[ii][2]+1<<"\n";
    }
  }else{
    for(size_t ii=0;ii<t.size();ii++){
      out<<fTok<<" "<<t[ii][0]+1<<" "<<
          t[ii][1]+1<<" "<<t[ii][2]+1<<"\n";
    }
  }
  out<<"#end\n";
}

void TrigMesh::save(const char * filename)
{
  std::ofstream out;
  out.open(filename);
  save(out);
  out.close();
}


void TrigMesh::load(std::istream &in)
{
  read_obj(in);
}

void TrigMesh::read_obj(std::istream & f)
{
  std::string line;
  std::string vTok("v");
  std::string fTok("f");
  std::string texTok("vt");
  char bslash='/',space=' ';
  std::string tok;
  while(1) {
    if (f.eof()) {
      break;
    }
    std::getline(f,line);
    if(line == "#end"){
      break;
    }
    if(line.size()<3) {
      continue;
    }
    if(line.at(0)=='#') {
      continue;
    }
    std::stringstream ss(line);
    ss>>tok;
    if(tok==vTok) {
      Vector3s vec;
      ss>>vec[0]>>vec[1]>>vec[2];
      v.push_back(vec);
    } else if(tok==fTok) {
      bool hasTexture = false;
      if (line.find(bslash) != std::string::npos) {
        std::replace(line.begin(), line.end(), bslash, space);
        hasTexture = true;
      }
      std::stringstream facess(line);
      facess>>tok;
      std::vector<int> vidx;
      std::vector<int> texIdx;
      int x;
      while(facess>>x){
        vidx.push_back(x);
        if(hasTexture){
          facess>>x;
          texIdx.push_back(x);
        }
      }
      texIdx.resize(vidx.size());
      for(int ii = 0;ii<vidx.size()-2;ii++){
        Vector3i trig, textureId;
        trig[0] = vidx[0]-1;
        textureId[0] = texIdx[0]-1;
        for (int jj = 1; jj < 3; jj++) {
          trig[jj] = vidx[ii+jj]-1;
          textureId[jj] = texIdx[ii+jj]-1;
        }
        t.push_back(trig);
        texId.push_back(textureId);
      }
    } else if(tok==texTok) {
      Vector2s texcoord;
      ss>>texcoord[0];
      ss>>texcoord[1];
      tex.push_back(texcoord);
    }
  }
  std::cout<<"Num Triangles: "<< t.size()<<"\n";
}

void TrigMesh::read_ply(std::istream & f)
{
  std::string line;
  std::string vertLine("element vertex");
  std::string faceLine("element face");
  std::string texLine("property float s");
  std::string endHeaderLine("end_header");
  while(true) {
    std::getline(f,line);
    if(std::string::npos!=line.find(vertLine)) {
      break;
    }
  }
  std::string token;
  std::stringstream ss(line);
  ss>>token>>token;
  int nvert;
  ss>>nvert;
  bool hasTex=false;
  while(true) {
    std::getline(f,line);
    if(std::string::npos!=line.find(faceLine)) {
      break;
    }
    if(std::string::npos!=line.find(texLine)) {
      hasTex=true;
    }
  }
  std::stringstream ss1(line);
  ss1>>token>>token;
  int nface;
  ss1>>nface;
  while(true) {
    std::getline(f,line);
    if(std::string::npos!=line.find(endHeaderLine)) {
      break;
    }
  }

  v.resize(nvert);
  t.resize(nface);
  if(hasTex) {
    tex.resize(nvert);
  }
  for (int ii =0; ii<nvert; ii++) {
    for (int jj=0; jj<3; jj++) {
      f>>v[ii][jj];
    }
    if(hasTex) {
      for (int jj=0; jj<2; jj++) {
        f>>tex[ii][jj];
      }
      tex[ii][1]=1-tex[ii][1];;
    }
  }
  for (int ii =0; ii<nface; ii++) {
    int nidx;
    f>>nidx;
    for (int jj=0; jj<3; jj++) {
      f>>t[ii][jj];
    }
  }
}

void TrigMesh::save_obj(const char * filename)
{
  std::ofstream out(filename);
  if(!out.good()){
    std::cout<<"cannot open output file"<<filename<<"\n";
    return;
  }
  save(out);
  out.close();
}

void TrigMesh::update()
{}

TrigMesh::TrigMesh(const char * filename,bool normalize)
{
  load_mesh(filename,normalize);
}


void TrigMesh::load_mesh(const char * filename, bool normalize)
{
  std::ifstream f ;
  f.open(filename);
  if(!f.good()) {
    std::cout<<"Error: cannot open mesh "<<filename<<"\n";
    return;
  }
  switch(filename[strlen(filename)-1]) {
  case 'y':
    read_ply(f);
    break;
  case 'j':
    read_obj(f);
    break;
  default:
    break;
  }
  if(normalize){
    rescale();
  }
  compute_norm();

  f.close();
}

void TrigMesh::rescale()
{
  if(v.size()==0){
    std::cout<<"empty mesh\n";
    return;
  }
  Vector3s mn=v[0],mx=v[0];

  //scale and translate to [0 , 1]
  for (unsigned int dim = 0; dim<3; dim++) {
    for( size_t ii=0; ii<v.size(); ii++) {
      mn[dim]= std::min(v[ii][dim],mn[dim]);
      mx[dim] = std::max(v[ii][dim],mx[dim]);
    }
    float translate = -mn[dim];
    for(size_t ii=0; ii<v.size(); ii++) {
      v[ii][dim]=(v[ii][dim]+translate);
    }
  }

  float scale = 1/(mx[0]-mn[0]);
  for(unsigned int dim=1; dim<3; dim++) {
    scale=std::min(1/(mx[dim]-mn[dim]),scale);
  }

  for(size_t ii=0; ii<v.size(); ii++) {
    for (unsigned int dim = 0; dim<3; dim++) {
      v[ii][dim]=v[ii][dim]*scale;
    }
  }
}

void TrigMesh::compute_norm()
{
	n.resize(v.size());
	std::fill(n.begin(), n.end(), std::array<float,3>({ 0.0f, 0.0f, 0.0f }));
  for(unsigned int ii=0; ii<t.size(); ii++) {
    Vector3s a = v[t[ii][1]] - v[t[ii][0]];
    Vector3s b = v[t[ii][2]] - v[t[ii][0]];
    b = cross(a, b);
    normalize(b);
    for(int jj=0; jj<3; jj++) {
      n[t[ii][jj]] = n[t[ii][jj]] + b;
    }
  }
  for(unsigned int ii=0; ii<v.size(); ii++) {
    normalize(n[ii]);
  }
}

void savePly(TrigMesh * tm, std::string filename)
{
  FileUtilOut out(filename);
  out.out << "ply\nformat ascii 1.0\nelement vertex " << tm->v.size() << "\n";
  out.out << "property float x\nproperty float y\nproperty float z\n";
  out.out << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
  out.out << "element face " << tm->t.size() << "\n";
  out.out << "property list uchar int vertex_index\nend_header\n";
  //compute average vertex color;
  std::vector<TrigMesh::Vector3s> vcolor(tm->v.size(), TrigMesh::Vector3s({ 0.0, 0.0, 0.0}));
  
  if (tm->tcolor.size() == tm->t.size()) {
    std::vector<int> vcnt(tm->v.size(), 0);
    for (size_t i = 0; i < tm->t.size(); i++) {
      for (int j = 0; j < 3; j++) {
        int vidx = tm->t[i][j];
        vcolor[vidx] = tm->tcolor[i][j] + vcolor[vidx];
        vcnt[vidx] ++;
      }
    }
    for (size_t i = 0; i < tm->v.size(); i++) {
      vcolor[i] = (1.0f / vcnt[i]) * vcolor[i];
    }
  }
  else {
	  vcolor = std::vector<TrigMesh::Vector3s>(tm->v.size(), TrigMesh::Vector3s({0.0f,0.0f,0.0f}));
  }

  for (size_t i = 0; i < tm->v.size(); i++) {
    out.out << tm->v[i][0] << " " << tm->v[i][1] << " " << tm->v[i][2] << " ";
    out.out << (unsigned int)(vcolor[i][0] * 255) << " " << (unsigned int)(vcolor[i][1] * 255) << " " << (unsigned int)(vcolor[i][2] * 255) << "\n";
  }
  for (size_t i = 0; i < tm->t.size(); i++) {
    out.out << "3 " << tm->t[i][0] << " " << tm->t[i][1] << " " << tm->t[i][2] << "\n";
  }
  out.close();
}

void TrigMesh::rmUnusedVert() {
    std::vector<int> vertIdx(v.size());
    std::fill(vertIdx.begin(), vertIdx.end(), -1);
    std::vector<Vector3s> newV;
    int vCnt = 0;
    for (unsigned int ii = 0; ii < t.size(); ii++) {
        for (int jj = 0; jj<3; jj++) {
            int vi = t[ii][jj];
            if (vertIdx[vi]<0) {
                newV.push_back(v[vi]);
                vertIdx[vi] = vCnt;
                vCnt++;
            }
            vi = vertIdx[vi];
            t[ii][jj] = vi;
        }
    }
    v = newV;
}