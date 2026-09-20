# Primer: 重心坐标 (Barycentric Coordinates)

## 一维（线段）

线段两端 $A$、$B$，线上点 $P$ 可表示为加权组合：

$$P = \alpha A + \beta B,\qquad \alpha + \beta = 1$$

- 物理意义：在 $A$ 放 $\alpha$ kg、$B$ 放 $\beta$ kg 砝码，总重 1 kg，系统质心恰在 $P$;
- 求解：$\alpha = \dfrac{B-P}{B-A}$，$\beta = \dfrac{P-A}{B-A}$ —— **"对面的距离 ÷ 全长"**;
- 约束 $\alpha+\beta=1$ 消除了冗余（否则同乘任意倍数质心不变），使每个 $P$ 对应唯一权重。

符号判读：

| $\alpha,\beta$ | $P$ 位置 |
|---|---|
| 都在 $(0,1)$ | 线段内部 |
| 有一个 $=0$ | 端点 |
| 有一个 $<0$ | 线段外（负权重 = 往上抬） |

应用：画线时 `t = (x-ax)/(bx-ax)` 求出权重，再用 `(1-t)*ay + t*by` 插值出 y——**先在一个域上求权重，再拿到另一个域上插值**，这是重心坐标的核心用法。

## 二维（三角形）

$$P = \alpha A + \beta B + \gamma C,\qquad \alpha+\beta+\gamma=1$$

**面积比公式**（几何直观）：连接 $P$ 与三顶点，分成三个子三角形：

$$\alpha=\frac{S_{PBC}}{S_{ABC}},\quad \beta=\frac{S_{APC}}{S_{ABC}},\quad \gamma=\frac{S_{ABP}}{S_{ABC}}$$

口诀同样是"**对面**"：顶点权重由它与对边构成的子三角形面积决定。三面积之和 = 总面积 ⇒ 权重和自动为 1。

**鞋带公式**算有向面积（点序决定符号，勿乱序）：

$$2S = (b_x-a_x)(c_y-a_y) - (b_y-a_y)(c_x-a_x)$$

符号判读：

| $(\alpha,\beta,\gamma)$ | $P$ 位置 |
|---|---|
| 全在 $(0,1)$ | 严格内部 |
| 恰一个 $=0$ | 在边上 |
| 恰两个 $=0$ | 是顶点 |
| 任一个 $<0$ | 三角形外 |

**好习惯**：算完先检查 $\alpha+\beta+\gamma=1$，最便宜的自检。

## 与光栅化代码的对应

`cover_triangle` 里的三个叉积 `a, b, c` 就是三个子三角形面积的 2 倍——**填充算法早就在算重心坐标了**，只是没除以总面积。

注意交叉对应（易错点）：

| 叉积 | 涉及边 | 对面子三角形 | 对应权重 |
|---|---|---|---|
| `a`(AB×AP) | 边 AB | △ABP | $\gamma$（顶点 C） |
| `b`(BC×BP) | 边 BC | △BCP | $\alpha$（顶点 A） |
| `c`(CA×CP) | 边 CA | △CAP | $\beta$（顶点 B） |

顶点绕序统一后（`total_area >= 1` 剔除），内部点判断从"全同号"简化为 `a>=0 && b>=0 && c>=0`。

## 插值应用

每个像素的任意属性 = 顶点属性按重心坐标加权：

$$attr(P) = \alpha\cdot attr_A + \beta\cdot attr_B + \gamma\cdot attr_C$$

已实现：RGB 颜色渐变三角形（`triangle_gradient`，中心处 $\alpha=\beta=\gamma=1/3$ 呈灰色）。

后续同样机制用于插值：**z 深度**（z-buffer）、**纹理坐标 uv**、**法线**（光照）。

## 踩坑记录

- 测试三角形顶点绕序写反（顺时针）→ `total_area < 1` 误杀 → 全黑；
- `TGAColor` 无 `.r/.g/.b` 成员，用 `color[0..2]`（BGR 顺序）访问；
- 函数参数名 `fb` 与函数体内 `framebuffer` 不一致 → 编译报错，前后命名要统一。
