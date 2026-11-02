# 公开设计流程参考源码

拉取日期：2026-11-02。位置：bm6 的 /hpc2hdd/home/haoyuetian/datapath_logic/ref/flows。

用途：查阅成熟流程的综合脚本、参数设置、SDC 约束接入方式和工具调用顺序。
三个项目均从官方 GitHub 仓库克隆，保留原版源码，固定在下列版本。

| 目录 | 版本 | 提交 | 来源 |
|---|---|---|---|
| [siliconcompiler](siliconcompiler/) | v0.38.12 | 487e42d7301a5358ca564847e20d3668276edd2b | https://github.com/siliconcompiler/siliconcompiler.git |
| [librelane](librelane/) | 3.0.17 | 64ef80802d3d189b476636d2a6b17888c03304ee | https://github.com/librelane/librelane.git |
| [OpenROAD-flow-scripts](OpenROAD-flow-scripts/) | master 固定提交 | 202c7fccbce32e86db899c54b0a0f0484f869cc3 | https://github.com/The-OpenROAD-Project/OpenROAD-flow-scripts.git |

三个项目仅作原版流程源码参考，不参与当前 run.py。
子模块依赖未递归下载；各子模块的原始路径和固定提交保留在仓库中。

## 子模块状态

    librelane:

    -9b3bebe834ccd972a5b4f10d82c32354f9a6a1ca test/designs

    -8626ef86424e219322314a553deab6c535957355 test/steps/all

    OpenROAD-flow-scripts:

    -98c6045c72b79e40023a129f11370587eec47e5a tools/OpenROAD

    -f70c2e38592d704c8cccf68433b091e82c8118f0 tools/kepler-formal

    -a5af9d690a43744bf6b2cc3dea2717c16b54621c tools/yosys
