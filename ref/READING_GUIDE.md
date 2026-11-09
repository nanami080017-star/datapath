# 逻辑与互斥共享优化阅读指南

当前阅读目标是理解无 SDC 条件下的字级逻辑与互斥共享优化，重点关注多扇出情况下的候选识别、变换合法性和面积收益。最终实验指标沿用项目现有的 CombArea 流程。

本指南覆盖项目现有的 18 篇论文，并给出研究源码、综合工具和测试集的阅读入口。阅读顺序按问题之间的依赖安排；每篇的工作概括依据原文，优先级和阅读顺序是针对当前课题的安排。

## 一 宏观阅读顺序

| 阶段 | 阅读内容 | 需要建立的认识 |
|---|---|---|
| 1 概念与工具定位 | 1 Zimmermann；2 Yosys | 数据通路优化、字级表示、门级实现与面积统计分别处于什么位置 |
| 2 直接共享方法 | 3 MCTS；4 Yu 2016；5 Yu 2017；6 Shang 2024 | 怎样形成共享候选，怎样移动 MUX，以及共享结构需要付出什么连接成本 |
| 3 等价重写与条件语义 | 7 ROVER；8 DAC 2023；9 TCAD 2025 | 怎样同时保留多种实现，怎样表达分支条件，怎样统一选择共享结构 |
| 4 实现与提取算法 | 10 Nextmap；11 e-boost；12 SparseExtraction | 怎样组织实际代码，以及怎样在给定候选图上选择实现 |
| 5 按问题补充 | 13 Port Assignment；14 E-Syn；15 ASPEN | 操作数输入分配、实际面积的成本估计、规则生成与反馈搜索 |
| 6 背景与专题查阅 | 16 McFarland；17 Share with Care；18 Coward 学位论文 | HLS 术语、调度与共享的关系，以及 ROVER 系列的完整推导 |

**第一次阅读的重点顺序：Zimmermann 第 2、3 节，Yosys 两个共享 pass，MCTS，Yu 2016，Yu 2017，ROVER，Constraint-Aware 两篇，Nextmap。** Shang 2024 可用于扩展共同结构匹配；e-boost 和 SparseExtraction 在研究提取算法时精读。

MCTS 介绍的矩阵问题比较具体，适合先理解共享与 MUX 成本。Yu 系列展示结构匹配和 MUX 移动。ROVER 将重写和全局实现选择结合起来。随后阅读条件优化，能够进一步理解分支中的逻辑事实怎样参与变换。

## 二 阅读时区分的研究问题

**公共子表达式共享**关注相同计算结果的复用。同一节点被多个后续操作使用时，面积模型需要按实际实现次数计算成本。ROVER 的共享感知提取、e-boost 和 SparseExtraction 与这一问题相关。

**互斥资源共享**关注不同运算在互斥条件下共用硬件。需要决定共用哪些运算器，以及如何生成输入选择逻辑。MCTS、MUX 移动方法和 ROVER 的部分重写规则涉及这一问题。

**多扇出**表示同一输出驱动多个使用位置。本题关注的关键场景是：候选运算除了当前 MUX，还被其他逻辑使用。阅读相关算法时，需要看它是否处理所有使用者、是否排除该候选，以及变换后哪些旧节点仍需保留。

**条件相关优化**利用 RTL 内部的分支条件和信号值域。两篇 Constraint-Aware 论文的核心 constraint 指这些逻辑条件，当前无 SDC 的研究也可使用其中的语义分析思想。

### 多扇出相关内容的定位

| 资料 | 明确涉及的内容 | 当前阅读用途 |
|---|---|---|
| Yosys opt_share | 候选输出需要专用于目标 MUX 的输入连接 | 明确题目指出的结构限制 |
| Yosys share | 激活模式分析与 SAT 共享判定 | 了解已有的条件分析机制 |
| Yu 2017 | 用侧扇出和重汇聚扇出信息辅助同构匹配；排除根 MUX 数据输入有额外扇出的候选 | 同时学习匹配方法和候选边界 |
| Shang 2024 | 共同拓扑和重配置逻辑；同样保留根 MUX 输入额外扇出的排除条件 | 学习扩大匹配范围的方法 |
| ROVER | MUX 与算术重写，以及多输出共享 DAG 的全局面积选择 | 学习局部重写和全局收益评估的结合 |
| Constraint-Aware 两篇 | 条件限定下的等价关系、值域分析和上下文表示 | 学习条件信息如何决定变换合法性 |
| e-boost 与 SparseExtraction | 给定 e-graph 的共享结构提取 | 学习实现选择算法 |

