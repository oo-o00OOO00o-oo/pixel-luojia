#include <iostream>
#include "../../geometry.h"

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

int main() {
    mat<4,4> M = lookat({3,0,0}, {0,0,0}, {0,1,0});
    vec4 p{0, 0, 0, 1};          // 原点（齐次坐标）
    vec4 q = M * p;              // mat × vec4
    std::cout << q << std::endl;
    std::cout << M << std::endl;    // mat 自带 << 重载，能直接打印
    return 0;
}