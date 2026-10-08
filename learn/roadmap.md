# TinyRenderer 学习简报与剩余计划

> 本文档交给带教 AI 使用：包含项目现状、代码约定、已完成内容、剩余六站的详细教学计划。
> 学习者：已理解每行代码的初学者，偏好"讲原理 → 小练习 → 自己写 → 验收"的节奏，中文教学。

---

## 0. 项目现状（带教前必读）

### 目录结构

```
tinyrenderer/
├── geometry.h          # 数学库：vec2/3/4、mat<R,C>（乘法/转置/求逆）
├── tgaimage.h/.cpp     # TGA 图像读写
├── obj/                # 模型：african_head/（含 diffuse/spec/nm 贴图）、boggie/、diablo3_pose/
└── learn/              # 学习代码（每课一个子目录）
    ├── camera_learn/render.cpp   # ★ 当前主力渲染器，后续站都在它基础上改
    └── ...
```

### 编译运行（统一在项目根目录）

```bash
g++ -std=c++17 learn/camera_learn/render.cpp tgaimage.cpp -o learn/camera_learn/test
./learn/camera_learn/test    # 必须根目录运行，obj 是相对路径
```

### 关键 API 约定（易错点）

- `TGAColor`：无构造函数、无 `.r/.g/.b`，内部是 `bgra[4]` 数组。赋值用 `TGAColor{b,g,r,a}`，访问用 `color[0..3]`（0=蓝 2=红）；
- `vec3` 的 `operator*` 是**点积**（不是逐分量乘）；叉积是 `cross(a,b)`；归一化 `normalized(v)`；
- `mat<4,4>`：逐行赋值 `M[i] = {a,b,c,d}`；`mat*vec4`、`mat*mat` 已定义；
- `TGAImage`：`w()`/`h()` 不存在，用 `width()`/`height()`；`get(x,y)`/`set(x,y,color)`；`read_tga_file(path)`；
- `vec4` 有 `.xyz()` 切片。

### render.cpp 当前管线（已完成，勿改坏）

```
main:
  Model model(...)                        // 解析 v/vt/vn/f（f 的三段索引都存了）
  mat Mv = lookat(eye, center, up)        // 视图矩阵
  mat P;  P[3][2] = -1/c                  // 透视矩阵，c=|eye-center|
  mat MVP = P * Mv
  for each face:
    v1..v3 = MVP * 顶点（vec4），透视除法 t.x/t.w
    it1..it3 = max(0, vertexNormal · light)   // 顶点处算亮度（世界空间）
    triangle(v1,v2,v3, it1,it2,it3, framebuffer, zbuffer)

triangle():
  视口变换 (x+1)*w/2  → 包围盒 → total_area<1 剔除
  → 逐像素算重心坐标 (α,β,γ)（注意交叉对应：叉积 a↔γ、b↔α、c↔β）
  → 插值 z、插值亮度 → zbuffer 比较 → 颜色×亮度 → set
```

### Model 现有字段

```cpp
vector<vec3> verts, norms;   vector<vec2> tex;
vector<vector<int>> faces, face_norms, face_tex;   // f 行三段索引（已减1）
```

---

## 第 6 站：纹理贴图 ✅（已完成，见 texture_learn/）

**概念**：每个顶点带 uv∈[0,1]²（贴图上的位置）；面内 uv 用重心坐标插值；颜色=贴图采样×亮度。

**步骤**：
1. main 循环外加载：`TGAImage diffuse; diffuse.read_tga_file("obj/african_head/african_head_diffuse.tga")`（检查返回值）;
2. 循环内取 `vec2 uv = model.tex[model.face_tex[i][k]]`(k=0,1,2);
3. triangle 加三个 vec2 参数，像素循环插值 uv;
4. `TGAColor c = diffuse.get(int(u*(diffuse.width()-1)), int(v*(diffuse.height()-1)))`;
5. 最终色 = c 各通道 × 插值亮度。

**坑**：v 轴方向反了会五官错位 → 采样时 `v = 1-v` 翻转。
**验证**：斜视角人头有肤色、红唇、眼白。
**理论检查题**：为什么 uv 插值和 z 插值用的是同一组重心坐标？

