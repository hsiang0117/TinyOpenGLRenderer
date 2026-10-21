# 体积云

## 云层范围

编辑器添加 `Cloud Layer` 后默认使用 `Sky Layer`。云底高度和最大厚度固定在世界空间，实际云顶高度可随 XZ 位置变化，相机沿 XZ 移动不会离开云层范围。天空云层不使用对象的 Transform；`Local Volume` 使用平移、缩放后的世界轴对齐包围盒。

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `LayerBottom` | 100 | 世界空间云底高度 |
| `LayerThickness` | 200 | 云层最大厚度 |
| `MaxTraceDistance` | 10000 | 视线和光线的最大追踪距离 |
| `DistanceFadeStart` | 8000 | 视线密度从这里开始平滑淡出，到最大距离为零 |

## 形态与尺度

新云层使用 `Scattered Cumulus` 预设。Inspector 的 `Apply preset` 可以切换积云、层积云和阴天；预设只修改形态和光照，保留云层高度、范围、风速、局部边缘衰减和采样质量。

天气分区、区内云量、云体、边缘细节分别控制。调整大范围晴空与云区使用 `RegionScale` / `RegionCoverage`，区内云量使用 `WeatherScale` / `CoverageLow` / `CoverageHigh`，云团尺寸使用 `BaseNoiseScale`。

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `CloudType` | 1 | 0 为层云，0.5 为层积云，1 为积云；控制高度包络和云体轮廓阈值 |
| `RegionScale` | 0.0004 | 大范围天气分区的世界坐标频率；格点间距为 1/scale，默认 2500 世界单位 |
| `RegionCoverage` | 0.5 | 大范围覆盖控制量；越小，成片晴空越多；不是精确的面积比例 |
| `DensityVariation` | 0.65 | 区域密度变化强度；0 表示各处共用同一个密度倍数 |
| `HeightVariation` | 0.4 | 区域厚度变化强度；默认局部厚度在最大厚度的 60%–100% 之间变化，范围 0–0.8 |
| `WeatherScale` | 0.004 | 天气分区内部的云量变化频率；越小，区内云群越大 |
| `CoverageLow` / `CoverageHigh` | 0 / 0.95 | 天气值从低到高对应的覆盖率区间 |
| `BaseNoiseScale` | 0.0028 | 云体噪声的采样频率；越小，云体结构越大 |
| `WarpScale` | 0.003 | 水平域扭曲的频率；越小，扭曲变化越缓 |
| `WarpStrength` | 50 | X、Z 每个分量的最大位移，单位为世界单位；Y 不偏移 |
| `DetailScale` | 0.02 | 边缘细节的频率；越大，细节越细 |
| `DetailErosion` | 0.16 | 细节侵蚀强度 |
| `WindVelocity` | (1, 0, 1) | 世界单位/秒；所有噪声随这个方向一起移动 |

天空天气使用世界坐标整数格点哈希和连续插值，不循环铺设有限天气图。低频分区形成成片晴空与云群；另一个独立的低频场控制区域密度和厚度，再由三个频率组合的区内天气控制小云团。所有区域场随水平风一起移动，不跟随相机。局部云体沿用天气纹理及其包围盒映射，不应用天空分区参数。基础 Perlin 纹理只有一个周期噪声频率；Shader 在旋转域中组合四层 FBM，每层改变方向并以 2.03 倍递增频率。独立的 XZ 域扭曲进一步打散短周期重复；细节纹理也在旋转域中采样。

`data/shader/cloudNoise.glsl` 保存采样函数，`cloudDensity.glsl` 保存 GPU 参数布局和密度塑形。四层 FBM 除以振幅和 0.9375，再用高度包络与轮廓阈值形成云体，最后用 Worley 侵蚀边缘。层云采用较软的轮廓阈值，积云采用较高阈值形成云团。

天气值映射到 `CoverageLow` / `CoverageHigh` 后，经过 0.4–0.6 区间的平滑门限，并乘以大范围分区遮罩，控制整列云体是否出现。降低覆盖率可以产生蓝天空隙。两个值都为 0 或 `RegionCoverage` 为 0 时没有天空云；两个区内覆盖率与 `RegionCoverage` 都为 1 时所有区域都可生成云体，最终透明度仍由云体形态和光学密度决定。将 `RegionCoverage` 设为 1、两项 Variation 设为 0，可回到统一区域的分布方式。

