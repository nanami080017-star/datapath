# 当前综合工具

- yosys/：上游 Yosys 源码，提交 28ba3cb926fe376fa05ebfc1b921747d011d4557。
- abc/：上游 ABC 源码及 abc 可执行程序，提交 6c5104934c90eb3cdef9ea063318f7588908a185。
- local/bin/：已安装的 Yosys，以及指向原版 ABC 的相对链接。
- local/share/yosys/：Yosys 原版映射数据。
- deps/：原有构建依赖。
- env.sh：加载 GCC 13.1.0 运行环境与工具路径，run.py 自动读取。

工具源码保持上游原版。当前命令与参数来源见 ../baselines/yosys_abc/README.md。
其他工具与公开流程仅作源码参考，位于 ../ref/frameworks/ 和 ../ref/flows/。
