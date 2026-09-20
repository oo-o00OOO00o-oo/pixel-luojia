# 数学库精读笔记：geometry.h

> 任务来源：教程作业（实现 vec/mat）。仓库自带完成版，故改为逐段精读 + 笔记。
> 这份笔记按"存储 → 乘法 → 行列式 → 求逆 → 为什么需要齐次坐标"的顺序展开，每节配推导和例子。

---

## 1. 向量体系：一个模板 + 三个特化

```cpp
template<int n> struct vec { double data[n]; ... };   // 通用版：数组存储
template<> struct vec<2> { double x, y; ... };        // 特化版：命名成员
template<> struct vec<3> { double x, y, z; ... };
template<> struct vec<4> { double x, y, z, w; ... };
```

**为什么要特化？** 通用模板里只能用 `v[0]`, `v[1]` 访问，可读性差；特化后能用 `v.x`, `v.y` 这样的名字，同时保留 `v[i]` 下标（特化里的 `operator[]` 把两种访问方式桥接起来）。

**运算符重载的语义约定**（容易混淆，记下来）：

| 表达式 | 含义 |
|---|---|
| `v1 + v2` | 逐分量相加 |
| `v1 * v2` | **点积**（返回 double，不是逐分量乘！） |
| `v * 2.0` | 数乘 |
| `cross(v1, v2)` | 叉积（仅 vec3） |
| `norm(v)` / `normalized(v)` | 模长 / 单位化 |

点积用 `operator*` 的后果：写光照时 `n * light_dir` 就是 `dot(n, light_dir)`——第一次见会以为是别的，注意。

`vec4` 还提供了 `xy()` / `xyz()` 切片，用于从齐次坐标取回三维部分（相机课会用到）。

---

## 2. 矩阵的存储：行向量数组

```cpp
template<int nrows,int ncols> struct mat {
    vec<ncols> rows[nrows];
```

设计动机：矩阵不直接用 `double m[4][4]`，而是复用 vec：

- `m[i]` 取出第 i **行**，是一个 `vec<ncols>`；
- 再 `[j]` 一次取列 → `m[i][j]` 是**两次一维下标**拼出的二维访问；
- 行级运算（点积、数乘、加减）全部继承自 vec，mat 里几乎不用写逐元素循环。

代价：列访问不方便（没有 `m.col(j)`），所以后面求逆时作者特意用了转置技巧把"列操作"变成"行操作"（见第 5 节）。

---

## 3. 矩阵乘法

定义：$(AB)_{ij} = \sum_k A_{ik}B_{kj}$，即**左行 × 右列**的点积。

```cpp
template<int R1,int C1,int C2>
mat<R1,C2> operator*(const mat<R1,C1>& lhs, const mat<C1,C2>& rhs);
```

**模板参数就是维度规则**：内维必须相同（C1 同时出现在两个参数里），否则编译期报错——类型系统免费承担了形状检查。

### 两个方向的"矩阵×向量"不同！

$$A v = \begin{pmatrix}1&2\\3&4\end{pmatrix}\begin{pmatrix}1\\1\end{pmatrix} = \begin{pmatrix}3\\7\end{pmatrix}
\qquad
v A = \begin{pmatrix}1&1\end{pmatrix}\begin{pmatrix}1&2\\3&4\end{pmatrix} = \begin{pmatrix}4&6\end{pmatrix}$$

- `mat × vec`（第 129 行）：`ret[i] = 第i行 · v`，列向量右乘；
- `vec × mat`（第 125 行）：把 vec 包装成 1×n 矩阵，**复用矩阵乘法**再取 `[0]`，一行解决。

**结论：矩阵乘法不可交换**。渲染管线的变换链 $M_{viewport} \cdot M_{proj} \cdot M_{view} \cdot P$ 从右往左读，顺序写反就是另一个变换。

---

## 4. 行列式：递归 + 模板元编程

数学依据——**拉普拉斯展开**（按第 0 行）：

$$\det A = \sum_{j} A_{0j}\, C_{0j},\qquad C_{0j} = (-1)^{0+j}\det(\text{删掉第0行第j列的子矩阵})$$

n 阶行列式归结为 n 个 n−1 阶行列式，递归到 1 阶（就是元素本身）终止。

代码为什么要包一层结构体？

```cpp
template<int n> struct dt { static double det(const mat<n,n>&); };
template<> struct dt<1> { static double det(const mat<1,1>& m) { return m[0][0]; } };
```

因为递归需要"模板参数 = 1 时换实现"，而 **C++ 不允许函数模板特化，只允许类模板特化**。所以把函数藏进类里，用 `dt<1>` 特化充当递归终点。这是模板元编程的标准手法。

### 手算验证（3×3)

$$A = \begin{pmatrix}1&2&3\\0&1&4\\5&6&0\end{pmatrix}$$

按第 0 行展开：

$$\det A = 1\cdot\begin{vmatrix}1&4\\6&0\end{vmatrix} - 2\cdot\begin{vmatrix}0&4\\5&0\end{vmatrix} + 3\cdot\begin{vmatrix}0&1\\5&6\end{vmatrix}
= 1(0-24) - 2(0-20) + 3(0-5) = -24+40-15 = 1$$

代码里 `cofactor(0,j)` 的 `((row+col)%2 ? -1 : 1)` 就对应展开式里的 $+-\,+\cdots$ 交错符号。

