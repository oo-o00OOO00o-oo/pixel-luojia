#include <cmath>
#include <algorithm>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../../geometry.h"
#include "../../tgaimage.h"

using namespace std;

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
                verts.push_back(vec3{x, y, z});
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
                while (iss >> group) {
                    size_t p1 = group.find('/');
                    size_t p2 = group.find('/', p1+1);
                    f.push_back ( stoi(group.substr(0, p1)) - 1);
                    ft.push_back( stoi(group.substr(p1+1, p2-p1-1)) - 1);
                    fn.push_back( stoi(group.substr(p2+1)) - 1);
                }
                faces.push_back(f);
                face_tex.push_back(ft);
                face_norms.push_back(fn);
            }
        }
    }
};

mat<4,4> lookat(vec3 eye, vec3 center, vec3 up) {
    vec3 z = normalized((eye - center));
    vec3 x = normalized(cross(up, z));
    vec3 y = normalized(cross(z, x));

    mat<4,4> R, T;
    R[0] = {x.x, x.y, x.z, 0};
    R[1] = {y.x, y.y, y.z, 0};
    R[2] = {z.x, z.y, z.z, 0};
    R[3] = {0, 0, 0, 1};

    T[0] = {1, 0, 0, -eye.x};
    T[1] = {0, 1, 0, -eye.y};
    T[2] = {0, 0, 1, -eye.z};
    T[3] = {0, 0, 0, 1};

    return R * T;
}

// ============================================================
// triangle: 光栅化一个三角形
//   v1..v3     —— MVP 变换 + 透视除法后的坐标（进 zbuffer）
//   w1..w3     —— 未变换的世界坐标（插值出像素世界位置，查 shadowmap 用）
//   depthOnly  —— true: 只更新深度（pass1）；false: 完整着色（pass2）
//   shadowmap/lightMVP —— pass2 用；pass1 传 nullptr / 单位阵
// ============================================================
void triangle(vec3 v1, vec3 v2, vec3 v3, vec3 w1, vec3 w2, vec3 w3,
              TGAImage &fb, double *zbuffer,
              vec3 n1, vec3 n2, vec3 n3,
              vec2 uv1, vec2 uv2, vec2 uv3, TGAImage &diffuse,
              vec3 light_dir, vec3 view_dir, TGAImage &specular,
              vec3 T, vec3 B, TGAImage &normalmap,
              bool depthOnly, const double *shadowmap, mat<4,4> &lightMVP) {
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
            double alpha = b / (double)total_area;
            double beta  = c / (double)total_area;
            double gamma = a / (double)total_area;

            if((a >= 0 && b >= 0 && c >= 0)) {
                double z = alpha*v1.z + beta*v2.z + gamma*v3.z;
                int idx = j + i*fb.width();
                if (zbuffer[idx] < z) {
                    zbuffer[idx] = z;

                    if (depthOnly) continue;   // pass1: 只要深度

                    // ---------- 纹理采样 ----------
                    double u = alpha * uv1.x + beta * uv2.x + gamma * uv3.x;
                    double v = alpha * uv1.y + beta * uv2.y + gamma * uv3.y;
                    v = 1 - v;
                    int tex_x = u * (diffuse.width() - 1);
                    int tex_y = v * (diffuse.height() - 1);

                    // ---------- 法线贴图 ----------
                    vec3 n = normalized(alpha*n1 + beta*n2 + gamma*n3);
                    vec3 t = normalized(T - n *(T * n ));
                    vec3 bb = normalized(B - n *(B * n ) - t * (B * t));
                    TGAColor normal_color = normalmap.get(tex_x, tex_y);
                    vec3 normal = {normal_color[2] / 255. * 2. - 1., normal_color[1] / 255. * 2. - 1., normal_color[0] / 255. * 2. - 1.};
                    n = normalized(t * normal.x + bb * normal.y + n * normal.z);

                    // ---------- Phong 光照 ----------
                    double diff = max(0., n * light_dir);
                    vec3 r = (n * (n * light_dir * 2.) - light_dir);
                    double s = 5 + specular.get(tex_x, tex_y)[0];
                    double sp = pow(max(0., r * view_dir), s);
                    double ka = 0.1, kd = 1.0, ks = 0.6;
                    double I = ka + kd * diff + ks * sp;

                    // ---------- 卡通渲染（第 11 站） ----------
                    constexpr bool TOON = true;   // 开关：false 即恢复连续光照
                    if (TOON) {
                        if      (I > 0.85) I = 1.00;
                        else if (I > 0.60) I = 0.80;
                        else if (I > 0.45) I = 0.60;
                        else if (I > 0.30) I = 0.45;
                        else               I = 0.30;
                    }
                    
                    vec3 wp = alpha*w1 + beta*w2 + gamma*w3;
                    vec4 lp = lightMVP * vec4{wp.x, wp.y, wp.z, 1};
                    lp.x /= lp.w; lp.y /= lp.w; lp.z /= lp.w;
                    double lx = (lp.x + 1.) * fb.width() / 2.;
                    double ly = (lp.y + 1.) * fb.height() / 2.;
                    double lz = lp.z;
                    int sx = (int)lx;
                    int sy = (int)ly;
                    
                    double bias = 0.005;
                    if (sx >= 0 && sx < fb.width() && sy >= 0 && sy < fb.height()
                        && lz < shadowmap[sx + sy * fb.width()] - bias) {
                        I = ka; // 比光源记录的深度远 → 被挡住 → 在阴影中
                    }

                    TGAColor color = diffuse.get(tex_x, tex_y);
                    unsigned char cr = min(255., color[2] * I);
                    unsigned char cg = min(255., color[1] * I);
                    unsigned char cb = min(255., color[0] * I);
                    fb.set(j, i, TGAColor{cb, cg, cr, 255});
                }
            }
        }
    }
}

