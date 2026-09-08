# tinyrenderer 学习笔记：Bresenham 画线与线框渲染（Day 1–2）

> 第一课：从画点到纯整数 Bresenham 直线算法的完整演化过程。
> 第二课：解析 OBJ 模型，用画线函数画出人脸线框。

## 0. 起步：画三个点

```cpp
#include "tgaimage.h"

constexpr TGAColor white   = {255, 255, 255, 255}; // 注意：BGRA 顺序
constexpr TGAColor green   = {  0, 255,   0, 255};
constexpr TGAColor red     = {  0,   0, 255, 255};
constexpr TGAColor blue    = {255, 128,  64, 255};
constexpr TGAColor yellow  = {  0, 200, 255, 255};

int main(int argc, char** argv) {
    constexpr int width  = 64;
    constexpr int height = 64;
    TGAImage framebuffer(width, height, TGAImage::RGB);

    int ax =  7, ay =  3;
    int bx = 12, by = 37;
    int cx = 62, cy = 53;

    framebuffer.set(ax, ay, white);
    framebuffer.set(bx, by, white);
    framebuffer.set(cx, cy, white);

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
```

### 补充：三种"常量"的区别

| 写法 | 性质 |
|---|---|
| `constexpr int width = 64;` | 编译期常量整数 |
| `const int x = 64;` | 只读，但不一定编译期确定 |
| `int y = 64;` | 普通变量，可改 |

## 1. 线段的参数方程

已知线段两端点 $A(a_x, a_y)$ 和 $B(b_x, b_y)$，线段上任意一点可以写成：

$$P(t) = (1-t)\cdot A + t\cdot B,\quad t \in [0,1]$$

- $t=0$ 时，$P = A$（在起点）
- $t=1$ 时，$P = B$（在终点）
- $t=0.5$ 时，$P$ 是线段中点

这就是画线函数的核心：让 $t$ 从 0 走到 1，把经过的点画出来。

### 为什么叫"重心坐标"

两个系数 $(1-t)$ 和 $t$ 满足 $(1-t) + t = 1$。

**物理类比**：在点 $A$ 放一个 $1-t$ 公斤的砝码，在点 $B$ 放一个 $t$ 公斤的砝码，总质量 1 公斤。这个系统的**质心（重心）**恰好就在点 $P(t)$ 处：

- 砝码全在 $A$ 端（$t=0$）→ 重心在 $A$
- 两边各半斤八两（$t=0.5$）→ 重心在中点

点 $P$ 由权重对 $(1-t,\ t)$ 唯一确定，这组权重就叫做点 $P$ 相对于线段 $AB$ 的**重心坐标**。

## 2. 第一种方法：按 t 采样

```cpp
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    for (float t=0.; t<1.; t+=.02) {
        int x = std::round( ax + (bx-ax)*t );
        int y = std::round( ay + (by-ay)*t );
        framebuffer.set(x, y, color);
    }
}
```

**缺陷**：步长 0.02 是拍脑袋定的——线长了会有缝隙，线短了会重复画同一个像素。

### 为什么用 round 而不是强转

`round` 的作用是四舍五入取整，把浮点坐标变成整数像素坐标：

| 方式 | 23.5 → | 23.2 → | -3.7 → |
|---|---|---|---|
| `(int)x` 强转 | 23（直接砍掉小数） | 23 | -3 |
| `round(x)` | 24（取最近的整数） | 23 | -4 |

强转是**截断**（向零取整），画出来的线会系统性地偏向左下，产生偏差；`round` 取最近的像素，画出的线最贴近真实直线，视觉上最平滑。

## 3. 第二次尝试：转置 + 从左到右

思路：不再按 $t$ 走，而是按 $x$ 逐像素走，保证没有缝隙。两个预处理：

1. **陡线转置**：如果线太"陡"（$\lvert\Delta y\rvert > \lvert\Delta x\rvert$），交换 x、y，把它变成"平"线来画，画的时候再换回来；
2. **强制从左到右**：如果 $a_x > b_x$，交换两端点。