本批文献提供上述相关机制，尚未找到直接以解除 Yosys opt_share 多扇出限制为目标的完整方案。精读时应分别记录候选范围、变换规则和面积模型。

## 三 按顺序阅读各篇论文

### 1 Zimmermann 2009

**Datapath Synthesis for Standard-Cell Design**

阅读安排：快速阅读。研究方向：综合流程与数据通路优化概览。

文章归纳标准单元设计中的三个阶段：从 RTL 提取算术数据通路，对数据通路进行高层算术优化，再生成门级网表。介绍公共子表达式共享与取消共享、互斥运算共享、代数变换，以及乘法器和加法器的不同实现结构。它提供整个综合过程的组织方式，适合先建立各类优化所在层次的认识。

**与本题的关系。** 用于确定课题位置：互斥共享属于数据通路优化的一部分，优化后的结构还需要经过门级实现和技术映射，最终得到 CombArea。

**精读位置。** 先读第 2 节 Datapath Extraction、第 3 节 Datapath Optimization，再浏览第 4 节 Datapath Generation。当前可以略读低功耗和时序相关细节。

**阅读后需要回答。** 共享优化改变了哪一层的结构？最终标准单元面积在哪一层形成？

**源码情况。** 本地保存论文。涉及的商业综合实现没有配套源码收录。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Zimmermann2009_Datapath_Synthesis.pdf) · [原文来源](https://iis-people.ee.ethz.ch/~zimmi/publications/datapath_synthesis.pdf)

### 2 Yosys 2013 与当前源码

**Yosys — A Free Verilog Synthesis Suite**

阅读安排：快速阅读并结合代码。研究方向：综合框架和中间表示。

介绍 Yosys 的前端、RTLIL 中间表示、优化 pass 和后端，以及细粒度与粗粒度综合的组织方式。粗粒度表示可以保留加法、乘法、MUX 等操作，便于在变成布尔门之前进行研究。论文适合了解框架结构；当前 opt_share 和 share 的具体行为由项目所用版本源码确定。

**与本题的关系。** 用于定位实验中的优化插入位置和数据表示。需要分别阅读 opt_share 的局部结构条件与 share 的激活条件分析，两个 pass 的能力范围分别记录。

**精读位置。** 论文中 The Yosys Framework 及示例流程；源码依次看 opt_share.cc 的 help 和候选筛选、share.cc 的激活模式分析与 SAT 检查，再看 synth.cc 的调用顺序。

**阅读后需要回答。** 同一 RTL 在什么阶段还保留字级运算？两个共享 pass 的候选条件分别是什么？

**源码情况。** 原版源码在 tools/yosys；具体入口见后文源码阅读顺序。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Yosys_Austrochip2013.pdf) · [原文来源](https://yosyshq.net/yosys/files/yosys-austrochip2013.pdf)

### 3 Yang 2023 MCTS

**A Resource Sharing Approach for Logic Synthesis Based on Monte Carlo Tree Search**

阅读安排：精读。研究方向：互斥分支资源共享与 MUX 输入分配。

研究互斥分支之间如何共用运算器。作者将分支及其操作数写成矩阵，把输入分配转成列组合决策，用蒙特卡洛树搜索比较不同组合。优化重点是共享运算器输入端的 MUX 成本。论文还讨论不同运算类型、级联运算和各分支运算数量不同的处理。

**与本题的关系。** 直接帮助理解共享的面积收益：运算器数量减少以后，MUX 的数量和输入数量仍会影响最终面积。论文的问题模型以互斥分支为起点；任意多扇出 RTL 中的使用条件分析需要另行研究。

**精读位置。** 第 II 节 Problem Formulation 的图 1 至图 5；第 III-A 节的矩阵决策；第 III-B 节的级联、对齐和结果替换。

**阅读后需要回答。** 搜索决策究竟改变了哪些操作数的连接？论文的 MUX 成本与本项目映射后的 CombArea 如何对应？

**源码情况。** baselines/logic/Resource-Sharing 是作者公开研究原型，包含 MCTS、贪心、穷举和 Verilog 生成。README 自称 demo，未确认为完整论文复现实验包。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Yang2023_MCTS_Resource_Sharing.pdf) · [原文来源](https://numbda.cs.tsinghua.edu.cn/papers/iseda23.pdf)

### 4 Yu 2016

**DAG-Aware Logic Synthesis of Datapaths**

阅读安排：精读方法。研究方向：AIG 上的 MUX 移动与共同逻辑识别。

将门级逻辑转成 AIG，识别共用控制信号的向量 MUX，再从 MUX 两侧寻找共同逻辑。方法通过子图同构识别可合并结构，将输出端的选择操作移动到共同逻辑的输入边界，并评估减少的逻辑与新增 MUX 的成本。

**与本题的关系。** 这篇把共享从整个加法器、乘法器扩展到组合逻辑子图。可学习候选边界、共同子图和面积收益的建模方式。DAG 表示允许图中存在共享结构，具体可变换范围需要结合候选筛选和重连规则阅读。

**精读位置。** 第 3.1 节 Pre-processing、第 3.2 节 Multiplexer relocation；结合第 4 节理解共同逻辑规模与输入边界规模的关系。

**阅读后需要回答。** 合并掉多少内部逻辑后，新增输入 MUX 才有面积收益？

**源码情况。** 本地保存论文，尚未收录作者实现。当前 PDF 为 NTHU 课程镜像的出版版本；作者 corrected-version 链接的下载失败记录在 SOURCES.json。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Yu2016_DAG_Aware_Datapath.pdf) · [原文来源](https://www.cs.nthu.edu.tw/~tingting/Als_23/presentation_paper/paper15.pdf)

### 5 Yu 2017

**Advanced Datapath Synthesis using Graph Isomorphism**

阅读安排：精读。研究方向：标准单元 DAG 匹配及扇出信息利用。

沿用 MUX 移动实现共享的思路，直接在标准单元 DAG 上寻找共同逻辑。利用单元类型、端口特性和侧扇出信息减少图同构匹配的候选组合；还研究通过额外逻辑支持近似同构。其主要改进集中在匹配效率和匹配范围。

**与本题的关系。** 论文明确讨论侧扇出和重汇聚扇出，并用这些信息辅助匹配。第 III 节预处理同时规定，若 MUX 数据输入还有其他扇出，就排除该 MUX 候选。这个候选限制与本题需要关注的额外使用者问题直接相关。

**精读位置。** 第 III 节开头的预处理条件；III-A.2 Including side fanout information；随后读近似同构部分。

**阅读后需要回答。** 哪些扇出被用作匹配信息，哪些扇出会直接排除共享候选？

**源码情况。** 本地保存论文，未找到作者公开实现。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Yu2017_Graph_Isomorphism.pdf) · [原文来源](https://arxiv.org/pdf/1708.09597)

### 6 Shang 2024

**Directed Acyclic Graph-Based Datapath Synthesis Using Graph Isomorphism and Gate Reconfiguration**

阅读安排：方法补充。研究方向：拓扑匹配和可重配置逻辑。

在共同逻辑匹配的基础上，把候选扩展到拓扑相同而局部门功能不同的网络。通过加入受选择信号控制的可重配置逻辑，使同一网络在不同条件下实现所需功能，再进行 MUX 移动和面积评估。加减法电路是文中的重要示例。

**与本题的关系。** 提供扩大共享候选范围的方法：可共享结构可以超出完全相同的门类型。第 3 节仍排除数据输入有额外扇出的根 MUX，阅读时需保留这一适用条件。

**精读位置。** 第 3 节 Proposed Methodology 的候选条件与共同拓扑；第 4 节 Implementation 的重配置实例和算法 1。

**阅读后需要回答。** 放宽结构匹配条件后，新增加的选择与重配置逻辑是否抵消了共享收益？

**源码情况。** 本地保存论文，未收录配套实现。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Shang2024_Gate_Reconfiguration.pdf) · [原文来源](https://mdpi-res.com/d_attachment/chips/chips-03-00008/article_deploy/chips-03-00008.pdf)

### 7 ROVER 2024

**ROVER: RTL Optimization via Verified E-Graph Rewriting**

阅读安排：核心精读。研究方向：字级等价重写与共享感知的全局提取。

ROVER 读取 RTL，将多个等价数据通路实现保存在 e-graph 中。重写规则覆盖算术变换、MUX 与算术运算交换、位宽与符号条件及运算融合。最后通过整数线性规划选择一份实现，输出 SystemVerilog，并提供等价验证流程。

**与本题的关系。** 表 II 包含互斥加法或乘法共享规则；第 V-B 节统一处理公共子表达式复用，为每个被选节点计算一次成本，同时满足所有输出。可重点研究局部变换与全局共享成本如何结合。该文没有以解除 Yosys opt_share 的多扇出限制作为独立实验目标。

**精读位置。** 先看图 2 和第 II 节 e-graph 基础，再看表 II、第 III 节位宽和符号语义，最后精读 V-A 成本模型及 V-B Common Sub-Expression Aware Extraction。

**阅读后需要回答。** 一处运算改写后，其他使用者仍需要哪些节点？提取模型如何计算整张 DAG 的成本？

**源码情况。** 完整 ROVER 优化器未找到公开源码。benchmarks/rtl/DatapathBench 是公开 RTL 测试集。Nextmap 提供部分 ROVER 算术与逻辑规则的公开重实现。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/ROVER2024.pdf) · [原文来源](https://arxiv.org/pdf/2406.12421)

### 8 Constraint-Aware DAC 2023

**Automating Constraint-Aware Datapath Optimization using E-Graphs**

阅读安排：精读条件语义。研究方向：分支条件、值域分析和子域等价。

研究某些变换只在特定输入取值范围内成立时，怎样在 e-graph 中表达和使用它们。方法利用分支条件和抽象解释推导中间信号的取值范围，并据此进行位宽缩减、条件重写和结构优化。论文用浮点减法器展示条件信息如何产生有效的优化机会。

**与本题的关系。** 这里的 constraint 指 RTL 分支条件和输入值域。当前无 SDC 的逻辑优化也可以阅读这一部分，用于理解条件成立范围与变换合法性的关系。

**精读位置。** 第 III 节 Sub-Domain Equivalences 和 Sub-Domains in E-Graphs；第 IV 节的位宽缩减、条件重写。当前可略读时延建模。

**阅读后需要回答。** 在某个分支内成立的等价关系，怎样限定其使用范围？

**源码情况。** 本地保存论文。文中给出重写资料链接 [重写资料](https://figshare.com/s/e3ab2850662d24991cbc)，用于查阅文中规则。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Constraint_Aware_DAC2023.pdf) · [原文来源](https://arxiv.org/pdf/2303.01839)

### 9 Constraint-Aware TCAD 2025

**Constraint-Aware E-Graph Rewriting for Hardware Performance Optimization**

阅读安排：接续上一篇精读。研究方向：ASSUME 语义、位向量值域分析及条件优化。

扩展 DAC 2023 的条件优化工作，定义 ASSUME 操作符，将表达式与其成立条件一起表示，从而在一个 e-graph 中编码不同条件下的等价关系。结合位精确的有符号值域分析实现 RTL 优化，并加入面积与时延权衡的提取模型。

**与本题的关系。** 对本题有价值的是 ASSUME 的创建、传播、条件细化，以及同一表达式在不同上下文下的语义。当前先研究这些逻辑部分；面积与时延的联合目标可留作后续阅读。

**精读位置。** 第 III 节 Localizing Constraint-Aware Optimization、表 I 的 ASSUME 重写，以及 IV-A Value Range Analysis、IV-B 的 RTL 重写。

**阅读后需要回答。** 同一个中间结果被多个上下文使用时，各上下文条件如何表达？哪些等价关系可以跨上下文复用？

**源码情况。** 本地保存论文；完整作者优化器源码未收录。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Constraint_Aware_TCAD2025.pdf) · [原文来源](https://spiral.imperial.ac.uk/bitstreams/de68c6e8-a66d-42d6-ae3e-16eab9fc9782/download)

### 10 Nextmap 2026

**Improving Equality Saturation for EDA via Semantic E-Graphs**

阅读安排：结合源码阅读。研究方向：字级与位级表示联通以及联合优化和映射。

提出 semantic e-graphs，通过底层位语义维护字向量表达式的等价关系，减少用大量语法重写连接字级和位级表示所产生的开销。Nextmap 在同一框架中组织多种优化与技术映射，并通过 ILP 提取实现。

**与本题的关系。** 提供可检查的完整公开框架，适合观察 Yosys 网表进入 e-graph 后如何重写、保留等价选择和提取。第 6.3 节说明实现了 ROVER 核心算术与逻辑规则的一部分，未实现常量展开等专门规则。

**精读位置。** 先看图 1、第 2 节问题例子及第 5 节系统，再看第 6.3 节。随后结合 rewrites 和 extracts 阅读。当前先看算术逻辑部分，FPGA 专用映射可暂缓。

**阅读后需要回答。** 字级运算与位级结构如何共享同一语义表示？候选实现怎样转成可提取的完整网表？

**源码情况。** baselines/logic/nextmap 是论文指向的公开仓库，含重写、ILP 提取、Yosys 集成和实验资料。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Nextmap_PLDI2026.pdf) · [原文来源](https://zsisco.net/papers/nextmap-pldi26.pdf)

### 11 e-boost 2025

**e-boost: Boosted E-Graph Extraction with Adaptive Heuristics and Exact Solving**

阅读安排：提取算法精读。研究方向：共享 DAG 的启发式提取和整数规划加速。

研究给定 e-graph 后如何较快选择成本较低的实现。方法结合并行启发式 DAG 提取、搜索空间裁剪和带初始解的整数线性规划。DAG 成本模型考虑公共子表达式复用，因此选择某个节点的价值会受到其他节点选择的影响。

**与本题的关系。** 适用于已有大量共享候选后的选择阶段，可帮助研究全局面积成本与运行时间。候选共享结构和条件语义需要由上游分析或重写提供。

**精读位置。** 先读第 II 节和第 III 节动机例子，再读第 IV 节的启发式提取、裁剪与精确求解之间的数据流。

**阅读后需要回答。** 启发式解如何引导后续求解？裁剪影响的是哪些候选及其共享机会？

**源码情况。** baselines/extraction/e-boost 包含提取器组织、ILP 生成和求解流程。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/e_boost_ICCAD2025.pdf) · [原文来源](https://www.csl.cornell.edu/~zhiruz/pdfs/eboost-iccad2025.pdf)

### 12 SparseExtraction 2024

**Fast and Optimal Extraction for Sparse Equality Graphs**

阅读安排：按算法需要精读。研究方向：基于树分解的最优 e-graph 提取。

研究稀疏 e-graph 的最优提取问题。证明一般问题的困难性后，利用树宽或路径宽受限的图结构进行动态规划，在给定成本模型下求最优解。实现使用 Rust，并在 Cranelift 产生的 e-graph 上进行实验。

**与本题的关系。** 可作为共享 DAG 选择阶段的精确算法参考。适用性与图的树宽及树分解成本有关；论文实验对象主要是编译器 e-graph。

**精读位置。** 第 2 节的合法提取和成本定义，再读第 4 节动态规划及第 5 节实验。NP 难度证明可在理解算法后补读。

**阅读后需要回答。** 哪些信息必须保留在树分解边界中，才能正确处理不同子图对同一节点的共享？

**源码情况。** baselines/extraction/SparseExtraction 是作者实验包，td-extract 中包含实际算法；同时包含 Cranelift 接入和实验脚本。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Sparse_Equality_Graphs_OOPSLA2024.pdf) · [原文来源](https://doi.org/10.5281/zenodo.13624896)

### 13 Brisk 2009 Port Assignment

**On the Complexity of the Port Assignment Problem for Binary Commutative Operators in High-Level Synthesis**

阅读安排：输入分配专题补充。研究方向：交换律运算器的端口分配。

研究共享加法器、乘法器等可交换运算器时，各次运算的两个操作数如何分配到左右输入端。分配影响两个输入 MUX 的输入集合和规模。论文讨论面积目标的组合复杂性，并提出平衡两侧 MUX 规模以改善延迟的目标。

**与本题的关系。** 与 MCTS 论文的操作数排列问题直接相接。当前重点看端口分配、MUX 成本和图模型；这篇的功能单元绑定已预先确定。

**精读位置。** 第 II 节假设、第 IV 节 Port Assignment Problem 及示例。可在阅读 MCTS 第 II 节后插入阅读。

**阅读后需要回答。** 运算器数目固定后，仅交换操作数的左右位置，为什么仍能改变 MUX 面积？

**源码情况。** 本地保存论文，未找到作者公开实现。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Brisk2009_Port_Assignment.pdf) · [原文来源](https://www.epfl.ch/labs/lap/wp-content/uploads/2018/05/BriskApr09_OnTheComplexityOfThePortAssignmentProblemForBinaryCommutativeOperators_VLSIDAT09_corrected.pdf)

### 14 E-Syn 2024

**E-Syn: E-Graph Rewriting with Technology-Aware Cost Functions for Logic Synthesis**

阅读安排：成本模型与位级方法补充。研究方向：布尔重写与技术相关成本估计。

以布尔逻辑为对象，用 e-graph 保留多种等价表达式，扩大局部 AIG 重写的搜索范围。提取阶段利用回归模型估计映射后的面积或时延，避免只依赖表达式节点数和深度。流程通过 ABC 导出方程，完成重写后再接回逻辑综合。

**与本题的关系。** 适合研究代理成本与实际 CombArea 的差异。论文第 3.2 节描述的提取对象是 AST，阅读时应与 ROVER 和 e-boost 的共享 DAG 成本模型分别比较。

**精读位置。** 第 3.1 节 Rewriting、第 3.2 节 Extraction 与回归特征，以及第 3.3 节综合流程接口。

**阅读后需要回答。** 成本模型使用了哪些结构特征？候选表达式如何交给技术映射并获得实际面积？

**源码情况。** baselines/logic/E-Syn 内 README 标题为 E-Brush。现有源码可用于理解实现，快照与论文版本的一一对应关系待核实。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/E_Syn_DAC2024.pdf) · [原文来源](https://arxiv.org/pdf/2403.14242)

### 15 ASPEN 2025

**ASPEN: LLM-Guided E-Graph Rewriting for RTL Datapath Optimization**

阅读安排：扩展方向。研究方向：LLM 引导的规则生成和综合反馈。

在 e-graph RTL 优化中引入三个角色：提出并验证新规则、选择当前设计适用的规则，以及利用实际综合 PPA 反馈调整提取成本。研究重点是减少手工编写规则与设计固定代理成本的工作，并根据具体设计组织搜索。

**与本题的关系。** 适合在已经理解重写语义、共享成本和综合评测后阅读，可参考规则生成与反馈搜索的组织方式。当前逻辑课题首先需要明确共享候选和多扇出语义，LLM 引导属于后续可研究方向。

**精读位置。** 第 III 节 ASPEN Framework 的整体循环和三个角色；关注规则正确性检查、实际综合调用及成本更新。

**阅读后需要回答。** LLM 改变的是规则、搜索决策还是提取成本？实际综合反馈在哪里进入下一轮选择？

**源码情况。** 本地保存论文，未收录完整 ASPEN 实现；论文中的 extraction-gym 链接对应提取实验平台。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/ASPEN_MLCAD2025.pdf) · [原文来源](https://www.csl.cornell.edu/~zhiruz/pdfs/aspen-mlcad2025.pdf)

### 16 McFarland 1990

**The High-Level Synthesis of Digital Systems**

阅读安排：背景查阅。研究方向：高层综合中的调度、分配和绑定。

系统介绍从行为描述产生 RTL 结构的高层综合任务，将其分解为相互关联的表示、变换、调度、资源分配与连接等问题，并讨论不同求解方法。它提供资源共享在高层综合中的历史与概念背景。

**与本题的关系。** 用于澄清 allocation、scheduling、binding 和 resource sharing 等术语。当前输入已经是 RTL，不需要先完成整套高层综合研究才能研究本题。

**精读位置。** 先看开头的设计层次与任务分解；遇到术语问题时查阅资源分配、调度和绑定相关内容。

**阅读后需要回答。** 运算发生在哪个控制步、使用哪一个功能单元、操作数如何连接，分别由什么决策决定？

**源码情况。** 本地为扫描版论文，没有配套算法仓库收录。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/McFarland1990_High_Level_Synthesis.pdf) · [原文来源](http://faculty.kfupm.edu.sa/coe/sadiq/richfiles/rich/pdf/synthesis.pdf)

### 17 Share with Care 2013

**Share with Care: A Quantitative Evaluation of Sharing Approaches in High-level Synthesis**

阅读安排：当前暂缓。研究方向：高层综合中调度与共享联合决策。

比较先调度再绑定共享资源与在调度过程中同时决定共享的两种方法。指出共享会改变连接结构和路径关系，单独的后处理绑定难以反映这些影响，并在工业高层综合设计上比较两种流程的结果。

**与本题的关系。** 研究对象包含调度和时序影响，与当前固定 RTL、只比较面积的实验范围不同。可在将来研究跨周期共享或调度协同时阅读。

**精读位置。** 先读第 II 节的问题例子，再读两种共享策略的描述和实验设置。当前不需要实现其中的调度流程。

**阅读后需要回答。** 当共享决定会影响调度可行性时，两个阶段怎样交换信息？

**源码情况。** 本地保存论文，未收录对应商业实现。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Share_with_Care_DATE2013.pdf) · [原文来源](https://past.date-conference.com/proceedings-archive/2013/PDFFILES/11.2_1.PDF)

### 18 Samuel Coward 博士论文 2025

**Equality Saturation for Circuit Synthesis and Verification**

阅读安排：按章节查阅。研究方向：ROVER、程序分析、条件优化和验证的系统论述。

系统整理数据通路优化中的等价饱和、面积优化、抽象解释、上下文条件和验证方法。相比会议或期刊论文，提供更完整的背景、推导和案例，适合作为阅读 ROVER 系列论文时的辅助资料。

**与本题的关系。** 用于补足 e-graph 与 RTL 语义的背景，核对重写、条件分析和提取之间的关系。无需从第一页连续读到最后。

**精读位置。** 第 2 章背景；第 3 章 RTL 面积优化，特别是 3.3.2 共享感知提取；第 4 章抽象解释；其余章节按对应论文需要查阅。

**阅读后需要回答。** 重写、程序分析和实现提取分别负责什么？它们怎样交换信息？

**源码情况。** 本地保存完整学位论文；研究实现的公开状态按各篇论文条目记录。

[项目内 PDF](/hpc2hdd/home/haoyuetian/datapath_logic/ref/papers/Samuel_Coward_Thesis2025.pdf) · [原文来源](https://samuelcoward.co.uk/assets/pdf/Thesis_Imperial.pdf)

## 四 源码阅读顺序

源码阅读从算法状态和数据流开始。先看输入表示、候选或规则的生成、成本计算及结果输出；构建配置和批量实验脚本可随后查阅。以下路径均对应项目中实际存在的文件。

### 1 Yosys 共享 pass 和当前基线

1. [opt_share.cc](/hpc2hdd/home/haoyuetian/datapath_logic/tools/yosys/passes/opt/opt_share.cc)：先看 help 中的共同输入、同一 MUX 等条件，再定位输出独占连接检查和合并操作。
2. [share.cc](/hpc2hdd/home/haoyuetian/datapath_logic/tools/yosys/passes/opt/share.cc)：先看 help，再看 find_cell_activation_patterns、SAT 判定及资源合并的数据流。
3. [synth.cc](/hpc2hdd/home/haoyuetian/datapath_logic/tools/yosys/techlibs/common/synth.cc)：查看两个 pass 在默认综合中的调用位置。
4. [abc.cc](/hpc2hdd/home/haoyuetian/datapath_logic/tools/yosys/passes/techmap/abc.cc) 与 [基线流程说明](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/yosys_abc/README.md)：了解优化后的结构如何交给 ABC，以及当前 CombArea 的计算来源。

opt_share 和 share 分别阅读。前者在目标 MUX 附近依据结构条件合并，后者使用 SAT 判断资源是否可以共享。ABC 的内部优化算法可在后续研究门级映射时阅读。

### 2 Resource-Sharing 研究原型

先读 [README](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/README.md) 的共享示例，接着按以下顺序阅读：

- [column.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/src/column.py)：矩阵列的表示。
- [gen_decision.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/src/gen_decision.py)：输入分配决策的入口。
- [mcts_merge.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/src/mcts/mcts_merge.py)：搜索状态、候选选择和模拟。
- [greedy_merge.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/src/baseline/greedy_merge.py) 与 [enum_merge.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/src/enumerate/enum_merge.py)：比较贪心和穷举的决策方式。
- [main.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/main.py) 与 [gen_verilog.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing/gen_verilog.py)：检查算法结果如何接入 RTL 处理与代码生成。

阅读目标是把论文中的矩阵状态、一次列组合和 MUX 连接对应起来。该仓库的身份是作者研究原型。

### 3 Nextmap

先读 [README](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/nextmap/README.md) 的 Yosys JSON 输入、重写和提取例子，再读：

- [schema.sql](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/nextmap/nextmap/schema.sql)：网表节点、线网与等价关系的数据库表示。
- [basic.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/nextmap/nextmap/rewrites/basic.py) 与 [arith.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/nextmap/nextmap/rewrites/arith.py)：模式匹配和等价候选的加入方式。
- [ilp.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/nextmap/nextmap/extracts/ilp.py)：从 extract_no_techmap 开始，看节点选择变量与连接约束。
- [Yosys 集成说明](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/nextmap/integrations/yosys/README.md)：查看系统接口。

当前优先关注逻辑重写和无专用映射的提取入口，DSP、LUT 和重定时内容按后续研究需要阅读。

### 4 e-boost

按 [main.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/e-boost/src/main.rs)、[extractor.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/e-boost/src/extractor.rs)、[ilp_gen.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/e-boost/src/ilp_gen.rs) 的顺序阅读。分别关注输入 e-graph 如何进入流程、启发式提取器如何组织，以及裁剪后的图和初始解如何形成 ILP。

这里要追踪的是给定候选集合上的选择过程；前端 RTL 解析和硬件重写由其他组件提供。

### 5 SparseExtraction

按 [egraph.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/SparseExtraction/td-extract/src/egraph.rs)、[tree_decomp.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/SparseExtraction/td-extract/src/tree_decomp.rs)、[solver.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/SparseExtraction/td-extract/src/solver.rs)、[td_solver.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/SparseExtraction/td-extract/src/td_solver.rs) 阅读。先理解图和树分解表示，再看动态规划状态、状态组合及树分解库接口。

[README](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction/SparseExtraction/README.md) 同时介绍实验包、Cranelift 接入和数据。其源码位于 td-extract。

### 6 E-Syn 目录中的实现

先读 [README](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/E-Syn/README.md)，再看 [run_beta.py](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/E-Syn/run_beta.py)、[e-rewriter 的 main.rs](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/E-Syn/e-rewriter/src/main.rs) 和 [模型文件目录](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/E-Syn/e-rewriter/src/model)。

当前快照 README 标题为 E-Brush。阅读时先了解方程转换、重写和成本模型的接口；将源码用于论文实验对照前，需要明确对应版本。

## 五 文件夹中的资料角色

| 内容 | 项目位置 | 角色与当前用途 |
|---|---|---|
| Yosys 与 ABC | [tools](/hpc2hdd/home/haoyuetian/datapath_logic/tools) | 当前实际使用的综合工具 |
| Yosys 与 ABC 基线文档 | [yosys_abc](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/yosys_abc) | 当前实验流程及指标定义 |
| Resource-Sharing | [研究原型](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/Resource-Sharing) | MCTS 共享与输入分配参考 |
| Nextmap | [公开框架](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/nextmap) | e-graph 硬件优化与映射实现 |
| E-Syn 目录 | [源码快照](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/logic/E-Syn) | 布尔重写和成本模型参考，版本对应关系待核实 |
| e-boost 与 SparseExtraction | [extraction](/hpc2hdd/home/haoyuetian/datapath_logic/baselines/extraction) | e-graph 提取算法 |
| MutexShare-Case-Set | [公开测试集](/hpc2hdd/home/haoyuetian/datapath_logic/benchmarks/rtl/MutexShare-Case-Set) | 当前题目的 RTL 和原始库等输入 |
| DatapathBench | [RTL 测试集](/hpc2hdd/home/haoyuetian/datapath_logic/benchmarks/rtl/DatapathBench) | ROVER 相关公开测试电路 |
| extraction-gym | [提取实验平台](/hpc2hdd/home/haoyuetian/datapath_logic/benchmarks/extraction/extraction-gym) | 提取器实现与 e-graph 测试数据 |
| XLS 与 CIRCT | [frameworks](/hpc2hdd/home/haoyuetian/datapath_logic/ref/frameworks) | 较大的硬件编译框架，按表示或优化机制需要查阅 |
| OpenSTA | [OpenSTA](/hpc2hdd/home/haoyuetian/datapath_logic/ref/frameworks/OpenSTA) | 静态时序分析工具，当前面积任务暂缓 |
| SiliconCompiler、LibreLane、OpenROAD Flow Scripts | [flows](/hpc2hdd/home/haoyuetian/datapath_logic/ref/flows) | 综合和设计流程的组织参考，当前研究共享算法时暂缓 |
| 专利资料 | [patents](/hpc2hdd/home/haoyuetian/datapath_logic/ref/patents) | 另外保存的技术披露，当前不排入论文精读顺序 |

研究源码目录中的实现尚未在本项目统一适配为 RTL 到 CombArea 的对照实验。当前固定基线仍为原版 Yosys 与 ABC，入口为 [run.py](/hpc2hdd/home/haoyuetian/datapath_logic/run.py)。

## 六 精读笔记建议

每篇只需先记录以下五项，便于之后比较方法：

1. 输入是什么：RTL、字级网表、门级 DAG、分支矩阵，还是已有 e-graph。
2. 允许做什么决定：合并运算器、调整操作数、移动 MUX、生成等价表达式，还是选择已有节点。
3. 等价性由什么保证：结构条件、分支条件、位宽语义、SAT，或经过验证的重写规则。
4. 优化什么成本：运算器数量、MUX 输入规模、AIG 节点数、估计 DAG 成本，或映射后的标准单元面积。
5. 多扇出怎样处理：保留所有使用者、排除候选、利用扇出做匹配，或在提取中统一计算共享节点成本。

优先完成上述信息与论文实例的对应，再阅读证明和实现细节。论文中的性能数字应连同测试电路、比较基线、映射流程和运行环境一起记录。

资料来源与下载版本记录在 [SOURCES.json](/hpc2hdd/home/haoyuetian/datapath_logic/ref/SOURCES.json)；前一轮源码搜索结果见 [源码搜索记录](/hpc2hdd/home/haoyuetian/datapath_logic/ref/metadata/logic_sharing_code_search_20261103.json)。
