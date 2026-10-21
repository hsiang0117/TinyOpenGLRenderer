# 项目文件组织

同一功能模块的头文件和实现文件放在同一组。源码目录与 VS 筛选器使用对应的分类，通常控制在两到三层。

## 源码目录与 VS 筛选器

自有源码已按下表迁移到模块目录，VS 筛选器使用对应分组。`TinyOpenGLRenderer.vcxproj` 记录项目文件及其构建类型，`TinyOpenGLRenderer.vcxproj.filters` 记录它们在解决方案资源管理器中的分组。

| 筛选器 | 内容 | 源码目录 |
| --- | --- | --- |
| `App` | `main`、`engine` | `src/app/` |
| `Platform` | `windowSystem`、`input` | `src/platform/` |
| `Scene` | `scene`、`gameObject`、`component`、`gameObjectFactory`、`sceneFactory` | `src/scene/` |
| `Assets` | `model`、`ply`、`modelLoader`、`shaderLoader`、`shaderCatalog`、`resourceManager`、`assimpGlmHelpers` | `src/assets/` |
| `Animation` | `animation`、`bone`、`assimpNode` | `src/animation/` |
| `Rendering` | `renderSystem`、`renderUtils`、`renderLimits`、`camera`、`material`、`skybox` | `src/rendering/` |
| `Rendering/Geometry` | `mesh`、`meshGenerator` | `src/rendering/geometry/` |
| `Rendering/Passes` | `renderPass`、`renderPasses` | `src/rendering/passes/` |
| `Rendering/OpenGL` | `shader`、`texture`、`glBuffer` | `src/rendering/opengl/` |
| `Rendering/Clouds` | `cloudVolume`、`cloudTemporal`、`noiseVolumeData`、`noiseTextureGenerator` | `src/rendering/clouds/` |
| `Editor` | `guiSystem` | `src/editor/` |
| `Core` | `logger` 等公共基础设施 | `src/core/` |

`Scene` 负责对象集合、延迟删除、光源容量和组件视图缓存。`ModelLoader` 负责后台模型导入与缓存，`ShaderLoader` 负责 Shader 注册与缓存。`ResourceManager` 通过现有接口协调加载器和当前场景；这四个类分别有独立文件，主要非模板实现放在 `.cpp` 中，模板和场景的简单访问器保留在头文件中。

加载器头文件前向声明 `Model` 和 `Shader`，完整定义只在实现中引入。场景工厂直接包含 `scene.hpp`，使用组件视图的调用方直接包含相应组件定义。

`assimpGlmHelpers.hpp` 保存模型导入使用的 Assimp 与 GLM 类型转换。`noiseTextureGenerator` 负责云噪声计算、磁盘缓存和纹理上传；其头文件只依赖 CPU 数据结构，纹理使用前向声明，GLM 和 OpenGL 的完整定义在实现中引入。

`shaderCatalog.hpp` 集中声明内置 Shader 的注册名称、顶点/片段/可选几何阶段路径和注册顺序，以及共享全屏顶点阶段、Shader 目录与配置文件路径。`ResourceManager::init()` 遍历声明表注册，渲染器、GUI 和测试通过 `ShaderCatalog::forward.key` 等声明获取名称。`ShaderLoader` 保留按字符串注册和查询的接口，独立测试 Shader 仍可提供自定义路径。

新增内置 Shader 时，在声明表中定义一个 `Program` 并加入 `programs`，再更新使用它的渲染上下文。所有路径仍相对于工作目录解析，应用以项目根目录运行，测试和基准程序使用各自的隔离目录。

Shader 单独显示在 `Shaders`，其磁盘路径仍为 `data/shader/`：

| 筛选器 | 文件用途 |
| --- | --- |
| `Common` | `fullscreen.vert`、Shader 设置及默认值 |
| `Forward` | `forward`、`skybox` |
| `Shadow` | `directionalShadow`、`pointShadow` |
| `Cloud` | `cloudRaymarch`、`cloudNoise.glsl` 噪声采样、`cloudDensity.glsl` 参数布局与密度塑形、`cloudTemporal` 历史帧重投影、`cloudUpsample` |
| `PostProcess` | `gaussianBlur`、`composite` |
| `Overlay` | `skeleton`、`lightMarker`、`gizmo` |

其余分组为：

- `Tests`：测试源码与运行脚本；测试 Shader 放在 `Tests/Shaders`。
- `Docs`：README 与开发说明。
- `Config`：编辑器、Git、应用配置和第三方依赖属性表。
- `ThirdParty`：按库分组的第三方源码、常用入口头文件、库文件和运行库；磁盘文件统一位于 `external-dependencies/`。

测试源码在应用工程中使用 `None` 类型，只供浏览；实际构建继续由 `tests/run-render-safety.ps1` 和 `tests/run-render-benchmark.ps1` 完成。将它们加入主程序的 `ClCompile` 列表会引入测试入口，应保持独立构建。

## 包含路径

工程和测试脚本都将 `src/` 作为自有源码的包含根。跨模块引用写明模块路径，同目录引用使用短文件名：

```cpp
#include "renderSystem.hpp"
#include "assets/resourceManager.hpp"
#include "rendering/passes/renderPass.hpp"
```

新增模块或移动文件时，同步更新引用、`.vcxproj` 的磁盘路径和 `.vcxproj.filters` 的分组。

第三方包含根为 `external-dependencies/include/`，既有的 `<glm/...>`、`<GLFW/...>`、`<assimp/...>` 和 `"imgui/..."` 写法保持一致。ImGui 源码在 `external-dependencies/src/imgui/`，属性表额外提供其头文件目录。依赖路径在 `config/external-dependencies.props` 中定义，测试脚本通过 MSBuild 读取相同属性。库和运行库按平台放在 `lib/x64/`、`runtime/x64/`，详情见 [第三方依赖说明](external-dependencies.md)。

应用和回归测试的可执行文件、DLL 输出到 `temporary-build/x64/<配置>/`，构建目录由工程的 `OutDir` 统一指定。基准采样数据继续记录在 `x64/BenchmarkRuns/`，历史性能报告保留采样时的路径和哈希。

## 后续整理

后续新增依赖时更新属性表、VS 项目项与依赖说明，保留第三方文件的原始内容和行尾。

新增源码时，在 `.vcxproj` 中设置正确的构建类型，并在 `.vcxproj.filters` 中分配到对应模块；头文件和实现使用同一筛选器。新增测试继续使用独立测试脚本。修改文件保持 Windows CRLF 行尾。