```cpp
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    bool steep = std::abs(ax-bx) < std::abs(ay-by);
    if (steep) { // 陡线：转置图像
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx) { // 强制从左到右
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    for (int x=ax; x<=bx; x++) {
        float t = (x-ax) / static_cast<float>(bx-ax);
        int y = std::round( ay + (by-ay)*t );
        if (steep) // 转置过的画回来
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
    }
}
```

### 随机图像压测

```cpp
std::srand(std::time({}));
for (int i=0; i<(1<<24); i++) {
    int ax = rand()%width, ay = rand()%height;
    int bx = rand()%width, by = rand()%height;
    line(ax, ay, bx, by, framebuffer, { rand()%255, rand()%255, rand()%255, rand()%255 });
}
```

编译运行：

```bash
g++ ../tgaimage.cpp ../main.cpp -O3 -Wno-narrowing && time ./a.out
```

```
real    0m3.694s
user    0m3.690s
sys     0m0.004s
```

## 4. 优化一：把乘法变成加法

原写法循环每次都要算：

$$y(x) = a_y + (x - a_x)\cdot\frac{b_y - a_y}{b_x - a_x}$$

包含 1 次除法 + 1 次乘法 + 若干加减。

但循环里 $x - a_x$ 是连续取 $0, 1, 2, 3, \dots$ 的，也就是说 $y$ 的值构成一个**等差数列**：

$$y_0 = a_y,\quad y_1 = y_0 + k,\quad y_2 = y_1 + k,\ \dots$$

其中斜率 $k = \dfrac{b_y - a_y}{b_x - a_x}$ 在同一条线上是常数。既然相邻两项只差一个固定的 $k$，就没必要每次从头乘，直接累加即可：

```cpp
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    bool steep = std::abs(ax-bx) < std::abs(ay-by);
    if (steep) {
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    float y = ay;                          // y 现在是浮点
    float k = float(by-ay) / (bx-ax);      // 斜率只算一次
    for (int x = ax; x <= bx; x++) {
        if (steep)
            framebuffer.set(std::round(y), x, color);
        else
            framebuffer.set(x, std::round(y), color);
        y += k;                            // 每步只加一次
    }
}
```

循环体从"除法 + 乘法"变成 **1 次加法**。乘除法在 CPU 上比加法慢一个量级，循环百万次时差距可观：

```
real    0m2.646s   ← 之前 3.694s
user    0m2.643s
sys     0m0.000s
```

## 5. 优化二：误差计数器（干掉 y 的浮点）

经过转置处理后，线总是"偏水平"的，即斜率满足 $|k| \le 1$。这意味着 x 每走 1 格，y 最多变化 1 格。既然 y 的变化只有"不变 / +1 / −1"三种可能，就没必要用 float 了——用一个 `int y` 加一个"还差多少就该进位"的**误差计数器**就够了：

```cpp
int   y      = ay;                                // y 回到整数
float ierror = 0;                                 // 累积的小数误差
float error  = std::abs(float(by-ay) / (bx-ax));  // = |k|，每步该涨的小数部分
for (int x = ax; x <= bx; x++) {
    if (steep) framebuffer.set(y, x, color);
    else       framebuffer.set(x, y, color);

    ierror += error;                // 累加这一步的斜率
    if (ierror > .5) {              // 误差超过半个像素
        y += (by > ay ? 1 : -1);    // y 进一格（方向取决于上升/下降）
        ierror -= 1.;               // 误差扣掉 1，继续累积
    }
}
```

### 为什么阈值是 0.5

这正好等价于 `round`。真值 $y^* = a_y + k(x - a_x)$，整数 y 固定不动时，ierror 累积的就是"真值超出当前 y 的小数部分"：

- `ierror < 0.5` → 真值离当前 y 更近 → 不动（相当于**舍**）
- `ierror > 0.5` → 真值离上一行更近 → y 进 1（相当于**入**）

以红线 $k = 50/55 \approx 0.909$ 为例：

| x | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|
| ierror 累加后 | 0.91 | 0.82 | 0.73 | 0.64 | 0.55 |
| y | 3（进位后） | 4 | 5 | 6 | 7 |

每步 `ierror + 0.91` → 超 0.5 则 `y+1`、`ierror − 1`，几乎每格都进位，和真值 $y = 3 + 0.909(x-7)$ 四舍五入的结果完全一致。

