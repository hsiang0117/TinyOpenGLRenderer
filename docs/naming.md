# 渲染代码命名

类使用 `PascalCase`，函数、变量和 Shader 注册名称使用 `lowerCamelCase`。名称优先说明用途；集合使用复数，数量使用 `Count`。渲染资源的缩写按单词书写，例如 `hdrFbo`、`cameraMatricesUbo`、`CloudVolumeGpu`。

阶段、状态、坐标空间与单位的命名，以及函数组织和接口契约，见 [代码阅读规则](readability.md)。当前功能的实现与测试入口见 [主流程与功能导航](code-reading.md)。

## C++ 命名空间

项目自有 C++ 声明统一放在根命名空间 `togr`（TinyOpenGLRenderer 的缩写）中，例如 `togr::Engine`、`togr::Model`、`togr::Camera`。
现有用途命名空间保留在根下：`togr::ShaderCatalog`、`togr::RenderHelpers`、`togr::RenderLimits`、`togr::ComponentEditors`。
目录分类不要求再建立同名子命名空间；当前规模优先让同一根内的类型直接协作。

- 头文件先在全局作用域包含依赖，再打开 `namespace togr { ... }`；自有类型的前向声明也放在其中。
- 第三方声明保持原作用域，如全局的 `GLFWwindow`、`aiNode`、`aiAnimation`，以及 `std`、`glm`、`Assimp` 中的类型。不能把第三方头文件包含进项目命名空间。
- 实现文件在依赖之后打开相同的根命名空间；仅供本文件使用的匿名命名空间嵌在其中。命名空间结束处注明 `// namespace togr`。
- 公共头文件不使用 `using namespace`，也不在全局提供旧类型的兼容别名。调用方在根外使用完整名称，例如 `togr::Engine`；根内可直接使用 `Model` 等名称。
- 自有测试使用 `togr::tests`；应用、安全检查和基准的 `main()` 保持全局作用域，通过显式限定调用项目入口。
- 宏不受命名空间约束。自有日志、相机默认值、骨骼容量和头文件保护宏统一使用 `TOGR_` 前缀；日志宏内部使用 `::togr::Logger` 等完整名称。第三方及平台配置宏沿用其要求的名称。

例如，应用入口显式使用项目中的类型：

```cpp
#include "app/engine.hpp"

int main() {
    togr::Engine engine;
    engine.init();
    engine.run();
}
```

阅读文档中的类型及接口名通常省略 `togr::` 前缀；源码中的真实声明仍遵循上述边界。

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
- 释放接口统一为 `shutdown()`，Shader 热重载为 `reload()`。组件编辑统一从 `ComponentEditors::draw(Component&)` 进入，具体控件使用具名编辑函数。
- 模型后台 CPU 导入为 `Model::importAsync()`，加载器排队入口为 `ModelLoader::requestLoad()`；GPU 初始化仍使用 `initGLResources()`。
- 组件中的待应用输入使用 `draft`：天空盒为 `draftFolderPath`，材质为 `draft`。材质提交使用 `applyMaterial()`；生效资源通过查询接口读取。
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
