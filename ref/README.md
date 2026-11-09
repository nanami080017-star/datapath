# 参考资料

- [逻辑与互斥共享优化阅读指南](READING_GUIDE.md)：18 篇论文的宏观顺序、主要内容、精读位置与源码入口。

- problem/：题目原始图片。
- papers/、patents/：原始论文、学位论文和专利资料。
- docs/：上游工具文档及公开技术资料。
- frameworks/：XLS、CIRCT、OpenSTA 原版参考源码。
- flows/：SiliconCompiler、LibreLane、OpenROAD Flow Scripts 原版流程参考源码。
- SOURCES.json：资料的位置、来源、下载时间与版本，路径相对于 datapath_logic 根目录。
- SHA256SUMS：原始下载文件的校验值。
- metadata/：原始压缩包与下载来源记录。

参考项目不会由当前 run.py 调用。当前实际执行流程见 ../baselines/yosys_abc/README.md。
