# 对照流程与研究方法

| 内容 | 位置 | 用途 |
|---|---|---|
| 原版 Yosys＋ABC 无 SDC 流程 | yosys_abc/ | 当前 run.py 的调用来源及 CombArea 定义 |
| E-Syn | logic/E-Syn/ | 逻辑优化研究参考；当前快照 README 标题为 E-Brush，论文版本对应关系待核实 |
| e-boost | extraction/e-boost/ | e-graph 提取算法参考 |
| SparseExtraction | extraction/SparseExtraction/ | 稀疏 e-graph 提取论文的作者实验包 |

后三项尚未在本项目适配为完整 RTL 综合对照。当前实际执行工具位于 ../tools/yosys/ 与 ../tools/abc/。
