# 第三方依赖

仓库捆绑的依赖统一放在 `external-dependencies/`，保留原始文件内容、行尾和已有的许可说明。头文件按库名保存，参与编译的源码、预编译库和运行库分别存放：

| 目录 | 内容 |
| --- | --- |
| `include/assimp/` | Assimp 头文件 |
| `include/GLFW/` | GLFW 头文件 |
| `include/glad/`、`include/KHR/` | GLAD 与 Khronos 头文件 |
| `include/glm/` | GLM 数学头文件 |
| `include/imgui/` | ImGui、ImGuiFileDialog 及其配套头文件 |
| `include/stb_image.h` | 图片加载头文件 |
| `include/dirent/` | 历史捆绑，当前未使用；文件枚举使用 `std::filesystem` |
| `src/imgui/` | ImGui、后端与 ImGuiFileDialog 实现；`imgui_demo.cpp` 保留在依赖目录，不参与应用构建 |
| `src/glad/` | GLAD 加载器实现 |
| `lib/x64/` | GLFW 静态库、Assimp 导入库 |
| `runtime/x64/` | Assimp DLL |

VS 的 `ThirdParty` 按库显示编译源码、常用入口头文件、库和 DLL。其余头文件随依赖完整保存在磁盘上，由包含路径解析。

## 工程与脚本

`config/external-dependencies.props` 定义 `ExternalIncludeDir`、`ExternalImGuiIncludeDir`、`ExternalLibraryDir` 和 `AssimpRuntimePath`。Debug、Release x64 使用相同的依赖路径，ImGui 实现的短文件名引用通过额外的 ImGui 包含目录解析。属性表还负责将 Assimp DLL 复制到当前 `TargetDir`。

两个测试脚本构建应用后，通过 MSBuild 读取 `IntDir`、`OutDir`、`ExternalIncludeDir`、`ExternalLibraryDir` 和当前配置的 `ClCompile` 项，再编译各自入口，只链接当前工程使用的对象文件。应用及测试可执行文件输出到 `temporary-build/x64/<配置>/`；基准采样结果继续写入 `x64/BenchmarkRuns/`。

第三方 DLL 是构建输入，保存在 `runtime/x64/`；生成目录中的 DLL 是构建时复制的输出。当前捆绑的 Assimp 库和 DLL 仍是 Debug 版本，目录整理没有更换依赖版本。

## 更新依赖

更新头文件和源码时，保留库要求的相对引用关系与许可文件，并同步 VS 项目项和筛选器。更新预编译库时，保持库、DLL、平台和运行库配置匹配；新增平台需要提供对应的库和运行库目录，再更新属性表的构建条件。自有配置和脚本使用 CRLF，第三方文件保留原始行尾。
