#include <cmath>
#include <algorithm>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../../geometry.h"   // 项目根目录的 vec3
#include "../../tgaimage.h"   // 项目根目录的 TGAImage
using namespace std;
struct Model {
    vector<vec3> verts;              // 所有顶点
    vector<std::vector<int>> faces;  // 每个面 3 个顶点索引

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
// TODO A: 把你写好的 Model 结构体复制过来（解析 v/f 那段）

// TODO B: triangle() 函数（带 zbuffer 的版本）
void triangle(vec3 v1, vec3 v2, vec3 v3,TGAImage &fb, double *zbuffer, TGAColor color) {
    int ax = (v1.x + 1.) * fb.width() / 2.;
    int ay = (v1.y + 1.) * fb.height() / 2.;
    int bx = (v2.x + 1.) * fb.width() / 2.;
    int by = (v2.y + 1.) * fb.height() / 2.;
    int cx = (v3.x + 1.) * fb.width() / 2.;
    int cy = (v3.y + 1.) * fb.height() / 2.;
    
    int min_x = min({ax, bx, cx});
    int max_x = max({ax, bx, cx});
    int min_y = min({ay, by, cy}); 
    int max_y = max({ay, by, cy});
    
    double total_area = (ax-bx)*(ay-cy) - (ax-cx)*(ay-by);
    if (total_area<1) return;
    
    for(int i = min_y; i <= max_y; i++) {
        for(int j = min_x; j <= max_x; j++) {
            int a = (bx - ax) * (i - ay) - (by - ay) * (j - ax);
            int b = (cx - bx) * (i - by) - (cy - by) * (j - bx);
            int c = (ax - cx) * (i - cy) - (ay - cy) * (j - cx);
            double alpha = b / (double)total_area;   // 顶点 A 的权重 ← b（边 BC）
            double beta  = c / (double)total_area;   // 顶点 B 的权重 ← c（边 CA）
            double gamma = a / (double)total_area;   // 顶点 C 的权重 ← a（边 AB）
            if((a >= 0 && b >= 0 && c >= 0)) {
                double z = alpha*v1.z + beta*v2.z + gamma*v3.z;   // 插值顶点深度（不是叉积！）
                int idx = j + i*fb.width();                          // 像素在一维数组里的下标
                if (zbuffer[idx] < z) {                         // 比之前画的远 → 丢弃
                    zbuffer[idx] = z;
                    fb.set(j, i, color);
                }
            }
        }
    }
}
int main() {
    Model model("obj/african_head/african_head.obj");
    constexpr int width = 800, height = 800;
    TGAImage framebuffer(width, height, TGAImage::RGB);

    // zbuffer 分配 + 初始化
    std::vector<double> zbuffer(width*height, -1e9);

    for (int i = 0; i < (int)model.faces.size(); i++) {
        const auto &face = model.faces[i];
        vec3 v1 = model.verts[face[0]];
        vec3 v2 = model.verts[face[1]];
        vec3 v3 = model.verts[face[2]];
        vec3 n  = normalized(cross(v1-v2,v1-v3));
        vec3 light_dir = normalized(vec3{1,1,1});
        double intensity = n * light_dir;   // operator* 即点积
        if (intensity > 0) {
            TGAColor color = { (unsigned char)(intensity*255),
                               (unsigned char)(intensity*255),
                               (unsigned char)(intensity*255), 255 };
            triangle(v1, v2, v3, framebuffer, zbuffer.data(), color);
        }
    }
    framebuffer.write_tga_file("zbuffer_learn/output.tga");
    return 0;
}