积云预设包含明显的分区间隙；层积云的分区覆盖量为 0.8，保留较连贯的云群；阴天为 1，保持连续云层。低角度视线穿过多个云团仍会积累较大光学厚度，分区用于打破均匀的云墙，不额外按视角削弱介质密度。

视线和光线调用同一个完整密度函数，包含高度塑形、覆盖率、细节侵蚀和密度倍数。空区域先检查云层边界、高度包络和天气覆盖，再采样 3D 噪声。单层 Perlin 缓存会根据生成版本自动重建。

## 密度与光照

| 参数 | 积云默认值 | 作用 |
| --- | --- | --- |
| `DensityMultiplier` | 2 | 密度倍数；同时影响透明度和自阴影，0 时介质消失 |
| `ExtinctionCoefficient` | 0.08 | 密度为 1 时的消光系数，单位为世界单位的倒数；视线和光线共用 |
| `ScatteringAlbedo` | 0.95 | 散射占总消光的比例；0 为纯吸收，1 为纯散射 |
| `AmbientStrength` | 0.45 | 天空漫射光强度，控制云底和阴影区域的亮度 |
| `SunStrength` | 3 | 云接收的太阳光倍率，乘以场景方向光的颜色与亮度 |
| `PhaseForwardG` | 0.65 | HG 前向散射强度；越大，朝向太阳观察时的亮边越强 |
| `MultipleScattering` | 0.65 | 四阶散射近似中高阶项的强度；0 时只保留直接散射 |

