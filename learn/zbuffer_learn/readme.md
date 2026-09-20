# Lesson 3: 隐藏面消除 (z-buffer) + Lambert 明暗着色

## 隐藏面消除的三代方案

1. **背面剔除**：一行有向面积判断，剔除背对相机的三角形。缺陷：同向三角形仍"谁后画谁赢"（口腔盖住嘴唇）。
2. **画家算法**：按深度排序三角形，从远到近画。缺陷：相机一动就要重排；三角形互相穿插时根本不存在正确顺序（三角形不适合全局排序）。
3. **z-buffer（每像素画家算法）**：排序粒度细化到像素。三角形不好排序，但像素深度永远可比较。

## z-buffer 核心三行

```cpp
if (zbuffer[idx] < z) {   // 候选像素比已画的更近？
    zbuffer[idx] = z;     // 更新深度记录
    fb.set(j, i, color);  // 画上颜色
}
```

- `zbuffer`：与画布同尺寸的一维 double 数组，初始化为 `-1e9`（无穷远）；
- `z`：由重心坐标插值得到 `alpha*v1.z + beta*v2.z + gamma*v3.z`；
- **结果与绘制顺序无关**——这是它取代画家算法的根本原因；
- 深度不需要映射到 [0,255]，映射只是为了输出灰度深度图；算法本身只要求映射保序且一致。

下标换算（行优先布局）：

```cpp
int idx = j + i * fb.width();   // idx = y*width + x
```

## Lambert 漫反射着色

亮度 = 法线与光方向夹角的余弦：

$$intensity = \vec n \cdot \vec l = \cos\theta$$

```cpp
vec3 n = normalized(cross(v1-v2, v1-v3));  // 法线：两条边的叉积，归一化
vec3 light_dir = normalized(vec3{1,1,1});  // 光从右上前上方照来
double intensity = n * light_dir;          // operator* 即点积
if (intensity <= 0) continue;              // 背光面跳过
```

- 叉积**参数顺序决定法线朝向**（右手定则），反了整张脸全黑——对调两个参数即可翻转；
- 必须**归一化**，否则点积不是纯净的 cosθ；
- `intensity <= 0` 的剔除顺带替代了背面剔除。

## 踩坑记录

| 现象 | 原因 |
|---|---|
| `dot` 未声明 | geometry.h 里点积是 `operator*`，不是 `dot()` |
| `get_width()` 报错 | 本仓库 API 是 `fb.width()` / `fb.height()` |
| 插值写成 `alpha*a.z` | `a,b,c` 是叉积不是顶点；顶点要换名（v1~v3）避免冲突 |
| 全黑 | 叉积顺序导致法线朝内；或光方向与法线整体反向 |
| 旧二进制"正常运行"的假象 | `g++ ... && ./prog` 编译失败时 `&&` 链中断，但用 `;` 或管道分隔时会跑到旧程序——改代码后务必确认编译成功 |

## 文件

- `zbuffer.cpp` — 完整管线：obj 解析 → 投影 → 包围盒 → 重心坐标 → z 插值 → z-buffer → Lambert 着色
- `output.tga` — 斜向光灰度人头

## 下一步

geometry.h 数学库作业（vec2/vec4、mat 矩阵乘法/转置/求逆）→ 相机与透视投影 → 纹理贴图。
