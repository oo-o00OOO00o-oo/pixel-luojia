#include <cmath>
#include <cstdlib>
#include <ctime>
#include "tgaimage.h"
#include <algorithm>

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

void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color) {
    line(ax, ay, bx, by, framebuffer, color);
    line(bx, by, cx, cy, framebuffer, color);
    line(cx, cy, ax, ay, framebuffer, color);
}


void cover_triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color) {
    int min_x = std::min({ax, bx, cx});
    int max_x = std::max({ax, bx, cx});
    int min_y = std::min({ay, by, cy}); 
    int max_y = std::max({ay, by, cy});

    for(int i = min_y; i <= max_y; i++) {
        for(int j = min_x; j <= max_x; j++) {
            int a = (bx - ax) * (i - ay) - (by - ay) * (j - ax);
            int b = (cx - bx) * (i - by) - (cy - by) * (j - bx);
            int c = (ax - cx) * (i - cy) - (ay - cy) * (j - cx);
            if((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0)) {
                framebuffer.set(j, i, color);
            }
        }
    }
}



void triangle_gradient(int ax,int ay, int bx,int by, int cx,int cy,
                       TGAImage &fb, TGAColor ca, TGAColor cb, TGAColor cc) {
    int min_x = std::min({ax, bx, cx});
    int max_x = std::max({ax, bx, cx});
    int min_y = std::min({ay, by, cy}); 
    int max_y = std::max({ay, by, cy});
    
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
                TGAColor color;
                for (int ch = 0; ch < 3; ch++)
                    color[ch] = (unsigned char)(ca[ch]*alpha + cb[ch]*beta + cc[ch]*gamma);
                    color[3] = 255;
                fb.set(j, i, color);
            }
        }
    }
    // 1. 包围盒（照抄 cover_triangle 2. 先算 total_area = (bx-ax)*(cy-ay) - (by-ay)*(cx-ax)
    //    若 < 1 直接 return（顺便完成背面剔除+退化保护）
    // 3. 遍历包围盒内每个像素 (x, y)：
    //    算 a、b、c 三个叉积（照抄）
    //    alpha = ?  beta = ?  gamma = ?     ← 注意对应哪个子面积
    //    若三者都 >= 0（在内部）：
    //       .r = alpha*ca.r + beta*cb.r + gamma*cc.r  （每个通道分别插值）
    //        fb.set(x, y, color)
}

int main(int argc, char** argv) {
    const int width = 1000;
    const int height = 1000;
    constexpr TGAColor white = {255, 255, 255, 255};
    constexpr TGAColor red = { 0, 0, 255, 255};
    constexpr TGAColor green = {0, 255, 0, 255};
    constexpr TGAColor blue = {255, 0, 0, 255};
    TGAImage framebuffer(width, height, TGAImage::RGB);

    triangle_gradient(50,50,  900,150,  400,900, framebuffer, red, green, blue);
    framebuffer.write_tga_file("output.tga");
    return 0;

}