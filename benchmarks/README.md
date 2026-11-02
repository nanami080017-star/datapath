# 测试数据

- rtl/MutexShare-Case-Set/：题目指定的 5000 个公开用例。原始 tc.v、tc.sdc 和 lib/gscl45nm.lib 均保留。
- rtl/DatapathBench/：补充的数据通路电路集合，含 SystemVerilog、MLIR 和验证资料。
- extraction/extraction-gym/：e-graph JSON 实例及测试框架。

当前 run.py 仅运行 MutexShare 用例的无 SDC 场景，读取 tc.v 与原始 Liberty，输出 CombArea。
完整题目与数据集说明保留在原始资料中；当前阶段的范围见根目录 README.md。
