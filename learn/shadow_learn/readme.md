# 第 9 站：阴影映射 (Shadow Mapping)

## 核心思想

**光源看不见的点，就在阴影里。**

```
Pass 1: 从光源位置渲染 → 只记录深度（shadowmap）
Pass 2: 相机正常渲染，每像素问一句：
        "我在光源深度图里的记录比我自己近吗？" 近 → 被挡 → 阴影
```

## 架构重构（本站的工程重点）

阴影需要两套 MVP 渲染两遍，渲染逻辑必须抽成函数：

```cpp
void render(Model&, mat<4,4> &MVP, TGAImage&, double *zbuffer,
            bool depthOnly, ...);   // depthOnly=true: pass1 只写深度
```

铁律：**重构不改行为**——先抽函数，确认画面和上一站完全一致，再加新功能。

## 关键数据流

`triangle()` 新增三类参数：

| 参数 | 作用 |
|---|---|
| `w1,w2,w3`（世界坐标） | 顶点经 MVP 变换后丢失了世界位置，必须额外传一份，像素循环里插值出 `wp` |
| `depthOnly` | pass1 跳过所有采样/着色，只更新 zbuffer |
| `shadowmap, lightMVP` | pass2 查询用 |

像素级阴影查询：

```cpp
vec3 wp = alpha*w1 + beta*w2 + gamma*w3;        // 插值世界坐标
vec4 lp = lightMVP * vec4{wp.x, wp.y, wp.z, 1};
lp.x /= lp.w; lp.y /= lp.w; lp.z /= lp.w;       // 透视除法（和顶点同款流程）
int sx = (lp.x + 1.) * width / 2.;
int sy = (lp.y + 1.) * height / 2.;
if (lz < shadowmap[sy*width + sx] - bias)       // 比记录远 → 阴影
    I = ka;                                      // 只留环境光
```

## 踩坑记录

| 现象 | 原因 |
|---|---|
| 画面无阴影 | **深度比较方向写反**：约定"z 大=近"，阴影条件应为 `lz < shadow记录 - bias`；写反后条件几乎不触发 |
| 偶发崩溃/花屏 | `sx,sy` 越界：点在光源视野外时查表越界，必须先做范围检查 |
| 满屏条纹（shadow acne） | 同一表面两处计算的深度有浮点误差，自己挡住自己 → 加 bias（本次 0.005 合适） |
| bias 过大 | 阴影脱离物体（"悬浮"感），需要在条纹和悬浮间权衡 |

## 验证

- `shadowmap.tga`：光源视角灰度深度图（轮廓清晰即可，五官模糊正常）;
- `output.tga`：下巴在脖子上投出影子，无条纹。

## 下一站

AO（环境光遮蔽）：眼窝、唇缝等凹陷处采样 AO 贴图变暗。
