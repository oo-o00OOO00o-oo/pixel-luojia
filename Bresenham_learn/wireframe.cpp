#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "geometry.h"   // 用仓库自带的 vec3
#include "tgaimage.h"  // 用仓库自带的 TGAImage
using namespace std;
struct Model {
    vector<vec3> verts;              // 所有顶点
    vector<std::vector<int>> faces;  // 每个面 3 个顶点索引

    Model(const char* filename) {
        ifstream in(filename);
        string line;
        while (getline(in, line)) {
            istringstream iss(line);
            string tag;
            iss >> tag;
            if (tag == "v") {
                double x, y, z;
                iss >> x >> y >> z;
                verts.push_back(vec3{x, y, z});   // 花括号，不是圆括号
            } else if (tag == "f") {
                vector<int> f;
                int idx;
                while (iss >> idx ) {
                if(iss.peek() == '/') { // 如果有纹理坐标或法线索引，跳过它们
                    while (iss.peek() != ' '&&iss.good()) {
                        iss.get(); // 读取并丢弃字符
                    }
                    f.push_back(idx - 1); // obj 文件索引从 1 开始
                }
                }
                faces.push_back(f);
            }
        }
    }
};

void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    bool steep = std::abs(ax-bx) < std::abs(ay-by);
    if (steep) { // if the line is steep, we transpose the image
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax>bx) { // make it left−to−right
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x=ax; x<=bx; x++) {
        if (steep) // if transposed, de−transpose
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by-ay);
        if (ierror > bx - ax) {
            y += (by > ay ? 1 : -1);
            ierror -= 2 * (bx - ax);
        }
    }
}

int main() {
    Model model("obj/african_head/african_head.obj");
    constexpr int width = 800,height = 800;
    TGAImage framebuffer(width, height, TGAImage::RGB);
    constexpr TGAColor white = {255, 255, 255, 255};

    for(int i = 0; i< model.faces.size(); i++) {
        const auto &face = model.faces[i];
        for(int j = 0; j< face.size(); j++) {
            int idx1 = face[j];
            int idx2 = face[(j+1) % face.size()];
            vec3 v1 = model.verts[idx1];
            vec3 v2 = model.verts[idx2];
            int x1 = (v1.x + 1.) * width / 2.;
            int y1 = (v1.y + 1.) * height / 2.;
            int x2 = (v2.x + 1.) * width / 2.;
            int y2 = (v2.y + 1.) * height / 2.;
            line(x1, y1, x2, y2, framebuffer, white);
        }
    }
    framebuffer.write_tga_file("output2.tga");
    return 0;
}