# 头文件依赖约定

头文件应能独立包含，不依赖调用方先包含其他文件。实现文件先包含自己的头文件，再包含实现所需的定义。

跨模块引用以 `src/` 为包含根，例如 `#include "rendering/opengl/texture.hpp"`；同目录引用使用短文件名，例如 `#include "renderSystem.hpp"`。VS 工程与测试脚本都将 `src/` 加入包含目录，模块归属可以直接从引用路径判断。

第三方头文件位于 `external-dependencies/include/`，包含写法沿用库自身命名空间，如 `<glm/glm.hpp>`、`<glad/glad.h>`。ImGui 源码独立放在 `external-dependencies/src/imgui/`，其短文件名引用由属性表中的 `ExternalImGuiIncludeDir` 解析。依赖路径统一通过 `config/external-dependencies.props` 提供。

- 按值存储成员、继承基类或在非模板内联函数中访问成员时，需要包含完整定义。
- 只声明指针、引用或 `std::shared_ptr<T>` 时，优先前向声明；创建对象和访问成员的代码放在 `.cpp` 中，并在那里包含完整定义。
- `GameObject` 的组件模板在调用时需要具体组件的定义。使用 `addComponent<T>()`、`getComponent<T>()` 的文件应直接包含对应组件头文件。
- `RenderSystem` 的构造和析构定义放在 `.cpp` 中，保证释放 `std::unique_ptr<RenderPass>` 时能看到 `RenderPass` 的完整定义。
- 标准库类型直接包含对应标准头文件；不要自行前向声明标准库类型或 GLM 的类型别名。
- 包含保护避免重复定义，不负责解决类型之间的依赖环。

## 当前边界

- `gameObject.hpp` 只声明组件基类，组件实现不再由对象容器头文件传播。
- `guiSystem.hpp`、`windowSystem.hpp` 通过前向声明使用外部对象；GLFW、组件、输入及渲染器定义在实现文件中引入。
- `renderPass.hpp` 只声明其非拥有资源指针所需的类型；`renderSystem.hpp` 不包含具体 Pass、GUI 或场景组件。
- `noiseVolumeData.hpp` 保存 CPU 噪声数据结构。`noiseTextureGenerator.hpp` 只包含该数据头文件并前向声明纹理类型；CPU 计算调用方无需引入 GLM 或 OpenGL，调用纹理生成和上传接口时直接包含 `texture.hpp`。
- Assimp 与 GLM 类型转换在独立的 `assets/assimpGlmHelpers.hpp` 中，模型导入直接引用它，云噪声模块不再引入 Assimp。
- 模型和网格组件的设置函数移到实现文件，组件头文件不再包含模型、网格和天空盒的完整定义。
- `Scene` 在独立的 `scene.hpp` 中声明，组件定义、光源上限和日志只在 `scene.cpp` 中引入；使用 `getView<T>()` 的调用方直接包含 `T` 的定义。
- `ModelLoader`、`ShaderLoader` 的头文件只声明模型和 Shader 指针，具体加载实现放在各自的 `.cpp`。`resourceManager.hpp` 保留协调接口及 `getSceneView<T>()`，不再传播模型、Shader 或具体组件的完整定义。
- `shaderCatalog.hpp` 只包含标准库和常量声明，不依赖加载器、渲染器或 OpenGL；使用内置 Shader 名称和路径的实现文件直接包含它。

自有头文件依赖扫描没有发现 `#include` 环。`RenderContext::owner` 仍让云 Pass 回调渲染器，这是实现层的逻辑耦合，通过前向声明避免了头文件互相包含。

头文件依赖环与共享所有权环是不同问题。系统之间使用非拥有指针，选中对象使用 `weak_ptr`；组件共享模型、网格和天空盒资源，资源不反向持有场景对象。新增反向关系时应明确所有权，避免双方都用 `shared_ptr` 持有对方。
