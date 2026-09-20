#include <cmath>
#include <algorithm>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../../geometry.h"   // 项目根目录的 vec3
#include "../../tgaimage.h"   // 项目根目录的 TGAImage

using namespace std;

// lookat 函数（照上一条讲的写）
mat<4,4> lookat(vec3 eye, vec3 center, vec3 up) {
    vec3 z = normalized((eye - center)); // 视线方向
    vec3 x = normalized(cross(up, z));    // 右方向
    vec3 y = normalized(cross(z, x));     // 上方向

    mat<4,4> R, T;
    R[0] = {x.x, x.y, x.z, 0};           // 新轴放行
    R[1] = {y.x, y.y, y.z, 0};
    R[2] = {z.x, z.y, z.z, 0};
    R[3] = {0, 0, 0, 1};

    T[0] = {1, 0, 0, -eye.x};            // 平移 -e
    T[1] = {0, 1, 0, -eye.y};
    T[2] = {0, 0, 1, -eye.z};
    T[3] = {0, 0, 0, 1};

    return R * T; // 注意顺序，先平移后旋转
}

struct Model {
    vector<vec3> verts;              // 所有顶点
    vector<vec3> norms;              // 所有法线
    vector<vec2> tex;                // 所有纹理坐标
    vector<vector<int>> faces;       // 每个面 3 个顶点索引
    vector<vector<int>> face_norms;  // 每个面 3 个法线索引
    vector<vector<int>> face_tex;    // 每个面 3 个纹理坐标索引

    Model(const char* filename) {
        ifstream in(filename);
        if (!in) { cerr << "打不开模型文件: " << filename << endl; exit(1); }
        string line;
        while (getline(in, line)) {
            istringstream iss(line);
            string tag;
            iss >> tag;
            if (tag == "v") {
                double x, y, z;
                iss >> x >> y >> z;
                verts.push_back(vec3{x, y, z});   // 花括号，不是圆括号
            } else if (tag == "vt") {
                double u, v;
                iss >> u >> v;
                tex.push_back(vec2{u, v});
            } else if (tag == "vn") {
                double x, y, z;
                iss >> x >> y >> z;
                norms.push_back(vec3{x, y, z});
            } else if (tag == "f") {
                string group;
                vector<int> f, ft, fn;
                while (iss >> group) {              // group = "1193/1240/1193"
                    size_t p1 = group.find('/');           // 第一个 /
                    size_t p2 = group.find('/', p1+1);     // 第二个 /
                    f.push_back ( stoi(group.substr(0, p1)) - 1);          // 顶点
                    ft.push_back( stoi(group.substr(p1+1, p2-p1-1)) - 1);  // 纹理
                    fn.push_back( stoi(group.substr(p2+1)) - 1);           // 法线
                }
                faces.push_back(f);
                face_tex.push_back(ft);
                face_norms.push_back(fn);
            }
        }
    }
};


int main() {
    Model model("obj/african_head/african_head.obj");
    cout << model.verts.size() << " "      // 1258
     << model.norms.size() << " "      // 应该是 1258
     << model.tex.size() << " "        // 应该是 1339
     << model.face_norms[0][0] << endl; // 第一面第一顶点的法线索引，应为 1192
    return 0;
}