### cofactor 的跳行技巧

```cpp
submatrix[i][j] = rows[i + int(i>=row)][j + int(j>=col)];
```

`i` 小于被删行 `row` 时原样复制；`i >= row` 时读 `i+1` 行——正好跳过被删行。列同理。一个 `int(bool)` 转换代替了 if 分支。

---

## 5. 求逆：伴随矩阵法

数学公式：

$$A^{-1} = \frac{1}{\det A}\,\mathrm{adj}(A)$$

其中伴随矩阵 $\mathrm{adj}(A)$ 是**余子式矩阵的转置**: $\mathrm{adj}(A)_{ij} = C_{ji}$（注意下标交换！）。

代码：

```cpp
mat invert_transpose() {
    for (...) adjugate_transpose[i][j] = cofactor(i, j);   // 没按下标交换存！
    return adjugate_transpose / (adjugate_transpose[0] * rows[0]);
}
mat invert() { return invert_transpose().transpose(); }
```

三个值得注意的点：

1. **函数故意构造"转置版"**：`adjugate_transpose[i][j] = cofactor(i,j)` 直接得到 $(A^{-1})^T$。这不是笔误——**变换法线的正确公式是 $\vec n' = (M^{-1})^T\,\vec n$**（下一课解释为什么），预留这个函数避免到时候再转置；
2. **行列式没有单独算**：由拉普拉斯展开，$\det A = \sum_j A_{0j}C_{0j}$ = 原矩阵第 0 行 · 余子式第 0 行。而 `adjugate_transpose[0]` 正好装着 $C_{00}, C_{01}, \dots$，所以 `adjugate_transpose[0] * rows[0]` 一个点积就复用出 det，省掉一整遍递归；
3. **前提**：$\det A \neq 0$（可逆）。渲染变换矩阵都可逆，代码没做检查。

### 手算验证（2×2)

$A = \begin{pmatrix}1&2\\3&4\end{pmatrix}$，$\det A = 4-6 = -2$

$$A^{-1} = \frac{1}{-2}\begin{pmatrix}4&-2\\-3&1\end{pmatrix} = \begin{pmatrix}-2&1\\1.5&-0.5\end{pmatrix}$$

验证 $AA^{-1}$：

$$\begin{pmatrix}1&2\\3&4\end{pmatrix}\begin{pmatrix}-2&1\\1.5&-0.5\end{pmatrix} = \begin{pmatrix}-2+3 & 1-1\\-6+6 & 3-2\end{pmatrix} = \begin{pmatrix}1&0\\0&1\end{pmatrix}\checkmark$$

---

## 6. 为什么需要齐次坐标（衔接相机课）

**3×3 矩阵的表达极限**：它的输出每个分量都是 $ax+by+cz$ 形态——**纯线性组合，没有常数项**。这类"线性变换"（缩放/旋转/错切）有一个共同软肋：

$$M \cdot \vec 0 = \vec 0 \quad(\text{原点永远不动})$$

但**平移** $\vec p' = \vec p + \vec t$ 恰恰要移动原点！3×3 矩阵无论填什么数都表达不了。

**解法：升一维。** 给点加第 4 个分量 $w=1$，用 4×4 矩阵，把平移量藏进最后一列：

$$\begin{pmatrix} 1&0&0&t_x\\0&1&0&t_y\\0&0&1&t_z\\0&0&0&1 \end{pmatrix}\begin{pmatrix}x\\y\\z\\1\end{pmatrix} = \begin{pmatrix}x+t_x\\y+t_y\\z+t_z\\1\end{pmatrix}$$

妙处在于那个 $w=1$ 相当于"常数项开关"：

- **点**：$w=1$，平移生效；
- **方向向量**：$w=0$，平移自动失效（方向不该被平移！）——类型系统层面的优雅；
- 透视投影会让 $w \neq 1$，届时用 $(x/w, y/w, z/w)$ 除回来（**透视除法**，近大远小就藏在这一步）。

### 示例：视口变换的矩阵形态

我们手写的 `(x+1)*width/2` 正规化后：

$$M_{viewport} = \begin{pmatrix} w/2 & 0 & 0 & w/2\\0 & h/2 & 0 & h/2\\0 & 0 & d/2 & d/2\\0 & 0 & 0 & 1\end{pmatrix}$$

验证（列向量右乘）：

$$M_{viewport}\begin{pmatrix}x\\y\\z\\1\end{pmatrix} = \begin{pmatrix} \frac w2 x + \frac w2\\ \frac h2 y + \frac h2\\ \frac d2 z + \frac d2\\ 1\end{pmatrix} = \begin{pmatrix}(x+1)\frac w2\\(y+1)\frac h2\\(z+1)\frac d2\\1\end{pmatrix}\checkmark$$

缩放（对角线）和平移（最后一列）各就各位——这正是相机课所有变换矩阵的通用构造模式。

---

## 7. 管线预告

$$P_{screen} = \underbrace{M_{viewport}}_{\text{映射到画布}} \cdot \underbrace{M_{proj}}_{\text{透视（w≠1）}} \cdot \underbrace{M_{view}}_{\text{移动相机}} \cdot P_{model}$$

下一课逐一把这三个矩阵构造出来，全部用本笔记的 mat 实现。