局部消光为 `sigma_t = shapedDensity * DensityMultiplier * ExtinctionCoefficient`，散射系数为 `sigma_s = sigma_t * ScatteringAlbedo`。每一步同时积分透射率和散射源，输出预乘 alpha。零消光时，透明度和散射贡献都为零。计算依据见 [PBRT 的体积散射采样](https://www.pbr-book.org/3ed-2018/Light_Transport_II_Volume_Rendering/Sampling_Volume_Scattering)。

双叶 HG 相位函数使用归一化的前向与后向分量。高阶散射通过逐阶降低权重、消光与方向性近似，天空漫射光采用随高度变化的颜色；这两项用于实时外观调校，没有计算完整的多次散射或大气传输。原来的 `DensityGain`、`ViewAttenuation`、`LightAttenuation` 和 `PowderStrength` 已替换为上面的参数。

建议先选预设，再按以下顺序调整：

1. 先用 `RegionCoverage` 调成片晴空与云区，用 `RegionScale` 调分区尺寸，再用 `CoverageLow` / `CoverageHigh` 和 `WeatherScale` 调区内云量。
2. 用 `BaseNoiseScale` 控制云团尺寸，再调整域扭曲和边缘侵蚀。
3. 用 `DensityMultiplier` 调整体厚实程度，用 `DensityVariation` 调区域疏密差异；`LayerThickness` 控制最大厚度，`HeightVariation` 控制云顶起伏。
4. 用 `SunStrength`、`AmbientStrength` 和方向光角度调明暗，再调整前向散射与高阶散射。

## 采样与历史帧

| 参数 | 默认值 | 作用 |
| --- | --- | --- |
| `ViewStepWorldSize` | 2 | 视线采样的目标世界间隔；实际步数受上下限约束 |
| `ViewMaxSteps` | 96 | 每个介质分段的最大视线步数；Inspector 范围为 32–256 |
| `LightSteps` | 8 | 每个云体的阴影采样数；Inspector 范围为 4–24 |
| `EmptyStepMultiplier` | 4 | 一次最多跳过多少个确定为空的视线采样；1 关闭块跳过 |
| `TemporalAccumulation` | 开启 | 用历史帧降低采样噪声，随后进行深度感知上采样 |

默认仍在半分辨率下使用最多 96 个视线步和 8 个阴影步。高度包络、区内覆盖率和大范围分区给出保守的密度上界，只有整个块能确定为空时才跳过；不确定时继续原来的细采样网格。高度证明包含最大厚度变化对应的全部归一化高度范围，不能沿用固定云顶的空块判断。天气范围跨越格点单元时使用完整上界，因此不会根据两个空端点推断中间也为空。相位函数中沿同一视线不变的项提前计算；零散射或没有太阳贡献时省去阴影追踪。

`cloudRaymarch.frag` 同时输出预乘颜色、按消光贡献加权的云距离和前景距离。`cloudTemporal.frag` 根据相机相对位置计算到上一帧的运动矢量，并按风速补偿位移，避免远处世界坐标让静止画面产生历史漂移；云距离、前景遮挡或画面范围不匹配时拒绝历史。历史颜色和 alpha 限制在当前 3×3 邻域范围内再混合，透明像素直接使用当前结果。

`cloudTemporal` 持有两组颜色与距离缓存。云参数、方向光、场景、Shader 程序、噪声纹理、FOV 或视口变化会重置历史；相机单帧移动超过 250 世界单位、转动约 20 度以上、时间倒退或帧间隔超过 0.5 秒也会重置。正常移动继续重投影。视口缩放保留纹理 ID，关闭渲染器时在有效 GL 上下文中释放缓存。

当前累积适用于所有云体都是 `Sky Layer`、启用该选项、共享同一个水平风速的场景。局部云体、不同风速或非零 Y 风速使用固定抖动的原始采样路径。高度包络固定在世界中，Y 方向的噪声运动不能代表整个云体的平移。累积不是超分辨率，也不会补足漏采的薄云；低角度视线和高频细节需要时仍应提高步数。

## 验证

运行 `tests/run-render-safety.ps1 -Configuration Debug` 或 `Release`。GPU 检查调用生产采样和密度函数，覆盖噪声重复、风速、预设的空白云列、覆盖率单调性、细节侵蚀、密度倍数和高度边界。相关系数输出到 `x64/RenderChecks/cloud-noise-correlations.csv`，预设统计输出到 `cloud-preset-density.csv`；`cloud-region-distribution.csv` 对比约 750×600 世界单位窗口中的平均垂直透明度，验证大范围分布差异，而非仅降低整体密度。渲染回归另检查云层远行、深度遮挡、混合介质、顺序无关性，以及均匀介质的解析消光、散射、纯吸收和步长一致性。

采样回归检查保守天气上下界与空块证明，包括默认厚度变化和 0.8 的极值。关闭区域塑形后保留上一阶段 Shader 的三种预设对照；新区域塑形另与完整细网格比较，覆盖 8° / 55°、远行及云内视角，结果写入 `cloud-sampling-differences.csv`。历史帧检查涵盖噪声衰减、远处静止视角、相机平移、风动、前景遮挡与恢复、透明像素、各项历史失效条件和缓存释放。

运行 `tests/run-render-benchmark.ps1` 可获取实际云层画面和 GPU 计时。`sky_layer` 在原点观察天空，`sky_travel` 水平移动 12000 单位；各自有不含云的对照场景。场景图片和逐 Pass 数据输出到脚本报告的隔离目录。

使用 `tests/run-render-benchmark.ps1 -CloudPresets` 检查三种预设、水平移动 12000 单位、朝向与背向太阳的画面。该模式在隔离目录中使用无云的蓝色渐变背景，便于观察体积云自身的形态。原项目天空盒和渲染设置不受影响。每个场景都有 BMP 截图和 GPU 计时。

添加 `-CloudReference` 可在隔离目录中运行第三阶段的密度和固定采样 Shader，并关闭历史累积，作为外观与性能对照。当前版与对照版必须使用相同视口、预设和采样轮数；云 Pass 的计时包含历史解析与上采样。

使用 `-CloudHorizon` 检查无前景遮挡的 8° / 15° / 55° 视角、三个远行位置以及三种预设，包含关闭历史累积和提高步数的积云对照。增加 `-CloudDistributionReference` 可关闭区域塑形，同时保持当前采样与历史累积，用于同场景对比 XZ 分布。区域分布的诊断、截图和测试结果见 [XZ 分布验证记录](../reports/cloud-distribution.md)。

本轮构建、GPU 回归、同场景计时与截图对照见 [第四步验证记录](../reports/cloud-sampling-step4.md)。
