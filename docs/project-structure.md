# 项目文件组织

同一功能模块的头文件和实现文件放在同一组。源码目录与 VS 筛选器使用对应的分类，通常控制在两到三层。

## 源码目录与 VS 筛选器

自有源码已按下表迁移到模块目录，VS 筛选器使用对应分组。`TinyOpenGLRenderer.vcxproj` 记录项目文件及其构建类型，`TinyOpenGLRenderer.vcxproj.filters` 记录它们在解决方案资源管理器中的分组。

| 筛选器 | 内容 | 源码目录 |
| --- | --- | --- |
| `App` | `main`、`engine` | `src/app/` |
| `Platform` | `windowSystem`、`input` | `src/platform/` |
| `Scene` | `scene`、`gameObject`、`component`、`gameObjectFactory`、`sceneFactory` | `src/scene/` |
| `Assets` | `model`、`modelLoader`、`shaderLoader`、`shaderCatalog`、`resourceManager`、`assimpGlmHelpers`、`assimpFileSystem` | `src/assets/` |
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

`assimpGlmHelpers.hpp` 保存模型导入使用的 Assimp 与 GLM 类型转换。`assimpFileSystem` 实现 Assimp 的文件系统与文件流接口，将导入器传入的 UTF-8 文件名转换为原生路径，统一访问模型和配套材质文件。`noiseTextureGenerator` 负责云噪声计算、磁盘缓存和纹理上传；其头文件只依赖 CPU 数据结构，纹理使用前向声明，GLM 和 OpenGL 的完整定义在实现中引入。

`shaderCatalog.hpp` 集中声明内置 Shader 的注册名称、顶点/片段/可选几何阶段路径和注册顺序，以及共享全屏顶点阶段、Shader 目录与配置文件路径。`ResourceManager::init()` 遍历声明表注册，渲染器、GUI 和测试通过 `ShaderCatalog::forward.key` 等声明获取名称。`ShaderLoader` 保留按字符串注册和查询的接口，独立测试 Shader 仍可提供自定义路径。

新增内置 Shader 时，在声明表中定义一个 `Program` 并加入 `programs`，再更新使用它的渲染上下文。应用以项目根目录作为资源工作目录，测试和基准程序使用各自的隔离目录。

## 模型与骨骼坐标空间

导入后的顶点统一保存在模型空间，包含 Assimp 根节点和 mesh 节点链的绑定变换。设原顶点为 `p`，完整 mesh 变换为 `M`，则保存的顶点为 `q = M * p`。Assimp offset `O` 接收 mesh 局部坐标，因此导入时保存为 `O_model = O * inverse(M)`，动画矩阵使用 `G(t) * O_model`。作用于 `q` 后得到 `G(t) * O * p`，不会再次应用根或 mesh 变换。此约定下不再额外左乘根逆矩阵。

骨架节点位置和绑定姿态 AABB 使用同一模型空间；动画不会修改绑定 AABB。法线在导入时使用 mesh 变换的逆转置，切线和副切线使用线性部分，缺失或退化的零方向保持为零。该导入规则不改变运行时 Shader 的蒙皮法线算法。

多个 mesh 共享骨骼时，转换后的 offset 必须兼容现有共享矩阵表。导入器检查不兼容绑定、不可逆的蒙皮 mesh 变换和非有限矩阵，报告原因并放弃该模型，不用最后一个 mesh 的绑定静默覆盖前面的结果。求逆使用双精度，不以固定行列式阈值拒绝小单位缩放。

`tests/modelAnimationChecks.cpp` 构造并重新导入非单位根变换的 Assimp 模型，覆盖共享骨骼、静态对照、绑定和动画帧、AABB 与骨架位置，并通过 transform feedback 检查前向渲染和两种阴影顶点阶段的实际输出。

## 资源路径

文件操作和目录组合使用 `std::filesystem::path`。公开的字符路径、编辑器文本和日志使用 UTF-8，通过 `u8path()` 转入原生路径，通过 `u8string()` 转回文本。工程和测试脚本使用 `/utf-8` 编译字符集。模型名称使用 `stem()`，贴图相对路径使用模型的 `parent_path()` 拼接，绝对贴图路径保留其位置，空贴图路径保持为空。加载器的字符串缓存键保留调用方提供的标识，不借路径重构改变缓存语义。

模型的异步导入任务在启动前持有路径副本；Assimp 和图片加载器通过原生文件句柄读取文件。编辑器路径字段使用可扩容字符串，天空盒和材质路径不再经过固定 255 字节数组。默认字体从 Windows 系统目录定位，找不到时使用 ImGui 默认字体。

Shader 的绝对 include 直接使用；以 `data/` 或 `tests/` 开头的 include（允许 `./` 前缀）保留工作目录相对语义；其余 include 优先相对当前文件所在目录解析，找不到时再按工作目录查找。嵌套 include 递归保留各自的源文件目录，并检查循环和展开上限。配置文件按解析、规范化后的路径识别，等价路径仍使用默认配置回退和设置覆盖。设置临时文件保留 `.glsl.tmp` 命名及原子替换行为。

噪声缓存从同一个目录定义组合文件名，保存时创建目标文件的父目录。构建脚本读取 MSBuild 的路径属性，兼容相对和绝对 `IntDir`、`OutDir`。路径类型不会改变应用的资源工作目录约定。

启动必需的 Shader、默认天空盒或渲染目标初始化失败会传播异常，并在销毁 OpenGL 上下文前清理已创建的资源。图片加载提供 `isValid()` 和 `lastError()`；cubemap 要求六面都是相同尺寸、相同格式的正方形，错误中包含具体面文件路径。

材质空路径表示没有可选贴图。非空路径加载失败时，`Material::initGLResources()` 返回 `false` 并保留错误，只将成功贴图加入有效列表，缺少部分使用完整的占位纹理。Shader 采样 albedo、ambient、specular、normal 四类贴图；导入的 shininess 贴图仍参与现有的默认材质判定，清理时保留该兼容行为。编辑器替换天空盒或材质时先创建候选资源，全部成功后才替换当前资源；失败原因在组件面板显示。未就绪的天空盒不绘制也不绑定，没有可用天空盒的场景使用完整黑色 cubemap 供环境采样。

FBO 在附件和读写目标装配完成后，以及相关纹理尺寸更新后，调用 `FrameBuffer::requireComplete()`。检查不改变当前读写绑定；不完整时报告目标名称、原因和 OpenGL 状态码，阻止继续使用该渲染目标。

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
