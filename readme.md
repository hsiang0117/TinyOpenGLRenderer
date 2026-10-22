# TinyOpenGLRenderer

一个基于 **OpenGL 4.5 + C++17** 的小型实时渲染器 / 场景编辑器,用于学习与实验现代渲染技术。组件驱动的 ECS-lite 架构,RenderPass 流水线,Dear ImGui(docking)编辑器界面。

## 阅读代码

- 初次阅读从 [主流程与功能导航](docs/code-reading.md) 开始，按启动、单帧、退出理解程序，再按功能查找实现与测试入口。
- 新增和整理代码遵循 [代码阅读规则](docs/readability.md)，统一职责、接口契约、状态与单位的表达，以及函数组织和排版。
- 自有 C++ 代码使用 `togr` 根命名空间，例如 `togr::Engine`、`togr::Model`；作用域与宏前缀规则见 [命名约定](docs/naming.md#c-命名空间)。

## 效果展示

### 编辑器总览(docking 布局)

![Image](data/docked_layout.png)

### 渲染效果

![Image](data/image.png)

### 骨骼动画

![Image](data/animation.gif)

### 体积云

![Image](data/volumeCloud.gif)

## 功能特性

### 渲染

- **前向 HDR 管线**:RGBA16F 渲染目标,在线性空间完成光照、Bloom 与叠加合成,曝光色调映射后统一 Gamma 编码
- **多光源**:平行光 / 点光 / 聚光,光源数据经 SSBO 上传;当前上限分别为 1 / 10 / 50,编辑器和场景插入均检查容量,渲染提交另有上限保护
- **阴影**:平行光 2D shadow map;点光 cube map array 全向阴影;分别作用于对应光源,屏幕外物体仍参与投影
- **体积云**(Horizon 风格 raymarch):
  - 噪声分层:世界空间天气控制大尺度覆盖率,旋转域的 4 层 Perlin FBM 成形,高频 Worley 侵蚀边缘;基础纹理只存单层 Perlin,避免重复叠加 FBM
  - XZ 域扭曲使用独立噪声,不偏移 Y;天气、域扭曲、云体与细节在同一世界坐标中随风平移,削弱轴向平铺重复
    - 归一化双叶 HG 相位函数、4 阶多重散射近似、天空漫射光与自阴影;视线和光线共用完整密度场与消光系数,散射源包含 `sigma_s`
  - 采样:半分辨率渲染、确定为空的块跳过、可调视线/阴影步数;天空云层支持风速补偿的历史帧重投影,经过邻域限制与遮挡检查后进行深度感知上采样
    - **组件参数化**:云类型(层云↔积云)/覆盖率/噪声尺度/光照/风速等参数,Inspector 实时可调
    - `Apply preset` 提供积云、层积云、阴天三种外观,保留云层高度、范围、风速与采样质量;密度倍数同时控制透明度与阴影
  - 默认创建 **Sky Layer**:按世界空间云底高度与厚度定义横向连续云层,相机在 XZ 移动不会离开云的覆盖范围;最大追踪距离限制计算范围,远处平滑淡出到天空
  - Inspector 中调整 `LayerBottom`、`LayerThickness`、`MaxTraceDistance`、`DistanceFadeStart`;Sky Layer 不使用物体 Transform,天气与噪声采样固定在世界坐标中
  - `RegionScale` / `RegionCoverage` 控制大范围晴空与云区,`DensityVariation` / `HeightVariation` 控制区域疏密与云顶起伏;`WeatherScale`、`BaseNoiseScale`、`DetailScale` 控制区内云量、云体和细节尺度;参数含义与 GPU 检查见 [体积云说明](docs/clouds.md)
  - **Local Volume** 模式沿用世界轴对齐包围盒(平移/缩放)与边缘衰减;天空云层与局部云体统一沿射线积分,支持交叠与云间遮光,结果不依赖对象添加顺序
  - 噪声后台并行生成 + 自描述磁盘缓存(参数变化自动失效重建)
- **骨骼动画**:Assimp 导入,GPU 蒙皮,骨骼调试叠加显示
- **天空盒**、通过 Assimp 异步导入模型（含 OBJ、FBX、网格 PLY）

### 编辑器

- **Dear ImGui docking**:可停靠面板 —— 场景树 / Inspector / 渲染设置 / 资源 / Viewport(渲染到纹理)
- **组件式 Inspector**:Transform、光源、材质、体积云等组件均有对应编辑面板
- **选中 Gizmo**:选中物体时在其原点渲染 XYZ 轴向箭头(随物体旋转、始终最前、恒定屏幕尺寸)
- **应用内日志控制台**:GL 调试输出 / 引擎日志直接显示在编辑器内,无系统 cmd 窗口
- 数据驱动的 Add 菜单一键创建光源 / 网格 / 天空盒 / 体积云

### 架构

- **ECS-lite**:GameObject + 组件(`unordered_map<type_index, std::shared_ptr<Component>>`),场景视图按类型缓存
- **RenderPass 流水线**:FrameSetup → DirectionalShadow → PointShadow → LightUpload → ForwardHdr → CloudSkeletonOverlay → Gizmo → Bloom → Composite。Pass 通过 RenderContext 访问帧数据与资源，GL 绑定和状态也随执行顺序传递；具体职责与依赖见 [主流程与功能导航](docs/code-reading.md)。
- **代码命名**:Shader、渲染目标和组件按用途命名;约定及旧名映射见 [命名说明](docs/naming.md)
- **头文件依赖**:声明层使用前向声明,实现层包含完整定义;约定与边界见 [依赖说明](docs/header-dependencies.md)
- **文件组织**:源码目录与 VS 筛选器按功能模块对应,同一模块的头文件和实现放在一起;分类及包含路径约定见 [文件组织说明](docs/project-structure.md)
- **提交开销**:Bloom 关闭时跳过全部 10 次模糊绘制;静态网格不再设置无用骨骼 uniform;uniform 位置(含不存在的项)按 shader 缓存,重编译成功后重新查询
- **Shader 重编译**:保留几何阶段,编译失败继续使用原程序;参数直接更新目标程序,切换 HDR/Bloom 后保留曝光值
- **配置与错误处理**:缺少本地配置时读取仓库默认设置;面板显示已编译程序的实际开关状态;配置切换先编译、再保存和替换程序,失败则保留原状态并在 Console 提示
- **依赖注入**:Engine 为组合根,系统间以非拥有指针注入,无单例(Logger 除外)
- **GL 资源 RAII**:纹理 / FBO / Buffer 均为 move-only 包装,显式 shutdown 防 context 期外释放

## 构建

- **环境**:Windows + Visual Studio 2026(v145),x64,C++17
- 依赖已捆绑在 `external-dependencies/`，头文件、源码、库和运行库分目录保存；工程通过 `config/external-dependencies.props` 统一引用，无需额外安装。目录约定见 [第三方依赖说明](docs/external-dependencies.md):
  | 库 | 用途 |
  |---|---|
  | GLFW | 窗口 / 输入 |
  | glad | GL 4.5 加载 |
  | glm | 数学 |
  | Assimp | 模型 / 动画导入 |
  | Dear ImGui(docking)+ ImGuiFileDialog | 编辑器 UI |
  | stb_image | 贴图加载 |

打开 `TinyOpenGLRenderer.sln`,选 Debug/Release x64 直接生成运行。启动后自动加载带天空盒、地面与平行光的默认场景。

程序和回归测试输出到 `temporary-build/x64/Debug/` 或 `temporary-build/x64/Release/`，对应的 Assimp DLL 随构建自动复制。

资源路径以项目根目录为工作目录。`data/shader/settings.defaults.glsl` 随仓库提供默认配置:软阴影和 HDR 开启,Bloom 和环境反射关闭。忽略的本地 `data/shader/settings.glsl` 存在时优先使用;在面板成功切换后写入本地配置,下次启动恢复这些开关。配置项使用四个平铺的 `#define`/`//#define` 指令。首次启动无需手动创建该文件。

缺失 shader、编译失败、默认天空盒加载失败、FBO 不完整或无法创建 OpenGL 4.5 窗口时,启动会停止并显示错误原因。shader 的顶点、几何、片段阶段统一处理 include;支持文件相对、嵌套和绝对引用,既有 `data/`、`tests/` 引用仍相对工作目录;注释内的 include 不会加载,循环 include 会报告错误。

可选材质贴图加载失败会报告路径和原因,使用有效默认纹理,不会将 ID=0 加入材质。编辑器替换天空盒或材质失败时保留原资源,并在组件面板显示原因。FBO 在附件装配完成及调整尺寸后检查完整性;资源错误处理约定见 [文件组织说明](docs/project-structure.md)。

文件路径内部使用 `std::filesystem::path`,编辑器和公开字符接口使用 UTF-8。模型、材质贴图、天空盒和 Shader 通过原生路径访问中文目录;异步导入持有路径副本,编辑器路径输入使用可扩容字符串。具体解析规则见 [资源路径说明](docs/project-structure.md#资源路径)。

Debug 和 Release x64 共用头文件路径、依赖库和 C++17 设置。仓库当前只提供 Debug 版 Assimp(`assimp-vc143-mtd.lib` / `.dll`)，两种构建都从 `external-dependencies/runtime/x64/` 复制 DLL 到输出目录。程序本身使用 Release 优化，但 Assimp 仍依赖 Visual C++ 调试运行库；对外发布前需替换为匹配的 Release 版 Assimp 库和 DLL。

在项目根目录用 PowerShell 运行安全回归检查(需支持 OpenGL 4.5 的显卡):

```powershell
.\tests\run-render-safety.ps1 -Configuration Debug
.\tests\run-render-safety.ps1 -Configuration Release
```

脚本先构建对应配置,再复用编译产物运行隐藏窗口测试,覆盖关键帧边界、非法骨骼索引、光源容量、后台任务结束和 GL 资源释放。GPU 像素回读检查逐光源阴影、屏幕外投影物体、线性 HDR/Bloom、最终 Gamma、多云添加顺序与交叠、不透明深度遮挡;同时检查着色器链接与 GL 调试错误。

纹理回归覆盖 1–4 通道图片、奇数宽度的行对齐、立方体贴图六面、浮点/字节纹理清色和 3D 上传,并检查上传后恢复像素对齐状态。网格回归检查球体按细分参数共享缓存、非法参数拒绝和最后一个持有者释放资源。

性能回归统计实际 GL uniform 查询和模糊绘制次数,覆盖 Bloom 热切换、uniform 缓存与移动/重编译、失败重编译保留旧程序和静态网格提交。失败重编译用例会故意生成非法 shader,预期的编译错误后应继续输出 `PASS`。这些调用计数不等同于完整场景的 FPS 基准。

在 `Render Settings > Performance` 勾选 `Collect timings`,可查看准备阶段和全部 9 个 Pass 的 CPU/GPU 毫秒耗时、场景渲染总耗时、视口大小、不透明物体/灯光/云体数量。统计默认关闭;开启后用四帧查询队列异步读取 GPU 时间,队列满时跳过采样。面板显示最近完成采样的帧号、数据延迟和跳过数量,CPU 与 GPU 数值对应同一帧。总耗时仅覆盖 `RenderSystem::render`,不含动画更新、GUI 或 VSync 等待;CPU/GPU 重叠执行,两者不能相加。数量是该帧的提交候选,不等同于实际绘制次数。

比较性能时使用 Release,固定相机、视口尺寸与渲染开关,等待云噪声生成和着色器初始化完成,再分别记录无云、单云、多云和不同灯光数量的结果。计时本身有开销,关闭采样后再观察最终 FPS。计时回归检查未完成查询不读取、队列满不覆盖、同帧元数据、开关与缩放、像素结果不变及查询资源释放。

CPU 列测量场景准备与 Pass 执行的主线程时间,不包含采样结果回读;GPU 列是对应命令区间的时间戳差,可能包含等待 CPU 继续提交命令的空闲时间,不代表 GPU 利用率。

固定场景的 Release 基线可用以下命令复现:

```powershell
.\tests\run-render-benchmark.ps1
```

默认视口为 1280×720,相机固定,场景包含地面和 64 个立方体,比较 0/1/4/10 个点光源、1/4 个云体、云体与点光源组合、Bloom 开启,最后重测无云场景作为对照。每个场景至少预热 60 帧且一秒,随后测三轮,每轮收集 120 个完整计时样本。分 Pass 采样在每帧测量区间外等待 GPU 完成;独立吞吐测量关闭计时,每轮至少运行一秒,按 `SampleFrames` 分批提交并等待 GPU 完成,避免积压过多命令。吞吐仅代表离屏场景渲染,不包含 GUI/交换缓冲,不能当作应用 FPS。

运行使用 `x64/BenchmarkRuns` 下的隔离 shader 和噪声缓存,固定 PCF/HDR 开、环境映射关、云风速为零,不改动本地设置。目录中保存硬件/驱动记录、原始样本、各 Pass 中位数/P95、吞吐和场景 BMP 截图。脚本支持 `-Width`、`-Height`、`-WarmupFrames`、`-SampleFrames`、`-Rounds` 参数;加 `-CloudPresets` 可检查三种云预设、水平远行与不同太阳方向,使用独立的无云渐变背景。

本机实测基线与后续优化优先级见 [2026-10-01 Release 性能报告](reports/performance/2026-10-01-release-baseline/baseline.md)。

启动回归复制 shader 到隔离目录模拟首次检出,不修改项目的本地设置,检查默认配置、四项开关反复切换和重启恢复、保存/编译失败保留原状态、缺失 shader、循环 include 和失败初始化后的清理。故障用例会输出预期的错误日志。

## 操作

| 输入 | 行为 |
|---|---|
| 在 Viewport 内按住鼠标右键 | 进入飞行模式并锁定光标，松开退出 |
| 飞行时 `W / A / S / D` | 相机平移 |
| 飞行时 `Space / 左 Ctrl` | 相机升 / 降 |
| 飞行时移动鼠标 | 调整视角 |
| 鼠标悬停 Viewport 或飞行时滚轮 | 缩放(FOV) |
| `Esc` | 退出 |

场景树中左键选中物体(出现 XYZ gizmo),右键可删除;Inspector 中实时编辑组件参数。
