# 主流程与功能导航

本文帮助第一次接触项目的读者建立运行流程，再按功能定位实现和验证。
目录分类见[文件组织](project-structure.md)，后续新增和整理代码遵循[代码阅读规则](readability.md)。

自有 C++ 类型与函数统一属于 `togr`；本文通常省略此前缀，`Engine::init()` 即 `togr::Engine::init()`。测试入口属于 `togr::tests`，`main()` 和第三方声明保持其原作用域；具体写法见[命名空间约定](naming.md#c-命名空间)。

## 第一次阅读路线

建议先沿下表读一遍入口和接口，再进入具体算法，不必从头通读所有文件。

| 顺序 | 入口 | 先回答的问题 |
| --- | --- | --- |
| 1 | [README](../readme.md) | 怎样构建、启动和操作编辑器？ |
| 2 | [main.cpp](../src/app/main.cpp) 的 `main()` | 应用如何启动，异常在哪里显示？ |
| 3 | [engine.cpp](../src/app/engine.cpp) 的 `init()`、`run()`、`shutdown()` | 谁组合各系统，调用顺序是什么？ |
| 4 | [scene.hpp](../src/scene/scene.hpp)、[gameObject.hpp](../src/scene/gameObject.hpp)、[component.hpp](../src/scene/component.hpp)、[selectionModel.hpp](../src/scene/selectionModel.hpp) | 场景、对象、组件和当前选择分别保存什么？ |
| 5 | [resourceManager.cpp](../src/assets/resourceManager.cpp)、[modelLoader.cpp](../src/assets/modelLoader.cpp) | 资源怎样加载，什么时候成为可绘制资源？ |
| 6 | [renderSystem.cpp](../src/rendering/renderSystem.cpp) 的 `init()`、`update()`、`render()`，再看 `prepareFrame()`、`executePasses()` | 谁准备资源与帧数据，谁执行 Pass，计时边界在哪里？ |
| 7 | [renderPass.hpp](../src/rendering/passes/renderPass.hpp)、[renderPasses.cpp](../src/rendering/passes/renderPasses.cpp) | 每一步读取什么、写入哪里？ |
| 8 | [guiSystem.cpp](../src/editor/guiSystem.cpp)、[componentEditors.cpp](../src/editor/componentEditors.cpp) | 谁协调面板，谁编辑组件，何时提交与生效？ |

之后按本文的功能导航进入模型、动画或云模块。
名字与渲染目标含义见[命名说明](naming.md)，包含边界见[头文件依赖](header-dependencies.md)。

## 启动：先有上下文，再创建渲染资源

[Engine::init()](../src/app/engine.cpp) 按以下顺序执行：

1. 初始化 `Input`，由 `WindowSystem::init()` 创建窗口并设为当前 OpenGL 上下文。
2. `RenderSystem::init()` 加载 GL 接口、创建渲染目标、缓冲和 Pass；返回后由 `Engine` 注入资源管理器及选择模型。
3. `ResourceManager::init()` 根据 [ShaderCatalog](../src/assets/shaderCatalog.hpp) 注册内置 Shader。
4. 初始化 `GuiSystem`，注入渲染器、资源管理器、选择模型和相机。
5. 用 [SceneFactory::createDefaultScene()](../src/scene/sceneFactory.cpp) 创建默认场景，并设为当前场景。

默认场景不由 GUI 创建；之后的 Add 菜单和模型菜单才是交互式对象创建入口。
`RenderSystem::init()` 中途失败时先自行清理其分配；`Engine` 按清理标记关闭已进入生命周期的系统，异常最终传给 `main()` 显示。
模型以 `Model::isReady()` 判断可绘制状态；纹理和天空盒分别提供 `isValid()`、`isReady()` 及 `lastError()`。
FBO 由 `requireComplete()` 检查，不完整时抛异常。不能只凭对象非空判断加载成功。

## 单帧：准备、绘制、交互依次执行

[Engine::run()](../src/app/engine.cpp) 的顺序会影响输入和编辑何时生效：

| 顺序 | 调用 | 本步行为 |
| --- | --- | --- |
| 1 | `Input::update()` | 保存上一份按键状态，供本帧的按下边沿判断使用。 |
| 2 | `WindowSystem::update()` | GLFW 轮询事件，由回调写入输入状态，并处理退出键。 |
| 3 | `ResourceManager::update()` | 收取后台模型结果、初始化其 GL 资源；随后处理场景延迟删除。 |
| 4 | `RenderSystem::update(dt)` | 根据上次面板报告的尺寸调整渲染目标，更新正在播放的动画。 |
| 5 | `GuiSystem::beginFrame()` | 开始 ImGui 帧。此时尚未执行面板交互。 |
| 6 | `RenderSystem::render(camera)` | 准备相机和场景数据，按顺序执行渲染 Pass。 |
| 7 | `GuiSystem::render(dt)` | 绘制各面板和 Viewport，处理选择、编辑、导入与相机控制，再提交 GUI 绘制。 |
| 8 | `WindowSystem::swapBuffers()` | 显示这一帧结果。 |

因此，Viewport 的飞行相机、尺寸及组件编辑通常影响下一次场景绘制。
删除先入队，在下一次资源更新时执行；导入请求也在后续更新时启动。
调试“一帧延迟”时先核对此顺序，避免把面板交互误认为发生在场景绘制之前。

## 退出：GL 对象必须先于上下文释放

[Engine::shutdown()](../src/app/engine.cpp) 先恢复本窗口为当前上下文，再依次：

1. 关闭 GUI，清空当前选择。
2. 关闭渲染器：等待云噪声任务，释放查询、Pass、渲染目标、缓冲及共享几何资源。
3. 关闭资源管理器：清空场景，等待模型导入任务并清空缓存，最后释放 Shader。
4. 关闭窗口和上下文。

正常循环结束和析构都可以进入清理；标记用于避免重复清理。
这些标记统一以 `CleanupRequired_` 结尾，表示清理责任。其中 `resourcesCleanupRequired_` 在资源初始化前设置，以便 Shader 注册中途失败时也能清理。
阅读资源析构时，要把它放回这条生命周期中判断，不能只看局部 RAII 包装。

## 线程和主要所有权

| 对象或任务 | 拥有者与使用方式 |
| --- | --- |
| 窗口、输入、相机、资源管理器、选择模型、渲染器、GUI | `Engine` 按值拥有；系统之间注入的指针不转移所有权。 |
| 当前场景与对象 | `ResourceManager` 持有场景；`Scene` 持有对象，`GameObject` 持有组件。 |
| 模型、网格、天空盒及 Shader | 缓存和使用者通过 `shared_ptr` 共享；资源不会反向拥有场景对象。 |
| 导入层级与动画关键帧 | `Model` 持有只读 `Node` 和 `Animation`；`AnimatorComponent` 自己保持模型的共享所有权，Animator 借用其中的数据。 |
| 实例姿态与骨骼调试显示 | 每个 `Animator` 按值持有 `SkeletonPose` 和 palette 纹理；`SkeletonViewerComponent` 只拥有可见性及调试 GL 缓冲，在绘制时借用节点和姿态。 |
| 当前选择 | `SelectionModel` 使用 `weak_ptr`；GUI 和渲染器读取同一份选择。 |
| 渲染目标、缓冲、云历史与查询 | `RenderSystem` 拥有；`RenderContext` 提供本帧视图和非拥有资源指针，也持有本帧对象/Shader 的共享引用。 |
| 模型导入任务 | `ModelLoader` 分别拥有等待队列和活动导入；活动导入将原始缓存键与 future 放在一起，主线程收取结果并上传 GL 资源。 |
| 云噪声任务 | `RenderSystem` 的 `CloudNoiseJobs` 拥有三个 CPU future；后台计算/访问磁盘缓存，渲染器在主线程上传纹理。 |
| 日志 | [Logger](../src/core/logger.hpp) 保护共享缓冲，后台可写入，GUI 取快照显示。 |

窗口事件、GUI、动画更新、渲染和 GL 资源操作在主线程执行。
动画不依赖骨架显示组件的存在或可见性；具体数据边界见[动画姿态与骨架显示](#动画姿态与骨架显示)。

## 资源加载、缓存与异步任务

先读 [ResourceManager](../src/assets/resourceManager.cpp) 的三个生命周期入口：`init()` 按 ShaderCatalog 顺序注册程序，
`update()` 先推进模型加载、再处理场景删除，`shutdown()` 先清场景、再等待并清理模型、最后清 Shader。
它协调加载器和场景；模型导入、Shader 编译及云噪声任务分别留在所属模块。

### 模型：请求、导入与发布

[ModelLoader::update()](../src/assets/modelLoader.cpp) 只协调 `collectCompletedImport()` → `startNextImport()`。
单次更新最多收取一个结果，再处理一个等待队首：启动导入，或跳过已经缓存的重复请求。

| 阶段 | 数据与责任 | 下一阶段的条件 |
| --- | --- | --- |
| `requestLoad()` | 复制 UTF-8 路径到 `pendingPaths_`；不启动线程、不操作 GL | 后续 update 有空闲导入槽位 |
| `startNextImport()` | 将原始路径键与有效 future 一起存入 `ActiveImport`，成功启动后才移除队首 | worker 完成 CPU 导入 |
| `collectCompletedImport()` | 用零等待检查就绪；取走活动任务后收取结果 | 模型存在且 `initGLResources()` 成功 |
| `publishImportedModel()` | 在当前 GL 线程上传并写入 `modelCache_` | 之后的资源快照可见，GUI 可创建场景对象 |

`Model::cpuImportSucceeded` 只表示 Assimp 数据导入成功；`glInitialized` 表示 GL 初始化成功，
`isReady()` 要求两者同时成立。worker 通过 [Model::importAsync()](../src/assets/model.cpp) 持有文件路径副本，
不访问加载器或场景。活动任务存在时不能再次启动同一路径并覆盖其结果，结果归属也不再依赖队首一直不变。

缓存键按请求的原始字符串比较，不进行路径规范化；同一文件的 `a.ply` 与 `./a.ply` 可以形成两个资源。
等待中和活动中的重复请求允许保留，轮到已缓存的请求时逐个跳过。普通导入或上传失败不进入缓存，后续请求仍可重试。
路径转换或任务启动抛异常时保留等待队首；收取或发布抛异常时活动请求已消费，异常向上传播，需要调用方显式重新请求。

`shutdown()` 等待活动任务但不收取或发布，随后丢弃结果、等待请求和缓存引用；可以重复调用。
`getAllLoadedModels()` 返回共享所有者的快照，清缓存不会撤销调用方持有的模型。
最后一份已上传资源的共享引用仍须在有效 GL 上下文中释放。

### 云噪声：CPU 任务与纹理就绪分开

[RenderSystem::ensureCloudNoise()](../src/rendering/renderSystem.cpp) 在云 Pass 内协调
`startCloudNoiseJobs()`、`cloudNoiseJobsReady()` 和 `publishCloudNoiseTextures()`。
第一次需要云时启动 Worley、Perlin、Weather 三项任务并返回；后续只有三项全部就绪才依次收取并上传，最后设置 `noiseReady`。
没有云体积时不轮询，已启动的 worker 继续执行。任务已经启动不等于纹理已经可用于绘制。

`CloudNoiseJobs` 只持有 CPU future；GL 纹理及 `noiseReady` 仍由渲染器拥有。
上传保持 Worley → Perlin → Weather 的次序，每项先 get 再上传；异常会向上传播，部分启动或上传不构成可重试的就绪状态。
退出时 `discardCloudNoiseJobs()` 等待并丢弃有效任务，不在清理中重新抛出 worker 错误，然后再释放纹理。

[NoiseTextureGenerator3D](../src/rendering/clouds/noiseTextureGenerator.cpp) 的各计算入口依次表达：
确定尺寸/参数与缓存键 → 尝试读缓存 → 调用对应的 CPU 生成函数 → 尝试保存 → 返回 `NoiseVolumeData`。
这些入口不调用 GL，但会访问文件系统并创建内部计算线程；`uploadVolume3D/2D()` 单独要求当前 GL 上下文。

### 各类缓存的含义

| 缓存或目录 | 标识与持有方式 | 失败、替换与清理 |
| --- | --- | --- |
| 模型缓存 | 原始 UTF-8 路径键，强引用模型 | 只缓存已上传模型；shutdown 释放自身引用 |
| Shader 实例缓存 | 注册名称键，强引用程序 | 已有名称直接复用，不重新读取路径；候选编译失败抛异常，不写入缓存 |
| `ShaderCatalog` | 内置名称、阶段路径和注册顺序的声明表 | 不是运行时资源缓存；程序的配置重编译仍由 Shader 负责 |
| 噪声磁盘缓存 | `data/cache/*.noise`，按算法、尺寸和参数标识 CPU 字节 | 头部与精确数据长度不匹配则重新生成；保存失败不影响返回生成结果 |

噪声缓存保持现有版本、32 字节头部和数据格式；它没有正文校验和，直接尽力写入，未提供原子替换或同键并发写入保证。
它与模型/Shader 的共享实例缓存有不同的有效性和所有权规则，不能把磁盘命中当作 GPU 资源就绪。

模型排队、精确路径键、失败重试和共享寿命见 [resourceLoadingChecks.cpp](../tests/resourceLoadingChecks.cpp)；
三种噪声的冷/暖缓存与损坏恢复见 [cloudNoiseChecks.cpp](../tests/cloudNoiseChecks.cpp)。
worker 退出清理另由 [renderSafety.cpp](../tests/renderSafety.cpp) 检查。

## 模型导入与加入场景是两个动作

1. **导入资源**：Assets 面板的 `Open...` 调用 `queueModelLoad()`，进入 `ModelLoader` 队列。
   `ModelLoader::requestLoad()` 接收请求，`Model::importAsync()` 持有路径副本并后台解析；`ModelLoader::update()` 收取结果，
   调用 `Model::initGLResources()`，成功后放入模型缓存，供 Models 列表显示。
2. **创建对象**：在模型条目右键选择 `Add to Scene`，调用 `GameObjectFactory::createFromModel()`，
   创建 Transform、模型绘制及骨骼显示组件；有动画时直接用模型创建 Animator、选择首个 clip 并执行 `update(0)`，随后交给 `Scene::addObject()`。

导入成功不会自动添加场景对象。同一模型资源可被多个场景对象共享。
模型空间、mesh 节点烘焙和骨骼 offset 的规则见[模型与骨骼坐标空间](project-structure.md#模型与骨骼坐标空间)。

阅读 [model.cpp](../src/assets/model.cpp) 时，从以下三个协调函数逐层展开：

| 入口 | 主流程 | 需要继续查看的实现 |
| --- | --- | --- |
| `importFile()` | 解析 Assimp 文件，导入节点和网格，最后创建动画；失败清空导入数据，成功才设置 CPU 导入完成状态 | `importNode()`、`importAnimations()` |
| `importNode()` | 登记或补全当前节点，递归孩子，随后导入当前节点的网格 | `importNodeHierarchy()`、`findOrCreateNode()`、`meshToModelBindTransform()` |
| `importMesh()` | 建立绑定空间变换，导入顶点、骨骼权重、索引和材质 | `makeMeshBindTransforms()`、`importVertices()`、`importSkinBindings()`、`importIndices()`、`importMaterial()` |

`MeshBindTransforms` 将顶点的 float 变换与逆矩阵、法线/方向的 double 计算放在同一个局部值对象中。
节点 ID 按首次出现分配，可能来自骨骼占位；层级补全保留它的 ID 与绑定矩阵。
因此应保持“登记本节点 → 孩子 → 本节点网格”的遍历顺序，不能在阅读或整理时假设所有层级节点先于网格建立。
两种骨骼出现顺序的节点身份、父子连接、绑定姿态和动画输出由 [modelAnimationChecks.cpp](../tests/modelAnimationChecks.cpp) 验证。

## 动画姿态与骨架显示

先从 [GameObjectFactory::createFromModel()](../src/scene/gameObjectFactory.cpp) 看资源绑定，再沿
[RenderSystem::update()](../src/rendering/renderSystem.cpp) → [Animator::updateAnimation()](../src/animation/animation.cpp)
看姿态更新；骨架的读取入口是 [RenderHelpers::drawSkeleton()](../src/rendering/renderUtils.cpp)。

| 数据或状态 | 拥有者 | 读取与修改边界 |
| --- | --- | --- |
| 节点层级、绑定位置和矩阵 | `Model` 的 `Node` 数组 | 导入阶段写入，发布后只有 const 查询；动画不修改绑定数据。 |
| clip 元数据与关键帧轨道 | `Model` 的 `Animation` 数组及 `Bone` | `Bone::sample(ticks) const` 返回局部 TRS，没有共享采样缓存。 |
| 当前 clip、时钟和实例姿态 | `AnimatorComponent` 内的 `Animator` | update 累计时钟，按层级采样，写入自己的 `SkeletonPose` 并上传自己的 palette。 |
| 可见性、点和线的 GL 缓冲 | `SkeletonViewerComponent` | 仅在 draw 时借用节点与可选姿态，不持有节点副本、不推进动画。 |

[SkeletonPose](../src/animation/skeletonPose.hpp) 的 `nodePositions` 按整个节点数组的下标存储模型空间位置，包含非骨骼节点；
`boneMatrices` 按 `Node::id` 保存固定容量的蒙皮矩阵。对象的世界变换仍由渲染入口另行应用。
Animator 初态使用绑定位置与 identity palette；工厂的 `update(0)` 将首个 clip 的首帧采样出来。

`playing` 由 `RenderSystem` 检查；暂停时不调用 update，姿态保持上一帧。
选择 clip 只重置时钟，下一次 update 才改变姿态；未知名称保持当前选择。
多个对象可以共享模型与 clip，但各自的时钟、姿态和 GPU palette 相互独立。

骨架显示有 Animator 时读取其 pose，没有 Animator 时使用 `Node::bindPosition`；传入 pose 长度不匹配时整体使用绑定位置。
显示开关及 Viewer 的销毁不会改变动画。Viewer 的借用只持续一次 draw，缓冲的创建、绘制与销毁仍要求当前 GL 上下文。

`AnimatorComponent` 在 Animator 之前声明模型共享所有者，使 Animator 借用的 nodes 和 clip 在其整个生命期内有效，
无需依赖其他组件的销毁顺序。模型绘制组件独立替换资源后，渲染入口会跳过与 Animator 绑定模型不一致的模型和骨架；
创建另一模型的动画对象应重新通过工厂组装组件，并遵守组件入场景前完成组装的约定。

## 组件中的编辑和生效状态

- `ModelRenderComponent::setModel()` 和 `StaticMeshComponent::setMesh()` 同时更新资源与局部包围盒；调用方通过 `getModel()`、`getMesh()`、`getBounds()` 查询，不能分别覆盖资源和派生包围盒。
- `SkyboxComponent::draftFolderPath` 是 UTF-8 编辑草稿，`getSkybox()` 是当前生效资源。`setSkybox()` 成功后发布资源并同步草稿；失败保留原资源，已有草稿仍可用于修改和重试。
- `DynamicMaterialComponent::draft` 保存三种可编辑贴图路径。`applyMaterial()` 从草稿创建候选，失败保留原材质；渲染调用 `bindMaterial()`，查询生效配置使用 `getMaterial()`。
- 场景中的组件值可以编辑，组件类型集合应在 `Scene::addObject()` 前组装完成。场景视图不跟踪之后的 `addComponent()`；借用视图在对象加入、移除或场景清理时失效。

## GUI 面板与组件编辑

[GuiSystem::render()](../src/editor/guiSystem.cpp) 按固定顺序协调窗口和提交：

| 顺序 | 入口 | 职责 |
| --- | --- | --- |
| 1 | `showDockspace()` | 构建 DockHost；没有保存的布局节点时创建默认布局，已有布局不覆盖。 |
| 2 | `showScenePanel()` | Add 菜单和 Light/Object/Skybox 树；更新选择、添加对象或请求延迟删除。 |
| 3 | `showInspectorPanel()` | 获取当前选择和组件快照，决定可编辑组件，再调用组件编辑器。 |
| 4 | `showRenderSettingsPanel()` | FPS/VSync、Shader 配置和渲染计时三块内容。 |
| 5 | `showAssetsPanel()` | Models 页展示已加载资源和导入入口；Console 页展示日志快照。 |
| 6 | `showViewportPanel()` | 报告目标尺寸、展示场景纹理，再处理飞行相机输入。 |
| 7 | `processModelImportDialog()` | 在面板之后显示模态对话框；确认后只排入模型加载请求，随后关闭对话框。 |
| 8 | ImGui Render / backend draw | 将本帧面板命令提交给 OpenGL 后端；不负责 scene render。 |

文件对话框放在面板之后，Viewport 的 hover 判断则紧随 Image，避免把其它控件当成图像区域。
相机输入保持“点击进入飞行 → RMB 释放或已有 modal 时退出 → 切换光标锁 → 计算 GLFW 虚拟坐标增量和移动 → 滚轮缩放”的顺序。
GuiSystem 只在持有飞行捕获时负责恢复光标，shutdown 会释放这份捕获及窗口借用。
Dock 窗口、Tab、菜单、Popup、Combo 和 tooltip 的 Begin/End、ID 及控件位置关系保留在所属绘制步骤内。

### 界面状态和领域状态

GuiSystem 自己拥有 FPS 采样、Console 过滤/自动滚动/已读修订号、飞行光标状态和设置错误提示。
Console 的修订号只在实际绘制 Console 页时推进；这些局部显示状态不与别的 GuiSystem 实例共享。
场景和组件属于 ResourceManager/Scene/GameObject；GUI 只在一次绘制中取得共享快照。
当前选择仍由 SelectionModel 弱引用观察，组件编辑器不持有选择或场景。

Shader checkbox 每帧查询生效程序的 `settingEnabled()`；提交仍由 Shader 执行编译、保存和替换。
失败显示错误并保留旧程序；composite 成功替换后重新写入 exposure。
Settings 保留 HDR → Exposure → Bloom 的先后顺序。VSync 切换直接作用于后续 swap 等待，
其它组件、相机、尺寸及 Shader 编辑通常由下一次 scene render 使用。

### Inspector 的分派与提交

[ComponentEditors::draw(Component&)](../src/editor/componentEditors.hpp) 是组件编辑的唯一入口，
只在当前调用借用组件。实现中的只读分派表按精确 C++ 类型选择具名编辑函数，
不存组件指针、不保留 shared_ptr，也不依赖 GUI 初始化时的注册过程。
入口统一按组件身份建立 ImGui ID scope，防止不同组件的 `OK`、`Enabled`、`Color` 等同名控件互相影响。
未提供编辑器的类型显示提示；模型和静态网格这两类存储组件保留空编辑器。

| 编辑对象 | 修改方式与提交责任 |
| --- | --- |
| Transform、灯光、Cloud 字段 | 控件直接编辑组件参数；Cloud 的 preset、联动 clamp 和参数范围留在具名编辑函数中。 |
| 天空盒路径 | 输入只修改 `draftFolderPath`；OK 才调用 `setSkybox()`，由组件创建候选并发布。 |
| 材质路径 | 输入只修改 `draft`；OK 才调用 `applyMaterial()`。资源失败时，编辑器显示组件的 lastError，不代替组件做回退。 |
| Animator | 选择 clip 调用 `playAnimation()`，按钮只修改 playing；姿态采样仍由动画更新负责。 |
| SkeletonViewer | 只修改 show，不持有或更新动画姿态。 |

SkyLayer 隐藏 Transform 是对象级 Inspector 规则，留在 GuiSystem；单个组件编辑器不反查所属对象。
Cloud 编辑器再按位置/模式、形状与覆盖、噪声、光照、动画与质量划分，主函数只展示这些阶段。
选择在 Scene 面板后立即供本帧 Inspector 使用；删除请求在下一次 ResourceManager 更新时执行，
删除选中对象时会先清选择。Models 的 Add to Scene 直接组装对象；导入对话框仅排队，加载和 GL 发布仍由加载器负责。

真实控件的 ID 隔离、草稿与提交、失败资源保留和播放按钮由 [guiChecks.cpp](../tests/guiChecks.cpp) 在隐藏 GL 上下文中驱动 ImGui 帧验证；
Dock 拖动、实际窗口光标锁与平台对话框的完整交互仍需人工检查。

## 一帧中的 Pass

注册顺序由 [RenderSystem::createPasses()](../src/rendering/renderSystem.cpp) 决定，执行实现集中在
[renderPasses.cpp](../src/rendering/passes/renderPasses.cpp)；云历史处理另见 [cloudTemporal.cpp](../src/rendering/clouds/cloudTemporal.cpp)。

### 主流程与计时边界

先阅读三个协调入口，再展开资源分配或具体绘制：

| 入口 | 协调的职责 | 不属于本步的工作 |
| --- | --- | --- |
| `init()` | GL 配置 → 共享缓冲 → 阴影目标 → Viewport 目标 → 注册 Pass；失败调用 shutdown | 不读取场景、不加载 Shader、不启动云噪声任务。 |
| `update(dt)` | 应用请求的目标尺寸，检查完整性；更新正在播放的动画 | 不执行场景绘制、不采集 Pass 计时。 |
| `render(camera)` | 选择可用计时槽 → 准备帧 → 按序执行 Pass → 提交本帧计时及元数据 | 不调整渲染目标、不处理 GUI 或 swap。 |

`prepareFrame()` 创建 [RenderContext](../src/rendering/passes/renderPass.hpp)：获取本帧 Shader、场景视图和被截断到容量内的光源列表，
连接渲染器拥有的目标，再上传相机 UBO 并计算视锥。全尺寸和云的半尺寸均来自已经应用的 Viewport 尺寸；
`setViewportSize()` 只记录请求，由下一次 update 应用。

Context 持有对象和 Shader 的共享引用，缓冲、纹理、相机与云服务则为非拥有借用，只在当前帧内使用。
它没有保存 OpenGL 状态；Context 中有某张纹理的指针，不代表该纹理已经绑定。
场景对象仍可编辑，复制视图不构成组件数据的深拷贝。

计时查询结果的收集与空槽选择发生在整帧计时之前。
Prepare 区间包含 Context 构建、相机上传和视锥计算，随后九个 Pass 分别计时；元数据来自同一份 Context。
云噪声的启动、轮询和完成后上传仍发生在 `CloudSkeletonOverlay` 内，归入该 Pass；后台任务只计算 CPU 数据。
update 中的 resize/动画、GUI、swap/VSync 等待均不在 scene render 计时中。
具体查询环、未完成帧跳过及匹配元数据检查见 [checkRenderProfiling()](../tests/renderPerformance.cpp)。

### Pass 的输入、产出和顺序

| 顺序 | Pass | 读取与写入 | 顺序或分支约定 |
| --- | --- | --- | --- |
| 1 | `FrameSetup` | 建立 GL 基线，清默认目标，绑定完整的骨骼与环境占位纹理 | 是本帧绘制的状态起点。 |
| 2 | `DirectionalShadow` | 场景几何 → 方向光深度 | 不按主相机视锥剔除投影物体；禁用阴影的光源清深度。 |
| 3 | `PointShadow` | 场景几何 → 点光六面深度数组 | 同样保留相机外投影物体；逐面清理后绑定分层目标绘制。 |
| 4 | `LightUpload` | 光源组件 → 三个 SSBO、forward 数量与阴影 uniform | 只准备数据，uniform 属于 forward 程序对象。 |
| 5 | `ForwardHdr` | 相机、几何、灯光与阴影 → HDR 颜色、高亮及深度 | 自行绑定光源 SSBO 和阴影纹理；LightUpload 必须先执行，但不再依靠它留下的纹理绑定。 |
| 6 | `CloudSkeletonOverlay` | HDR 深度、云输入与历史 → 透明 overlay，再追加骨架 | 总是清 overlay；无云时仍须清除上一帧结果。 |
| 7 | `Gizmo` | 当前选择与已有 overlay → 追加轴向标记 | 不清目标；无选择、无 Transform 或天空云层时跳过。 |
| 8 | `Bloom` | 高亮图 → 十次交替模糊，最终结果在 `bloomBlurTextures[1]` | 开启时自行设置全尺寸 viewport；关闭时不绘制且不修改 GL 状态。 |
| 9 | `Composite` | HDR、blur[1]、overlay → Viewport 颜色纹理 | 按配置做色调映射和显示编码，输出供 GUI 采样。 |

LightUpload 与 Forward 之间传递的是缓冲内容和程序 uniform，前者不再替后者绑定阴影采样器。
阴影必须先于 Forward；Forward 的深度供云终止和上采样使用；云/骨架、Gizmo 依次累积在同一 overlay；
Composite 最后消费这些结果。显式绑定消除了部分状态依赖，资源的生产与消费顺序仍然存在。

### GL 基线和绑定责任

FrameSetup 绑定默认 FBO 和本帧全尺寸 viewport，开启 depth test、`LEQUAL`、深度写入及可编程点大小，
关闭 blend、cull、scissor、framebuffer sRGB，并恢复全部颜色通道写入。
Pass 以这份基线为前提，只管理自己需要改变的状态。它们不保存、恢复任意调用者的完整 GL 状态。

| 状态或资源 | 设置责任与退出约定 |
| --- | --- |
| FBO / viewport | 绘制 Pass 绑定自己的目标；阴影用 1024²，云步进/历史用半尺寸，overlay/Bloom/Composite 用全尺寸。完成目标工作后解绑到 FBO 0。空光源的阴影 Pass 仍设置 1024² viewport，不改变 FBO；跳过的 Gizmo/Bloom 则不改状态。 |
| Depth / blend / cull | 阴影临时开启 front-face cull，绘制后关闭；云步进/历史 resolve、Gizmo、Composite 临时关闭 depth，结束后设回开启。Gizmo 关闭 blend/cull；云 resolve 关闭 blend。cull 模式值可能仍为 FRONT，但 cull 开关已关闭。 |
| Program | 每阶段选择自己的程序；清目标前先解绑程序或先建立有效采样绑定，避免 clear 时验证上一阶段采样器。活动 program 不作为后继 Pass 的输入。 |
| 相机 UBO 0 | Prepare 上传时重新绑定索引 0，Pass 共享本帧相机矩阵。 |
| 光源 SSBO 1 / 2 / 3 | LightUpload 更新数据；Forward 在消费前重新绑定 point / directional / spot。 |
| 云 SSBO 4 | 云 Pass 收集有效 volume 后上传并绑定；仅在云绘制分支使用。 |
| 纹理 5 / 6 / 7 / 8 | 5 为环境、6/7 为方向光与点光阴影、8 为骨骼。FrameSetup 提供 5/8 占位，天空盒和 Animator 在绘制时替换；Forward 自行绑定 6/7。 |
| 全屏阶段纹理 0…3 | 云步进、历史、上采样、Bloom 和 Composite 各自绑定本次采样输入；单元号可复用，不能借用前一阶段的偶然绑定。 |

云 Pass 主入口按“噪声就绪 → 步进与历史 → 清透明 overlay / 上采样 → 骨架”协调。
有效 volume 的筛选/打包、raymarch 和 overlay 绘制是局部实现步骤，未增加计时 Pass。
`CloudTemporal` 自己拥有历史资源，raw cloud 和 overlay 仍由 RenderSystem 持有；历史失效与重投影规则没有改变。

阅读或调整 Pass 时应同时核对头文件契约、生产/消费目标和对应验证，不能只看单个 execute 的函数体。

## 按功能定位实现和验证

| 想理解的功能 | 实现入口 | 验证入口 |
| --- | --- | --- |
| 场景、组件与选择 | [scene.cpp](../src/scene/scene.cpp)、[gameObject.hpp](../src/scene/gameObject.hpp)、[selectionModel.hpp](../src/scene/selectionModel.hpp)、[guiSystem.cpp](../src/editor/guiSystem.cpp) 的 `drawSceneTree()` | [renderSafety.cpp](../tests/renderSafety.cpp) 的场景容量与生命周期检查；[renderCorrectness.cpp](../tests/renderCorrectness.cpp) 的场景绘制检查；选择交互没有独立回归 |
| 帧准备、Pass 与 GL 状态 | [一帧中的 Pass](#一帧中的-pass)、[renderSystem.cpp](../src/rendering/renderSystem.cpp)、[renderPasses.hpp](../src/rendering/passes/renderPasses.hpp) | [renderCorrectness.cpp](../tests/renderCorrectness.cpp) 的 `checkFrameSetupBaseline()`、`checkForwardBindings()`、`checkComposite()`、`checkClouds()`；[renderPerformance.cpp](../tests/renderPerformance.cpp) 的 Bloom/相机绑定/计时检查 |
| GUI 面板与 Inspector | [GUI 面板与组件编辑](#gui-面板与组件编辑)、[guiSystem.cpp](../src/editor/guiSystem.cpp) 的 `render()`、`drawInspector()`；[componentEditors.cpp](../src/editor/componentEditors.cpp) | [guiChecks.cpp](../tests/guiChecks.cpp) 的真实控件交互；停靠布局、完整面板和模态窗口交互需要人工检查 |
| Viewport 与相机 | [guiSystem.cpp](../src/editor/guiSystem.cpp) 的 `showViewportPanel()`、`updateViewportCamera()`；[camera.cpp](../src/rendering/camera.cpp) | 右键飞行、窗口光标锁定和缩放仍需要手动交互验证 |
| 模型和资源路径 | [加载、缓存与异步流程](#资源加载缓存与异步任务)、[modelLoader.cpp](../src/assets/modelLoader.cpp)、[model.cpp](../src/assets/model.cpp)、[assimpFileSystem.cpp](../src/assets/assimpFileSystem.cpp) | [resourceLoadingChecks.cpp](../tests/resourceLoadingChecks.cpp) 的队列/缓存/寿命；路径夹具见 [renderCorrectness.cpp](../tests/renderCorrectness.cpp) |
| 材质、纹理与天空盒 | [material.cpp](../src/rendering/material.cpp)、[texture.cpp](../src/rendering/opengl/texture.cpp)、[skybox.cpp](../src/rendering/skybox.cpp)、[component.cpp](../src/scene/component.cpp) | [renderCorrectness.cpp](../tests/renderCorrectness.cpp) 的 `checkResourceFailures()`、`checkTextureUploads()`、`checkFramebufferFailures()`；[启动检查](../tests/renderStartup.cpp) |
| 光照与阴影 | [renderPasses.cpp](../src/rendering/passes/renderPasses.cpp)、[renderUtils.cpp](../src/rendering/renderUtils.cpp)、[forward.frag](../data/shader/forward.frag) | [renderCorrectness.cpp](../tests/renderCorrectness.cpp) 的 `checkLighting()`、`checkOffscreenCasters()`；容量检查见 [renderSafety.cpp](../tests/renderSafety.cpp) |
| 骨骼动画与调试显示 | [姿态所有权与读取顺序](#动画姿态与骨架显示)、[animation.cpp](../src/animation/animation.cpp)、[bone.cpp](../src/animation/bone.cpp)、[component.cpp](../src/scene/component.cpp) | [modelAnimationChecks.cpp](../tests/modelAnimationChecks.cpp) 的导入/实际 GPU 蒙皮/实例隔离；[renderSafety.cpp](../tests/renderSafety.cpp) 的采样边界、Viewer 数据借用与资源寿命检查 |
| 体积云 | [体积云说明](clouds.md)、[cloudDensity.glsl](../data/shader/cloudDensity.glsl)、[cloudRaymarch.frag](../data/shader/cloudRaymarch.frag)、[cloudTemporal.cpp](../src/rendering/clouds/cloudTemporal.cpp) | [cloudNoiseChecks.cpp](../tests/cloudNoiseChecks.cpp)、[cloudSamplingChecks.cpp](../tests/cloudSamplingChecks.cpp)；[renderCorrectness.cpp](../tests/renderCorrectness.cpp) 的 `checkClouds()` |
| Bloom 与最终合成 | [renderPasses.cpp](../src/rendering/passes/renderPasses.cpp) 的 `BloomPass`、`CompositePass`；[composite.frag](../data/shader/composite.frag) | [renderCorrectness.cpp](../tests/renderCorrectness.cpp) 的 `checkComposite()`；[renderPerformance.cpp](../tests/renderPerformance.cpp) 的 `checkBloom()` |
| Shader 配置与重载 | [shader.cpp](../src/rendering/opengl/shader.cpp)、[shaderCatalog.hpp](../src/assets/shaderCatalog.hpp)、`GuiSystem::showRenderSettingsPanel()` | [renderStartup.cpp](../tests/renderStartup.cpp)；[renderPerformance.cpp](../tests/renderPerformance.cpp) 的缓存和重编译检查 |
| 日志与故障出口 | [logger.cpp](../src/core/logger.cpp)、[guiSystem.cpp](../src/editor/guiSystem.cpp) 的 `drawConsoleTab()`、[main.cpp](../src/app/main.cpp) 的异常捕获 | [启动故障](../tests/renderStartup.cpp)和[资源失败检查](../tests/renderCorrectness.cpp)验证相关原因；没有独立的 Console 交互回归 |
| 渲染计时与基准 | [renderSystem.cpp](../src/rendering/renderSystem.cpp) 的 `render()`、`collectProfileResults()`；GUI Performance 区域 | [renderPerformance.cpp](../tests/renderPerformance.cpp) 的 `checkRenderProfiling()`；[renderBenchmark.cpp](../tests/renderBenchmark.cpp) 及[基准脚本](../tests/run-render-benchmark.ps1) |

表中的函数名可以直接在对应文件中搜索；它们是定位入口，不表示每项都有独立运行参数。
安全回归由 [renderSafety.cpp](../tests/renderSafety.cpp) 的 `main()` 统一串接，包含组件控件交互、渲染正确性、调用计数和启动检查。

```powershell
.\tests\run-render-safety.ps1 -Configuration Debug
.\tests\run-render-safety.ps1 -Configuration Release
```

脚本先构建应用，再链接独立测试入口；测试在应用工程中只是浏览项。
测试可执行文件位于 `temporary-build/x64/<配置>/`，生成夹具及诊断数据主要位于 `x64/RenderChecks/`。
部分故障检查会输出预期错误，最终以检查结果和进程退出码判断是否通过。
基准测量是另一条入口，运行参数、输出和计时边界见 [README](../readme.md) 与[体积云验证说明](clouds.md#验证)。
