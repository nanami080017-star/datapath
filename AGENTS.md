# datapath_logic 项目执行配置

## 当前任务范围（高优先级）

- 当前研究无时序约束的字级逻辑与互斥共享优化，只比较 CombArea。
- 原始 RTL 和 Liberty 来自 benchmarks/rtl/MutexShare-Case-Set；原始 tc.sdc 随公开仓库保留，当前流程不读取。
- 当前入口只调用原版 Yosys 和 ABC，不调用 SiliconCompiler、DC、LC 或 OpenSTA；不计算 TNS。
- 根目录 run.py 是唯一实验入口，支持 --case、--name 和 --jobs。结果保存在 run/，默认按北京时间命名。
- 单例使用 YOSYS_MAX_THREADS=4 并绑定 4 个独立物理核；--jobs 控制并发，默认 10、最大 10；不同 case 的核心不重叠。不自行增加时延目标、驱动、负载或单元过滤。

## 上游流程与来源（高优先级）

- 以题目原文、公开用例仓库、对应工具版本手册和上游源码为依据。命令、参数、单位与指标来源记录在 baselines/yosys_abc/README.md。
- 使用 Yosys 内置 synth -top tc -noabc，再通过 abc -exe 指定原版 ABC、-liberty 指定原始库，执行上游默认 ABC_COMMAND_LIB；清理后导出网表并用 stat -json -liberty 统计面积。
- 禁止自行拼接 ABC 优化序列或加入 -script、-D、-constr 等额外配置。流程变更必须有明确来源与用户授权。
- Yosys、ABC 及其他第三方源码保持上游原版。未经用户明确授权，禁止修改源码、打补丁或替换工具内部算法。
- 原始 RTL、SDC、Liberty 不改写。无明确文档依据的步骤和换算不实现。
- 已删除的旧工作和结果不作为新基线依据，不复用其实现，不将补测指标并入当前场景。
- 保存实际工具版本、源码提交、输入与工具摘要、执行脚本、原始报告和逐例运行时间。
- 验证采用当前改动所需的最小有效检查，不反复冒烟测试，不自动运行全量实验。
- 研究参考源码、论文和原始下载记录保留在 ref/、baselines/ 和 benchmarks/，当前执行流程以根目录入口与流程文档为准。

## 服务器工作位置（高优先级）

- 本项目编辑、开发、测试、实验、研究和结果分析均直接在 bm6 进行，根目录为 /hpc2hdd/home/haoyuetian/datapath_logic。
- 本机项目目录只作为 SSH 入口，不在本地编写后上传，不同步或备份工程和实验结果。
- 优先复用已认证的 bm6 SSH 会话；会话失效后自主重连，任务结束后保持连接。
- 解释问题时直接说明问题与处理结论，不追加题目和用户未要求的指标、检查或修复任务。
- 电路图使用 SVG，统一保存在 figure/。
