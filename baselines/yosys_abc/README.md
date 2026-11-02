# 无 SDC 的 Yosys＋ABC 面积流程

本项目选用 Yosys 上游内置综合流程和默认 ABC 库映射流程。评测输入是原始 tc.v 和 gscl45nm.lib，顶层为 tc；当前只评测 CombArea。
流程来源与项目输入、执行方式分别记录，未将本流程称为题目方提供的官方评测脚本。

## 固定来源

| 对象 | 提交 |
|---|---|
| Yosys | 28ba3cb926fe376fa05ebfc1b921747d011d4557 |
| ABC | 6c5104934c90eb3cdef9ea063318f7588908a185 |
| MutexShare-Case-Set | 6a75068034c4ce3b7b8b01d86b9361babff54ca2 |

来源仓库：
- https://github.com/YosysHQ/yosys
- https://github.com/YosysHQ/abc
- https://github.com/tianchen5246/MutexShare-Case-Set

## 实际命令与依据

run.py 为每个用例生成以下脚本，其中路径由当前项目位置确定：

~~~yosys
read_verilog "<原始 tc.v>"
synth -top tc -noabc
abc -exe "<原版 ABC 程序>" -liberty "<原始 gscl45nm.lib>"
clean
write_verilog netlist.v
tee -o area.json stat -json -liberty "<原始 gscl45nm.lib>"
~~~

| 步骤 | 上游依据 |
|---|---|
| read_verilog、techmap、abc -liberty、clean、write_verilog 的 ASIC 调用顺序 | [完整官方示例 counter.ys](https://github.com/YosysHQ/yosys/blob/28ba3cb926fe376fa05ebfc1b921747d011d4557/docs/source/code_examples/intro/counter.ys) |
| 用 synth 执行内置综合、-top 选顶层、-noabc 将 ABC 阶段交给下一条命令 | [synth.cc 帮助与实现](https://github.com/YosysHQ/yosys/blob/28ba3cb926fe376fa05ebfc1b921747d011d4557/techlibs/common/synth.cc)；其中 fine 阶段调用 opt -full，随后执行 techmap |
| -exe 选择实际 ABC 程序、-liberty 指定库 | [ABC 接口手册](https://yosyshq.readthedocs.io/projects/yosys/en/stable/cmd/index_passes_techmap.html#abc-use-abc-for-technology-mapping) |
| 不指定自定义脚本及约束时选择默认 ABC_COMMAND_LIB | [abc.cc 第 32 行默认脚本](https://github.com/YosysHQ/yosys/blob/28ba3cb926fe376fa05ebfc1b921747d011d4557/passes/techmap/abc.cc#L32)；同文件第 1084–1094 行实现脚本选择及延迟参数替换 |
| stat -liberty 读取单元面积，-json 输出机器可读报告，tee 保存报告 | [stat.cc 帮助与实现](https://github.com/YosysHQ/yosys/blob/28ba3cb926fe376fa05ebfc1b921747d011d4557/passes/cmds/stat.cc)；帮助明确说明用 tee 保存 JSON |

Yosys 自动执行的默认 ABC 脚本来自源码中的 ABC_COMMAND_LIB，不另写 -script，不设置 -D 或 -constr。-exe 仅选择改名后项目内的原版 ABC 程序。
运行日志保留 ABC 实际执行的命令。

## 指标与输出

CombArea = stat 报告 design.area − design.sequential_area。对应源码 stat.cc 的单元面积累加与 sequential_area 字段。
公开用例是组合逻辑；不读取 SDC，不计算 TNS。面积数值使用原始 Liberty 的 area 字段单位，不自行换算为物理面积。

run.json 保存参数、工具实际版本、源码提交、工具与输入 SHA256、入口脚本 SHA256。
每例 flow.ys、yosys.log、netlist.v、area.json 分别保留综合命令、原始日志、映射网表和原始面积报告；execution.json 保存逐例资源配置与执行起止时间。
results.csv 记录逐例 CombArea 与 elapsed_s；elapsed_s 包含 Yosys 进程启动、综合、ABC 映射、导出与统计，用于记录执行时间。

Python 3 标准库负责用例选择与文件输出；tools/env.sh 加载已安装的 GCC 13.1.0，满足现有二进制的 libstdc++ 运行依赖。
用户指定的执行配置：每例设置 YOSYS_MAX_THREADS=4，并通过 taskset 绑定 4 个独立物理核；--jobs 支持 1–10 例并发，默认 10。通过 lscpu 的 socket/core 标识选取物理核，不重复使用 SMT 兄弟线程。核心分组与线程上限写入 run.json，实际 case 分配写入 execution.json。
Yosys 的并行步骤受线程上限控制；opt_share 和单个 ABC 映射任务仍按原版算法执行。没有修改 ABC 默认脚本。
资源配置与调度来源：
- [当前版本 Yosys 线程控制源码](https://github.com/YosysHQ/yosys/blob/28ba3cb926fe376fa05ebfc1b921747d011d4557/kernel/threading.cc#L6)：YOSYS_MAX_THREADS 及线程池上限。
- [taskset 手册](https://man7.org/linux/man-pages/man1/taskset.1.html)：设置进程 CPU 亲和性；子进程继承。
- [Python 3.8 concurrent.futures](https://docs.python.org/3.8/library/concurrent.futures.html)：ThreadPoolExecutor 调度独立外部综合进程，as_completed 收集完成结果。
4 核与最多 10 例并发是用户执行配置；没有将其标为工具默认值。

## 验证状态

已完成一次 case_0001 的实际运行：

- 命令：python3 run.py --case 1 --name verify_case_0001。
- 输出：run/verify_case_0001/。
- CombArea：15424.013800；映射单元数：5404；sequential_area：0.0。
- 整个 Yosys/ABC 进程 elapsed_s：1.298269。
- 已核对日志中的默认 ABC 命令、映射单元属于原始库、CSV 数值与原始 JSON 报告一致。
- Yosys、ABC 和用例仓库的受版本控制文件均未修改。
- 未执行 5000 例全量实验。

## 并发与线程上限验证

- 命令：python3 run.py --case 1 2 --jobs 2 --name verify_parallel_4t。
- 两例核心分别为 [0, 1, 2, 3]、[4, 5, 6, 7]，均设置 YOSYS_MAX_THREADS=4。
- 核心分组互不重叠，执行时间区间重叠 1.286993 秒，已确认两例并发。
- case_0001 的 CombArea 与先前单例验证一致；两例 CSV 均与原始面积报告一致。
- 原版工具二进制摘要与先前单例验证一致。
- 未运行 5000 例全量实验，未进行 3 GHz 运行时间归一化。
