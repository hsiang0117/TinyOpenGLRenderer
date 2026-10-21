# 渲染代码命名

类使用 `PascalCase`，函数、变量和 Shader 注册名称使用 `lowerCamelCase`。名称优先说明用途；集合使用复数，数量使用 `Count`。渲染资源的缩写按单词书写，例如 `hdrFbo`、`cameraMatricesUbo`、`CloudVolumeGpu`。

## Shader

Shader 注册名称、`RenderContext` 成员和文件名使用相同的用途名称。共享的全屏顶点阶段保留独立名称 `fullscreen.vert`。

| 用途 | 注册名称 | 主要文件 | 原名称 |
| --- | --- | --- | --- |
| 前向着色 | `forward` | `forward.vert`、`forward.frag` | `default`、`test.*` |
| 方向光阴影 | `directionalShadow` | `directionalShadow.vert`、`directionalShadow.frag` | `depth`、`depth.*` |
| 点光源阴影 | `pointShadow` | `pointShadow.vert`、`pointShadow.geom`、`pointShadow.frag` | `depthCube`、`depthCube.*` |
| 最终合成 | `composite` | `fullscreen.vert`、`composite.frag` | `screenQuad`、`screenQuad.*` |
| 光源位置标记 | `lightMarker` | `lightMarker.vert`、`lightMarker.frag` | `lightCube`、`lightCube.*` |
| 骨骼叠加 | `skeleton` | `skeleton.vert`、`skeleton.frag` | `bone`、`bone.*` |
| 云光线步进 | `cloudRaymarch` | `cloudRaymarch.vert`、`cloudRaymarch.frag` | `volume`、`volume.*` |

`skybox`、`gaussianBlur`、`cloudUpsample`、`gizmo` 沿用其用途名称。更改 uniform 名称时，同时更新 GLSL 声明、C++ 设置调用、存在性检查和测试。

内置 Shader 的注册名称和阶段路径统一在 `assets/shaderCatalog.hpp` 中声明。查询时使用 `ShaderCatalog::forward.key` 等常量，避免在调用方重复字符串。新增或重命名内置 Shader 时，同步维护声明表、GLSL 文件和使用它的渲染上下文；uniform 名称按 GLSL 的实际声明单独维护。

## 场景与接口

- `ModelRenderComponent` 表示模型绘制组件，原名 `RenderComponent`；对应视图为 `modelObjects`。
- `materialMeshObjects` 对应带 `DynamicMaterialComponent` 的对象，原名 `staticMeshes`。
- `CloudVolumeComponent` 保存云体参数，原名 `VolumeMarkerComponent`；工厂入口为 `createCloudVolume()`，默认创建 `Mode::SkyLayer`。天空云层的高度与追踪范围由组件控制，`Mode::LocalVolume` 沿用 Transform 包围盒。
- 方向光统一使用 `DirectionalLight`，天空盒统一使用 `Skybox`。
- 当前云与骨骼共用的 Pass 名为 `CloudSkeletonOverlayPass`；计时标签为 `CloudSkeletonOverlay`。
- 释放接口统一为 `shutdown()`，Shader 热重载为 `reload()`，组件面板注册为 `registerComponentWidgets()`。
- 统计字段使用 `opaqueObjectCount`、`pointLightCount`、`directionalLightCount`、`spotLightCount`、`cloudVolumeCount`。

## 渲染目标

| 名称 | 内容与契约 |
| --- | --- |
| `hdrColorTexture` / `hdrFbo` | 线性 HDR 场景颜色 |
| `bloomBrightTexture` | 线性 HDR 高亮提取结果 |
| `bloomBlurTextures` / `bloomBlurFbos` | Bloom 模糊使用的两个交替目标 |
| `cloudRaymarchTexture` / `cloudRaymarchFbo` | 半分辨率云步进结果，线性 HDR、预乘 Alpha |
| `overlayColorTexture` / `overlayFbo` | 全分辨率云、骨骼和 Gizmo 叠加结果，线性 HDR、预乘 Alpha；原名 `afterEffect*` |
| `sceneColorTexture` / `sceneFbo` | 已完成显示编码的 Viewport 最终颜色 |
| `directionalShadowTexture` / `directionalShadowFbo` | 方向光阴影深度 |
| `pointShadowTexture` / `pointShadowFbo` | 点光源阴影深度立方体数组 |

历史性能报告保留采样时的名称、文件路径和哈希。后续采样使用当前 Pass 名称，比较旧报告时按以上映射对应。