---

## 第 7 站：Phong 光照 + 高光贴图 ✅（phong_learn/）

**概念**：
- Phong 模型 = 环境光 + 漫反射 + 高光：$I = k_a + k_d(\vec n\cdot\vec l) + k_s\max(0,\vec r\cdot\vec v)^s$
- 反射向量 $\vec r = 2(\vec n\cdot\vec l)\vec n - \vec l$；$\vec v$ 是视线方向
- **从 Gouraud（插值亮度）升级到 Phong shading（插值法线，逐像素算光）**——高光必须逐像素，否则糊掉
- 高光指数 s 从 `african_head_spec.tga` 采样（越亮越光滑）

**步骤**：
1. triangle 改为传三条 vec3 法线（不再传亮度），插值法线后**重新归一化**；
2. 逐像素算 $\vec n\cdot\vec l$ 和高光项；
3. spec 贴图采样作为高光指数/系数。

**验证**：额头、鼻梁出现集中的油光高光；皮肤仍有漫反射过渡。
**检查题**：为什么 Gouraud 做不出集中的高光点？（提示：插值会抹平峰值）

---

## 第 8 站：法线贴图与切空间 ✅（normalmap_learn/）

**概念**：
- 法线贴图 `african_head_nm_tangent.tga`：RGB 编码扰动法线，让低模表面"假装"有细节；
- 贴图里的法线存在**切空间**（T=切线、B=副切线、N=法线构成的局部坐标系）;
- 由三角形三顶点的 (位置, uv) 解出 T、B：$T \propto$ uv 的 u 方向对应的世界方向；
- 法线变换用 $(M^{-1})^T$（geometry.h 的 `invert_transpose()` 首次实战）。

**步骤**：
1. 对每面解线性方程组求 T、B（教程有推导，本质是 2×3 方程）;
2. 组 TBN 矩阵；贴图采样 RGB→映射 [-1,1]³→切空间法线；
3. TBN × 切空间法线 → 世界法线 → 逐像素光照。

**验证**：皮肤出现毛孔、皱纹级细节，超过网格几何精度。
**教学方式建议**：这站数学重，先带学"为什么法线贴图用切空间存"（换三角形贴图还能复用），再推 TBN。

---

## 第 9 站：阴影映射（Shadow mapping）✅（shadow_learn/）

**概念**：
- 两遍渲染：第一遍从光源视角画深度图；第二遍相机视角，每像素变换到光源空间比较深度——"光源看不见的点就在阴影里";
- shadow acne（自遮挡条纹）→ 加 bias 修正；
- 需要把渲染器重构成"传入 MVP + zbuffer 指针"的可复用函数。

**步骤**：
1. 重构：抽出 `render(model, MVP, framebuffer, zbuffer)`;
2. pass1：光源 MVP → 深度图存数组；
3. pass2：相机 MVP 渲染，每像素算光源空间坐标 → 比深度 → 阴影中则只留环境光。

**验证**：鼻子在脸颊投下影子。

---

## 第 10 站：环境光遮蔽（AO)✅（final_learn/）

**概念**：凹陷处环境光被遮挡 → 采样 AO 贴图乘到环境光项（教程用预计算贴图，简单）。
**验证**：眼窝、唇缝柔和变暗。

---

## 第 11 站：卡通渲染（番外）✅（final_learn/）

`intensity` 量化分档：`if (intensity>.85) i=1; else if (.60) i=.80; ...`
**验证**：赛璐璐风格。

---

## 毕业标准

单张图同时含：透视相机 + 纹理 + 法线贴图 + 高光 + 阴影 + AO。
后续方向：Games101 / 《Real-Time Rendering》。

---

## 带教注意事项（给 AI 的元指令）

1. 学习者要求自己写代码，给骨架+填空，不要直接给完整实现（除非他明确要求）;
2. 每步配数字小练习（他吃这套）;
3. 新踩的坑补充进对应 learn 目录的 readme.md;
4. 每站完成后提醒 git add/commit/push（代理已配好：http://172.20.32.1:7892）;
5. 统筹/答疑/验收由 Kimi 负责，遇到方向性问题可以让学习者回来找我。
