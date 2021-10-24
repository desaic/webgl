#include "meshutil.h"
#include "Array3D.h"
#include "ArrayUtil.hpp"
#include "FileUtil.hpp"
#include "TrigMesh.hpp"

#include <thread>
struct QuadMesh {
    std::vector<TrigMesh::Vector4i> q;
    std::vector<TrigMesh::Vector3s> v;
};

void saveCalibObj(const float * table, const int * tsize,
                  std::string filename, int subsample)
{
    FileUtilOut out (filename);
    for(int ix = 0; ix<tsize[0]; ix+=subsample){
        for(int iy = 0; iy<tsize[1]; iy+=subsample){
            for(int iz = 0; iz<tsize[2]; iz+=subsample){
                out.out<<"v";
                int tableIdx = gridToLinearIdx(ix, iy, iz, tsize);
                for(int d = 0; d<3; d++){
                    out.out<<" "<<table[tableIdx*3 + d];
                }
                out.out<<"\n";
            }
        }
    }
    out.close();
}

void savePointsObj(const std::string & fileName, const std::vector<std::array<float, 3 > > & points)
{
	FileUtilOut out(fileName);
	if (!out.out.good()) {
		return;
	}
	for (size_t i = 0; i < points.size(); i++) {
		out.out << "v " << points[i][0] <<" "<< points[i][1] << " " << points[i][2] << "\n";
	}
	out.out.close();
}

void addCube(int startj, int endj, int i, int k, TrigMesh & mout,
    const TrigMesh::Vector3s & dx) {
    TrigMesh box;
    TrigMesh::Vector3s box0={ dx[0] * i, dx[1] * startj, dx[2] * k };
    TrigMesh::Vector3s box1={ dx[0] + dx[0] * i, dx[1] + dx[1] * endj, dx[2] + dx[2] * k };
    makeCube(box, box0, box1);
    mout.append(box);
}

void addQuad(QuadMesh & m, const std::vector<TrigMesh::Vector3s> & v) {
    TrigMesh::Vector4i q;
    for (int i = 0; i < 4; i++) {
        q[i] = i + m.v.size();
    }
    m.q.push_back(q);
    m.v.insert(m.v.end(), v.begin(), v.end());
}

void growRect(const std::vector <uint8_t> & plane, int x0, int y0, int & x1, int & y1, int nx, int ny) {
    std::vector<uint8_t> colFlag(ny, 1);
    int y = y0;
    for (; y < ny; y++) {
        if (plane[x0*ny + y] == 0) {
            break;
        }
    }
    y1 = y - 1;

    bool fullRow = true;
    for (int x = x0 + 1; x < nx; x++) {
        for (int y = y0; y <= y1; y++) {
            if (plane[y + x * ny] == 0) {
                fullRow = false;
                break;
            }
        }
        if (!fullRow) {
            x1 = x - 1;
            break;
        }
    }
    if (fullRow) {
        x1 = nx - 1;
    }
}

