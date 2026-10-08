# 第 10-11 站：环境光遮蔽 (AO) + 卡通渲染（完结篇）

## 第 10 站：AO

**原理**：环境光来自四面八方，凹陷处（眼窝、唇缝）能"看到的天空"少 → 收到的环境光少 → 暗。

**算法**（HBAO 简化版，纯 zbuffer 后处理）：

把 zbuffer 当地形高度图。对每个像素沿 8 个方向向外走，每方向找最大仰角：

```cpp
double max_elevation(const double *zbuf, int w, int h, int x, int y, double angle) {
    double maxang = 0;
    for (double dist = 1; dist < 30; dist += 1) {
        int sx = x + cos(angle)*dist;
        int sy = y + sin(angle)*dist;
        if (sx < 0 || sx >= w || sy < 0 || sy >= h) break;
        double dz  = zbuf[sx + sy*w] - zbuf[x + y*w];   // 深度差 = 高度差
        double dxy = dist * 2. / w;                     // 归一化平面距离
        maxang = max(maxang, atan2(dz, dxy));           // 仰角
    }
    return maxang;
}
```

每方向贡献 = π/2 − 最大仰角（可见天空角），8 方向平均 → AO 系数 → 逐像素乘回 framebuffer。

**副作用彩蛋**：邻居是背景时 dz 为巨大负数，仰角为负，被 `max(0, ·)` 天然免疫，无需特判。

## 第 11 站：卡通渲染（Toon Shading）

把连续光照强度 I 量化分档：

```cpp
if      (I > 0.85) I = 1.00;
else if (I > 0.60) I = 0.80;
else if (I > 0.45) I = 0.60;
else if (I > 0.30) I = 0.45;
else               I = 0.30;
```

`TOON` 常量开关控制。对 `I` 分档 vs 只对 `diff` 分档的区别：前者连高光一起量化（更彻底的平面化），后者保留连续高光。

## 踩坑记录

| 现象 | 原因 |
|---|---|
| 编译错误 cannot convert | 调用 `max_elevation` 参数顺序写错；`vector` 传裸指针要 `.data()` |
| ao.tga 全黑 | 算完 `total` 忘了 `ao.set()` 写回 |
| 图写到旧目录 | 复制文件后输出路径没跟着改 |
| AO 无效 | 系数算出来没乘回 framebuffer |

## 文件

- `render.cpp` — 完整最终管线：MVP → 光栅化 → 纹理/法线贴图 → Phong → 阴影 → 卡通分档 → AO 后处理
- `shadowmap.tga` / `ao.tga` / `output.tga` — 三个 pass 的产物

## 毕业

单图集成：透视相机 + 纹理 + 法线贴图 + 高光 + 阴影 + 卡通 + AO。
一个纯 CPU 的迷你渲染管线完成。下一步：Games101 / Real-Time Rendering。