// ============================================================
// render: 用给定 MVP 渲染整个场景（两个 pass 共用）
// ============================================================
void render(Model &model, mat<4,4> &MVP, TGAImage &fb, double *zbuffer,
            bool depthOnly,
            TGAImage &diffuse, TGAImage &specular, TGAImage &normalmap,
            vec3 light_dir, vec3 view_dir,
            const double *shadowmap, mat<4,4> &lightMVP) {
    for (int i = 0; i < (int)model.faces.size(); i++) {
        const auto &face = model.faces[i];
        vec3 w1 = model.verts[face[0]];   // 世界坐标（不变换）
        vec3 w2 = model.verts[face[1]];
        vec3 w3 = model.verts[face[2]];

        vec3 n1 = model.norms[model.face_norms[i][0]];
        vec3 n2 = model.norms[model.face_norms[i][1]];
        vec3 n3 = model.norms[model.face_norms[i][2]];

        vec4 t1 = MVP * vec4{w1.x, w1.y, w1.z, 1.0};
        vec4 t2 = MVP * vec4{w2.x, w2.y, w2.z, 1.0};
        vec4 t3 = MVP * vec4{w3.x, w3.y, w3.z, 1.0};

        vec2 uv1 = model.tex[model.face_tex[i][0]];
        vec2 uv2 = model.tex[model.face_tex[i][1]];
        vec2 uv3 = model.tex[model.face_tex[i][2]];

        vec3 e1 = w2 - w1;
        vec3 e2 = w3 - w1;
        double du1 = uv2.x - uv1.x, dv1 = uv2.y - uv1.y;
        double du2 = uv3.x - uv1.x, dv2 = uv3.y - uv1.y;
        double det = 1.0 / (du1 * dv2 - du2 * dv1);
        vec3 T = normalized((e1 * dv2 - e2 * dv1) * det);
        vec3 B = normalized((e2 * du1 - e1 * du2) * det);

        vec3 v1 = {t1.x/t1.w, t1.y/t1.w, t1.z/t1.w};
        vec3 v2 = {t2.x/t2.w, t2.y/t2.w, t2.z/t2.w};
        vec3 v3 = {t3.x/t3.w, t3.y/t3.w, t3.z/t3.w};

        triangle(v1, v2, v3, w1, w2, w3, fb, zbuffer,
                 n1, n2, n3,
                 uv1, uv2, uv3, diffuse,
                 light_dir, view_dir, specular,
                 T, B, normalmap,
                 depthOnly, shadowmap, lightMVP);
    }
}

// ============================================================
// TODO 10 环境光遮蔽（AO）：单方向最大仰角
// 从像素 (x,y) 沿 angle 方向走，找邻居地形的最大仰角
//   dz  = 邻居深度 - 当前深度（深度差 = 高度差）
//   ang = atan2(dz, 平面距离)   ← 仰角
// ============================================================
double max_elevation(const double *zbuf, int w, int h, int x, int y, double angle) {
    double maxang = 0;
    for (double dist = 1; dist < 30; dist += 1) {
        int sx = x + cos(angle)*dist;
        int sy = y + sin(angle)*dist;
        if (sx < 0 || sx >= w || sy < 0 || sy >= h) break;
        double dz = zbuf[sx + sy * w] - zbuf[x + y * w];
        double dxy = dist * 2. / w;   // 归一化平面距离（[-1,1]）
        double ang = atan2(dz, dxy);
        maxang = max(maxang, ang);
    }
    return maxang;
}