void addLayer(QuadMesh * mpt, int layer0, int layer1, int faceIdx, const Array3D8u * volpt, int mat, const TrigMesh::Vector3s * dxpt) {
    QuadMesh & m = (*mpt);
    const Array3D8u&vol = (*volpt);
    const TrigMesh::Vector3s & dx = (*dxpt);
    //-1 for negative side and 1 for positive side face
    int faceOffset = faceIdx % 2;
    int sign = 2 * (faceOffset) - 1;
    int zAxis = faceIdx / 2;
    int xAxis = ( zAxis + 1 ) % 3;
    int yAxis = ( zAxis + 2 ) % 3;
    int nx = vol.GetSize()[xAxis];
    int ny = vol.GetSize()[yAxis];
    int nz = vol.GetSize()[zAxis];
    int volIdx[3];
    int nbrIdx[3];

    std::vector <uint8_t> plane(nx*ny, 0);
    //copy visible voxels in this layer to 2D plane
    for (int layer = layer0; layer < layer1; layer++) {
        volIdx[zAxis] = layer;
        nbrIdx[zAxis] = layer + sign;

        for (int x = 0; x < nx; x++) {
            volIdx[xAxis] = x;
            nbrIdx[xAxis] = x;
            for (int y = 0; y < ny; y++) {
                volIdx[yAxis] = y;
                nbrIdx[yAxis] = y;
                uint8_t val = vol(volIdx[0], volIdx[1], volIdx[2]);
                if (val != mat) {
                    continue;
                }
                if (nbrIdx[zAxis] >= 0 && nbrIdx[zAxis] < nz) {
                    int nbrVal = vol(nbrIdx[0], nbrIdx[1], nbrIdx[2]);
                    if (nbrVal == mat) {
                        continue;
                    }
                }
                plane[x * ny + y] = mat;
            }
        }
        for (int px = 0; px < nx; px++) {
            for (int py = 0; py < ny; py++) {
                if (plane[px*ny + py] != mat) {
                    continue;
                }
                int x0 = px;
                int y0 = py;
                int x1 = px;
                int y1 = py;
                growRect(plane, x0, y0, x1, y1, nx, ny);
                for (int rx = x0; rx <= x1; rx++) {
                    for (int ry = y0; ry <= y1; ry++) {
                        plane[rx*ny + ry] = 0;
                    }
                }
                std::vector<TrigMesh::Vector3s> q(4);
                for (int qi = 0; qi < 4; qi++) {
                    q[qi][zAxis] = layer + faceOffset;
                }
                q[0][xAxis] = x0;
                q[0][yAxis] = y0;

                q[2][xAxis] = x1 + 1;
                q[2][yAxis] = y1 + 1;
                if (sign == 1) {
                    q[1][xAxis] = x1 + 1;
                    q[1][yAxis] = y0;

                    q[3][xAxis] = x0;
                    q[3][yAxis] = y1 + 1;
                }
                else {
                    q[3][xAxis] = x1 + 1;
                    q[3][yAxis] = y0;

                    q[1][xAxis] = x0;
                    q[1][yAxis] = y1 + 1;
                }
                for (int qi = 0; qi < 4; qi++) {
                    for (int dim = 0; dim < 3; dim++) {
                        q[qi][dim] *= dx[dim];
                    }
                }
                addQuad(m, q);
            }
        }
    }
}

void appendQuads(QuadMesh & m, const QuadMesh & src) {
    size_t nV = m.v.size();
    size_t nq = m.q.size();
    m.v.insert(m.v.end(), src.v.begin(), src.v.end());
    m.q.insert(m.q.end(), src.q.begin(), src.q.end());
    for (size_t i = 0; i < src.q.size(); i++) {
        for (int j = 0; j < 4; j++) {
            m.q[i + nq][j] += nV;
        }
    }
}

void addFaces(QuadMesh & m, int faceIdx, const Array3D8u & vol, int mat,const TrigMesh::Vector3s & dx) {
    int axis = faceIdx / 2;
    int nLayer = vol.GetSize()[axis];
    int nThread = 10;
    std::vector<QuadMesh> localM(nThread);
    std::vector<std::thread> threads(nThread);
    int layerPerThread = nLayer / nThread + ((nLayer % nThread) != 0);
    for (int t = 0; t < nThread; t++) {
        int layer0 = t * layerPerThread;
        int layer1 = std::min((t+1)*layerPerThread, nLayer);
        threads[t] = std::thread(addLayer, &localM[t], layer0, layer1, faceIdx, &vol, mat, &dx);
    }
    for (int t = 0; t < nThread; t++) {
        if (threads[t].joinable()) {
            threads[t].join();
        }
        appendQuads(m, localM[t]);
    }
}

void save_obj(const QuadMesh & m, const std::string & filename) {
    std::ofstream out(filename);
    if (!out.good()) {
        std::cout << "cannot open output file" << filename << "\n";
        return;
    }

    std::string vTok("v");
    std::string fTok("f");
    char bslash = '/';
    std::string tok;
    const std::vector<TrigMesh::Vector3s> * vert = &m.v;
    for (size_t ii = 0; ii<vert->size(); ii++) {
        out << vTok << " " << (*vert)[ii][0] << " " << (*vert)[ii][1] << " " << (*vert)[ii][2] << "\n";
    }
    for (size_t ii = 0; ii<m.q.size(); ii++) {
        out << fTok << " " << m.q[ii][0] + 1 << " " <<
            m.q[ii][1] + 1 << " " << m.q[ii][2] + 1 << " "<<m.q[ii][3] + 1 << "\n";
    }
    out << "#end\n";
    out.close();
}

void saveObjRect(std::string outfile, const Array3D8u & vol,
    float *voxRes, int mat)
{
    QuadMesh mout;
    //convert zres to mm.
    TrigMesh::Vector3s dx = { voxRes[0], voxRes[1], voxRes[2] };
    const int N_FACE = 6;
    for (int f = 0; f < N_FACE; f++) {
        addFaces(mout, f, vol, mat, dx);
    }
    save_obj(mout, outfile.c_str());
}