# datapath_logic

研究运算与选择逻辑的互斥共享优化。当前阶段不加载 SDC，只以标准单元网表的 CombArea 作为评测指标。

执行流程：原始 RTL → Yosys 内置综合与字级优化 → ABC 默认标准单元映射 → 面积统计。
命令和参数来源见 [Yosys＋ABC 流程](baselines/yosys_abc/README.md)。

## 使用

在 bm6 上执行：

~~~bash
cd /hpc2hdd/home/haoyuetian/datapath_logic
python3 run.py --case 1
python3 run.py --case 1 2 3 --jobs 3 --name area_baseline
~~~

- --case：公开测试集编号，范围 1–5000。
- --jobs：最大并发 case 数，范围 1–10，默认 10；--jobs 1 为逐例执行。
- 每个 case 设置 YOSYS_MAX_THREADS=4，并绑定 4 个独立物理核；并发 case 的核心不重叠。
- --name：run/ 下的新输出目录名；省略时使用北京时间。已有同名目录不会覆盖。
- run.py 自动加载 tools/env.sh 中的 GCC 13.1.0 运行环境。
- 不提供 SDC、TNS 或工具切换选项。

每次运行保存 run.json 和 results.csv。每例目录保存 flow.ys、yosys.log、netlist.v、area.json、execution.json。
results.csv 记录 case、CombArea 和整个 Yosys/ABC 进程的墙钟时间 elapsed_s。面积沿用原始 Liberty 数值，不作缩放。CSV 按完成顺序写入；execution.json 记录逐例命令、核心、线程上限与执行起止时间。

## 目录

| 目录 | 用途 |
|---|---|
| src/ | 本项目算法实现，目前为空 |
| tools/ | 当前运行的原版 Yosys、ABC 及构建依赖 |
| baselines/ | 当前流程来源与外部研究方法 |
| benchmarks/ | 官方 5000 个用例及补充研究数据 |
| ref/ | 原题、论文、参考工具源码与下载来源 |
| run/ | 当前入口生成的实验结果 |
| figure/ | SVG 电路图 |
| docs/ | 项目目录说明 |

当前尚未实现新的共享算法；run.py 调用上游原版优化。

## Git 仓库

run/ 及其所有结果、日志、PID 文件均不纳入 Git。
Yosys、ABC、公开用例及已有 Git 参考项目使用固定提交的子模块；克隆时可使用 git clone --recurse-submodules。
本机编译产物、构建依赖、下载压缩包及解压的第三方参考快照保留在 bm6，不纳入主仓库；对应来源、版本和校验值保存在 ref/SOURCES.json。