int main() {
    Model model("obj/african_head/african_head.obj");
    constexpr int width = 800, height = 800;
    TGAImage framebuffer(width, height, TGAImage::RGB);
    std::vector<double> zbuffer(width*height, -1e9);

    vec3 eye{1, 1, 3}, center{0, 0, 0}, up{0, 1, 0};
    vec3 light_dir = normalized(vec3{1, 1, 1});
    vec3 view_dir = normalized(eye - center);

    TGAImage diffuse, specular, normalmap;
    if(!diffuse.read_tga_file("obj/african_head/african_head_diffuse.tga"))   return 1;
    if(!specular.read_tga_file("obj/african_head/african_head_spec.tga"))     return 1;
    if(!normalmap.read_tga_file("obj/african_head/african_head_nm_tangent.tga")) return 1;

    vec light_pos = light_dir * 3;   // 光源位置（世界坐标）
    mat<4,4> lightM = lookat(light_pos, center, up);

    double c = norm(light_pos - center);
    mat<4,4> lightP;
    lightP[0] = {1, 0, 0, 0};
    lightP[1] = {0, 1, 0, 0};
    lightP[2] = {0, 0, 1, 0};
    lightP[3] = {0, 0, -1./c, 1};
    mat<4,4> lightMVP = lightP * lightM;
    
    vector<double> shadowmap(width*height, -1e9);
    TGAImage shadowmap_fb(width, height, TGAImage::RGB);
    mat<4,4> I;I[0] = {1, 0, 0, 0}; I[1] = {0, 1, 0, 0}; I[2] = {0, 0, 1, 0}; I[3] = {0, 0, 0, 1};
    render(model, lightMVP, shadowmap_fb, shadowmap.data(),
           true, diffuse, specular, normalmap,
           light_dir, view_dir, nullptr, I);
    TGAImage shadowmap_img(width, height, TGAImage::RGB);
    for (int i = 0; i < width*height; i++) {
        if(shadowmap[i] <= -1e9) continue;
        unsigned char g = min (255., (shadowmap[i] + 1.) * 127.5);
        shadowmap_img.set(i % width, i / width, TGAColor{g, g, g, 255});
    }
    shadowmap_img.write_tga_file("learn/final_learn/shadowmap.tga");

    mat<4,4> Mcam = lookat(eye, center, up);
    double c2 = norm(eye - center);
    mat<4,4> Pcam;
    Pcam[0] = {1, 0, 0, 0};Pcam[1] = {0, 1, 0, 0};Pcam[2] = {0, 0, 1, 0};Pcam[3] = {0, 0, -1./c2, 1};
    mat<4,4> MVP = Pcam * Mcam;
    render(model, MVP, framebuffer, zbuffer.data(),
           false, diffuse, specular, normalmap,
           light_dir, view_dir, shadowmap.data(), lightMVP);


    TGAImage ao(width, height, TGAImage::RGB);
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            if (zbuffer[y*width+x] <= -1e9) continue;      // 背景跳过
            double total = 0;
            for (int k = 0; k < 8; k++) {
                double angle = k * M_PI / 4;
                total += M_PI_2 - max_elevation(zbuffer.data(), width, height, x, y, angle);  // 可见天空角
            }
            total /= 8;                                    // 平均
            unsigned char g = (unsigned char)(total / (M_PI/2) * 255);
            ao.set(x, y, TGAColor{g, g, g, 255});
        }
    ao.write_tga_file("learn/final_learn/ao.tga");

    // 应用 AO：framebuffer 逐像素乘 AO 系数
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
            if (zbuffer[y*width+x] <= -1e9) continue;
            double f = ao.get(x, y)[0] / 255.;
            TGAColor col = framebuffer.get(x, y);
            for (int ch = 0; ch < 3; ch++) col[ch] = (unsigned char)(col[ch] * f);
            framebuffer.set(x, y, col);
        }

    framebuffer.write_tga_file("learn/final_learn/output.tga");
    return 0;
}