## 6. 优化三：纯整数 Bresenham（干掉最后的浮点）

上一版还有三个浮点量：

$$\text{error} = \frac{|b_y - a_y|}{b_x - a_x},\quad \text{ierror 每次加 error},\quad \text{阈值 } 0.5$$

观察代码会发现，它们只以"比较大小"的形式出现（`ierror + error > 0.5` 这类判断）。**不等式两边同乘一个正数，结果不变**，那就全部乘以 $2(b_x - a_x)$：

$$\text{error}\cdot 2\Delta x = \frac{|\Delta y|}{\Delta x}\cdot 2\Delta x = \underbrace{2|\Delta y|}_{\text{整数！}},\qquad 0.5\cdot 2\Delta x = \underbrace{\Delta x}_{\text{整数！}}$$

除法没了、小数没了，只剩整数。

### 最终代码

```cpp
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    bool steep = std::abs(ax-bx) < std::abs(ay-by);
    if (steep) {                    // 陡线转置
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx) {                  // 强制从左到右
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x = ax; x <= bx; x++) {
        if (steep) framebuffer.set(y, x, color);
        else       framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by-ay);      // += 2|Δy|
        if (ierror > bx - ax) {             // 阈值 Δx
            y += (by > ay ? 1 : -1);
            ierror -= 2 * (bx - ax);        // -= 2Δx
        }
    }
}
```

### 验证（还是红线，Δx = 55，Δy = 50）

放大后的量：每步 `ierror += 100`，阈值 55，超了就 `−110`：

| x | 7 | 8 | 9 | 10 | 11 |
|---|---|---|---|---|---|
| 逐步 | +100=100>55 → y+1，−110 → −10 | +100=90>55 → y+1 → −20 | → 80 → −30 | → 70 → −40 | → 60 → −50 |
| y | 4 | 5 | 6 | 7 | 8 |

和上一版浮点结果逐点一致——因为两边只是同乘 $2\Delta x$ 的缩放，判断时机完全相同。

### 至此达成的目标

- **纯整数运算**：循环体只有加、减、比较，CPU 最便宜的指令；
- **零累积误差**：没有浮点，画任意长的线末端都不偏；
- **无间隙、方向无关、支持陡线和竖线**。

这就是完整的 **Bresenham 直线算法**，也是硬件光栅化里画线/扫描线的思想源头。

## 7. 番外：无分支版本（速度最快）

用布尔值隐式转 0/1 代替 `if` 分支，避免分支预测失败：

```cpp
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    bool steep = std::abs(ax-bx) < std::abs(ay-by);
    if (steep) {
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx) {
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x=ax; x<=bx; x++) {
        if (steep)
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by-ay);
        y      += (by > ay ? 1 : -1) * (ierror > bx - ax);
        ierror -= 2 * (bx-ax)        * (ierror > bx - ax);
    }
}
```

## 8. 附：投影公式为什么长这样

$$x_{screen} = (x+1)\cdot\frac{\text{width}}{2},\qquad y_{screen} = (y+1)\cdot\frac{\text{height}}{2}$$

核心就一件事：**把 $[-1,1]$ 这个区间线性映射到 $[0, \text{width}]$ 这个区间**。

OBJ 模型的顶点坐标是归一化过的，x、y 分量大致落在 $[-1,1]$ 里（人脸中心在原点附近）。但画布坐标是像素编号，范围是 $[0,800]$，没有负数。分三步看：

1. $x + 1$：把 $[-1,1]$ 平移到 $[0,2]$ —— 消除负数；
2. $\div 2$：把 $[0,2]$ 压缩到 $[0,1]$ —— 归一化成"比例"；
3. $\times\ \text{width}$：把 $[0,1]$ 放大到 $[0,800]$ —— 比例变成像素坐标。

从函数角度看，这就是一条直线 $f(x) = ax + b$，满足 $f(-1)=0$、$f(1)=\text{width}$，解得 $a = b = \text{width}/2$，与上式完全等价。这类"保持比例不变的区间拉伸 + 平移"叫**仿射变换**，是图形学里最基础的变换。

---

# Day 2：线框渲染（wireframe.cpp）

## 9. Model 类：解析 OBJ 文件

