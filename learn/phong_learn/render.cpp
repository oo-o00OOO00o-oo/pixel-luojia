#include <cmath>
#include <algorithm>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../../geometry.h"   // 项目根目录的 vec3
#include "../../tgaimage.h"   // 项目根目录的 TGAImage

using namespace std;

// TODO A: 把你写好的 Model 结构体复制过来（解析 v/f 那段）
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

// TODO B: triangle() 函数（带 zbuffer 的版本）
void triangle(vec3 v1, vec3 v2, vec3 v3,TGAImage &fb, double *zbuffer, 
            vec3 n1, vec3 n2, vec3 n3, 
            vec2 uv1, vec2 uv2, vec2 uv3, TGAImage &diffuse,
            vec3 light_dir, vec3 view_dir,TGAImage &specular ) {
    int ax = (v1.x + 1.) * fb.width() / 2.;
    int ay = (v1.y + 1.) * fb.height() / 2.;
    int bx = (v2.x + 1.) * fb.width() / 2.;
    int by = (v2.y + 1.) * fb.height() / 2.;
    int cx = (v3.x + 1.) * fb.width() / 2.;
    int cy = (v3.y + 1.) * fb.height() / 2.;
    
    int min_x = max(0, min({ax, bx, cx}));
    int max_x = min(fb.width() - 1, max({ax, bx, cx}));
    int min_y = max(0, min({ay, by, cy})); 
    int max_y = min(fb.height() - 1, max({ay, by, cy}));
    
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
                if (zbuffer[idx] < z) {                         // 比之前画的近 → 覆盖（否则丢弃）
                    zbuffer[idx] = z;
                    // 使用纹理映射
                    double u = alpha * uv1.x + beta * uv2.x + gamma * uv3.x;
                    double v = alpha * uv1.y + beta * uv2.y + gamma * uv3.y;
                    v = 1 - v; // obj 的 v=0 在贴图底部；read_tga_file 读入后内存里 y=0 是顶行，方向相反故翻转
                    int tex_x = u * (diffuse.width() - 1);
                    int tex_y = v * (diffuse.height() - 1);

                    vec3 n = normalized(alpha*n1 + beta*n2 + gamma*n3);   // 插值法线
                
                    double diff = max(0., n * light_dir);   // 漫反射强度

                    vec3 r = (n * (n * light_dir * 2.) - light_dir);   // 反射光线
                    double s = 5 + specular.get(tex_x, tex_y)[0] ;   // 高光指数（越大越尖锐）
                    double sp = pow(max(0., r * view_dir), s);   // 高光强度

                    double ka = 0.1,kd = 1.0,ks = 0.6;   // 环境、漫反射、高光系数
                    double I = ka + kd * diff + ks * sp;   // 光照强度（环境光 + 漫反射 + 高光）

                    TGAColor color = diffuse.get(tex_x, tex_y);
                    unsigned char cr = min(255., color[2] * I);
                    unsigned char cg = min(255., color[1] * I);
                    unsigned char cb = min(255., color[0] * I);
                    fb.set(j, i, TGAColor{cb, cg, cr, 255});      // TGAColor 内部是 bgra 顺序
                }
            }
        }
    }
}


// TODO C: lookat() 函数（相机变换矩阵）
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

int main() {
    Model model("obj/african_head/african_head.obj");
    constexpr int width = 800, height = 800;
    TGAImage framebuffer(width, height, TGAImage::RGB);

    // zbuffer 分配 + 初始化
    std::vector<double> zbuffer(width*height, -1e9);

    vec3 eye{1, 1, 3}, center{0, 0, 0}, up{0, 1, 0};
    mat<4,4> M = lookat(eye, center, up);

    double c = norm(eye - center);       // 相机到目标的距离 = 透视强度
    mat<4,4> P;
    P[0] = {1, 0, 0, 0};
    P[1] = {0, 1, 0, 0};
    P[2] = {0, 0, 1, 0};
    P[3] = {0, 0, -1./c, 1};             // 把 z 折算进 w → 透视除法生效

    mat<4,4> MVP = P * M;                // 先视图后投影（从右往左读）
    
    TGAImage diffuse;
    if(!diffuse.read_tga_file("obj/african_head/african_head_diffuse.tga")) {
        return 1;
    }
    vec3 light_dir = normalized(vec3{1,1,1});
    vec3 view_dir = normalized(eye - center);
    
    TGAImage specular;
    if(!specular.read_tga_file("obj/african_head/african_head_spec.tga")) {
        return 1;
    }

    for (int i = 0; i < (int)model.faces.size(); i++) {
        const auto &face = model.faces[i];
        vec3 v1 = model.verts[face[0]];
        vec3 v2 = model.verts[face[1]];
        vec3 v3 = model.verts[face[2]];

        vec3 n1 = model.norms[model.face_norms[i][0]];
        vec3 n2 = model.norms[model.face_norms[i][1]];
        vec3 n3 = model.norms[model.face_norms[i][2]];

        vec4 t1 = MVP * vec4{v1.x, v1.y, v1.z, 1.0};
        vec4 t2 = MVP * vec4{v2.x, v2.y, v2.z, 1.0};
        vec4 t3 = MVP * vec4{v3.x, v3.y, v3.z, 1.0};
        
        vec2 uv1 = model.tex[model.face_tex[i][0]];
        vec2 uv2 = model.tex[model.face_tex[i][1]];
        vec2 uv3 = model.tex[model.face_tex[i][2]];
 
        v1 = {t1.x/t1.w, t1.y/t1.w, t1.z/t1.w};
        v2 = {t2.x/t2.w, t2.y/t2.w, t2.z/t2.w};
        v3 = {t3.x/t3.w, t3.y/t3.w, t3.z/t3.w};

        triangle(v1, v2, v3, framebuffer, zbuffer.data(),
                 n1, n2, n3, 
                 uv1, uv2, uv3, diffuse, 
                 light_dir, view_dir, specular);
    
    }
    framebuffer.write_tga_file("learn/phong_learn/output.tga");
    return 0;
}