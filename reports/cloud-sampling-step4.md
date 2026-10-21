# 体积云第四步验证

2026-10-02。保留第三步的密度塑形、三个预设和光照参数，默认仍为 96 个视线步、8 个阴影步。新增确定为空的块跳过、相位项缓存和风速补偿的历史帧重投影。累积开启；相机相对运动矢量用于避免远处坐标的历史漂移。

## 正确性

Debug、Release 构建及各自 27 组 GPU/资源回归通过。关闭累积时，三个预设 × 三个视角 × 三种空块步长共 27 组图像与第三步参考 Shader 对比，最大 RGBA 误差为 3.58×10⁻⁷，只有浮点舍入差异。结果见 [逐组数据](../x64/RenderChecks/cloud-sampling-differences.csv)。

固定输入的交替噪声测试中，RGB RMS 从 0.024 降至 0.00347。历史检查同时覆盖相机移动、12000 单位处的静止视角、水平风动、前景遮挡与恢复、透明像素，以及调参、光照、场景、Shader、噪声纹理、FOV、视口和相机切换时的历史重置。测试不代表任意场景都获得同样的噪声降幅。

## 同场景计时

Release，NVIDIA GeForce RTX 5060 Laptop GPU，完整视口 960×540，云采样为 480×270。每个场景至少预热 60 帧且一秒，三轮各 90 个计时样本。表中为云 Pass 的 GPU 中位数，包含历史解析与深度感知上采样。

| 场景 | 第三步参考 / ms | 最终版 / ms | 耗时下降 |
| --- | ---: | ---: | ---: |
| 积云 | 3.493 | 2.980 | 14.7% |
| 层积云 | 4.603 | 2.437 | 47.1% |
| 阴天 | 1.060 | 0.979 | 7.6% |
| 积云，水平远行 12000 单位 | 3.212 | 2.784 | 13.3% |
| 积云，朝向太阳 | 3.364 | 2.959 | 12.0% |
| 积云，背向太阳 | 3.504 | 3.054 | 12.8% |

这是本机这些固定视角的测量，GPU 频率与场景会影响耗时；不等同于编辑器整体 FPS。对照版在隔离目录中使用第三步密度/采样 Shader 并关闭累积，其他场景配置一致。

运行命令：

```powershell
tests/run-render-benchmark.ps1 -CloudPresets -CloudReference -Width 960 -Height 540 -WarmupFrames 60 -SampleFrames 90 -Rounds 3
tests/run-render-benchmark.ps1 -CloudPresets -Width 960 -Height 540 -WarmupFrames 60 -SampleFrames 90 -Rounds 3
```

对照数据：[参考版](../x64/BenchmarkRuns/20261002-162132-2ed20cf7/summary.csv)、[最终版](../x64/BenchmarkRuns/20261002-163111-4f06e6fb/summary.csv)、[时间与截图差异](../x64/BenchmarkRuns/20261002-163111-4f06e6fb/cloud-comparison.csv)。同目录保存设备、环境、隔离 Shader、逐帧采样和截图。

## 画面与完整流程

最终版六个含云场景相对参考截图的 RGB 平均绝对误差为 0.00015–0.00035，99% 的通道差异不超过 1/255。远行视角的历史漂移修正后，误差回到与原点相近的水平。实际截图检查保留了云团轮廓、蓝天空隙、阴影和朝向太阳的亮边。

[积云](../x64/BenchmarkRuns/20261002-163111-4f06e6fb/preset_cumulus.bmp) · [层积云](../x64/BenchmarkRuns/20261002-163111-4f06e6fb/preset_stratocumulus.bmp) · [阴天](../x64/BenchmarkRuns/20261002-163111-4f06e6fb/preset_overcast.bmp) · [朝向太阳](../x64/BenchmarkRuns/20261002-163111-4f06e6fb/preset_cumulus_sunward.bmp) · [远行](../x64/BenchmarkRuns/20261002-163111-4f06e6fb/preset_cumulus_travel.bmp)。

另运行 640×360、预热 30 帧、采样 30 帧一轮的 [13 场景标准检查](../x64/BenchmarkRuns/20261002-163303-5fe3837f/summary.csv)，覆盖局部云、多云、点光源、Bloom 和天空远行；全部通过，截图像素有限且无 GL 错误。

历史累积当前用于共享水平风速的天空云层。局部云体、不同风速或非零 Y 风速回到固定抖动采样；提高细节质量仍需合适的视线与阴影步数。参数说明见 [体积云文档](../docs/clouds.md)。