OBJ 是文本格式，每行以标签开头：

```
v  -0.0005  0.234  0.107            ← 顶点坐标
vt  0.532   0.923                   ← 纹理坐标（忽略）
vn  0.1     0.2    0.9              ← 法线（忽略）
f  24/24/24  25/25/25  26/26/26     ← 面：顶点/纹理/法线 三组索引
```

解析思路：逐行读 → 取第一个词判断类型 → `v` 存顶点，`f` 存索引，其余行自然忽略。

```cpp
struct Model {
    vector<vec3> verts;              // 所有顶点
    vector<vector<int>> faces;       // 每个面的顶点索引

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
                while (iss >> idx) {
                    if (iss.peek() == '/') {      // 后面跟着纹理/法线索引
                        while (iss.peek() != ' ' && iss.good())
                            iss.get();            // 把 "/vt/vn" 逐字符丢弃
                    }
                    f.push_back(idx - 1);         // OBJ 索引从 1 开始，vector 从 0 开始
                }
                faces.push_back(f);
            }
        }
    }
};
```

### `f` 行解析的关键技巧

难点在于 `24/24/24` 这种三段式：

- `iss >> idx` 读整数时自动停在非数字字符（`/`）上，精确切出每段第一个数字；
- `peek()` 只看不取，判断下一个字符是不是 `/`；
- 是的话用 `get()` 把 `/纹理/法线` 部分逐个字符吞掉，直到遇到空格或行尾。

### ⚠️ 坑：push_back 的位置

最初版本把 `f.push_back(idx - 1)` 写在了 `if (peek()=='/')` **里面**。对 african_head 没问题（面全是 `v/vt/vn` 格式），但如果 OBJ 的面写成光秃秃的 `f 1 2 3`（没有斜杠），索引就一个都存不进去，画出空白。**push 要挪到 if 外面**，不管有没有斜杠都存。

另一个注意点：构造函数没检查文件是否打开成功，路径错了会**静默画出全黑图**——输出全黑时先查 `obj/african_head/african_head.obj` 这个相对路径。

## 10. 主循环：遍历每个面，逐边连线

```cpp
int main() {
    Model model("obj/african_head/african_head.obj");
    constexpr int width = 800, height = 800;
    TGAImage framebuffer(width, height, TGAImage::RGB);
    constexpr TGAColor white = {255, 255, 255, 255};

    for (int i = 0; i < model.faces.size(); i++) {   // 遍历每个三角形面
        const auto &face = model.faces[i];
        for (int j = 0; j < face.size(); j++) {      // 遍历面的每条边
            int idx1 = face[j];
            int idx2 = face[(j+1) % face.size()];    // 下一个顶点，末尾绕回开头
            vec3 v1 = model.verts[idx1];
            vec3 v2 = model.verts[idx2];
            int x1 = (v1.x + 1.) * width  / 2.;      // 投影（见第 8 节）
            int y1 = (v1.y + 1.) * height / 2.;
            int x2 = (v2.x + 1.) * width  / 2.;
            int y2 = (v2.y + 1.) * height / 2.;
            line(x1, y1, x2, y2, framebuffer, white);
        }
    }
    framebuffer.write_tga_file("output2.tga");
    return 0;
}
```

两个关键技巧：

- **`(j+1) % face.size()`**：取模让最后一个顶点的"下一个"绕回第 0 个，三条边（0→1，1→2，2→0）都画到，三角形自动闭合，无需特殊判断；
- **共享边重复画**：相邻三角形共享的边会被画两遍，白线叠白线看不出来，练习阶段无所谓。

## 11. 编译运行与验证

```bash
g++ -std=c++17 wireframe.cpp tgaimage.cpp -o wireframe
./wireframe
```

打开 `output2.tga`：白线三角形拼成的正面人脸网格，眼窝、鼻梁处网格更密，能看出五官起伏即说明**模型加载、投影、画线三步全通**。

小整洁项：`int i < model.faces.size()` 有 signed/unsigned 比较警告，写成 `for (size_t i = 0; ...)` 或范围 for `for (const auto &face : model.faces)` 更干净。

---

**下一步**：三角形光栅化（重心坐标派上用场的地方）。
