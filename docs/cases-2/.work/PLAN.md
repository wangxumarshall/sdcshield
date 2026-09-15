# 计划：12 个 vmcore 微架构级 SDC 根因独立重研究（cases-2）

- 创建：2026-09-06
- 分支：research/vmcore-cases-5newdumps-0940batch
- 输出目录：`/home/sdc/wangxu/vmcore0102/gem5-fi/docs/cases-2/`
- 铁律：**不读 docs/cases、docs/hypothesis、docs/cpu、.planning 等现有诊断文档**（参考报告 vmcore-diagnosis-report-127.0.0.1-2026-09-04-123331 除外——用户点名要它的格式，且本文件已记录其结构要点，研究时不再回读）。所有结论必须来自本次真实命令输出。

## 环境事实（已实证）

- crash 8.0.4-17.oe2403sp4 + `/tmp/vmlinux-0102`（BuildID 276194e5…，_text=ffff800080000000，与 dump 的 KASLR 布局匹配）可加载完整 vmcore：case1 实测 `bt`/`sys` 成功（PARTIAL DUMP 标注但可执行命令，CPU179 kworker 崩溃 find_busiest_group 路径）。
- 仓库内 `gem5-fs/vmlinux`（BuildID fcd50d99…，_text=ffffffc008000000）与 dump **不匹配**，勿用。
- 内核：6.6.0-145.3.23.154.oe2403sp3.aarch64，192 CPU，768GB，Kunpeng-920。所有 dump 同一内核 build。
- 每个 dump 目录含 vmcore(-incomplete) + vmcore-dmesg.txt；case6 另有旧 DIAGNOSIS_REPORT.md（**不读**）。

## 报告格式（仿 vmcore-diagnosis-report-127.0.0.1-2026-09-04-123331）

目录 `docs/cases-2/vmcore-diagnosis-report-<dir-name-sanitized>/`，内含：
- `vmcore-diagnosis-report-<dir-name-sanitized>.md`（主报告）
- `dmesg_forensics.txt`（命令+输出全文）
- `algebra.py` + `algebra_out.txt`（64 位地址运算，Python3 模 2^64，禁止手算）

主报告章节：标题+副题 / 信息表 / 1 执行摘要 / 2 证据规则与方法 / 3 开机时间线 / 4 故障现象（Oops 原文、全量寄存器、Call trace、前兆 WARNING、RAS 负证据）/ 5 业务现象 / 6 诊断定位过程（P1..Pn，含 crash 实证）/ 7 逻辑链条（指令语义、闭合等式、反事实）/ 8 故障根因 / 9 启示（微架构级+芯片设计启示）/ 10 处置建议 / 附录：命令索引。
三级置信标注：【实锤】【强推】【假设】。

## 案件清单与任务分解（每案一个 subagent，互不共享结论）

- [x] T01 case 127.0.0.1-2026-08-14-19:07:04（vmcore 11.6GB 完整，crash 已验证可加载）
- [x] T02 case 127.0.0.1-2026-08-17-13:47:08（vmcore-incomplete 28.9GB）
- [x] T03 case 127.0.0.1-2026-08-24-18:03:07（vmcore 116GB + sdc_long 文件——注意独立分析该文件本身）
- [x] T04 case 127.0.0.1-2026-08-25-15:42:24（vmcore 27.6GB）
- [x] T05 case 127.0.0.1-2026-08-25-15:58:09（vmcore 9.9GB）
- [x] T06 case 127.0.0.1-2026-08-26-10:37:27（vmcore 13.9GB；目录内旧 DIAGNOSIS_REPORT.md 禁读）
- [x] T07 case 127.0.0.1-2026-08-31-00:47:32（vmcore 14.6GB）
- [x] T08 case 127.0.0.1-2026-09-03-18:25:12（vmcore 73.7GB）
- [x] T09 case 127.0.0.1-2026-09-04-09:15:42（vmcore 10.6GB）
- [x] T10 case 127.0.0.1-2026-09-04-10:27:58（vmcore 29.7GB）
- [x] T11 case 127.0.0.1-2026-09-04-11:00:00（vmcore 46.9GB）
- [x] T12 case 127.0.0.1-2026-09-04-12:33:31（vmcore-incomplete 9.2GB）

## 执行方式

每个任务一个独立 general-purpose subagent（Agent tool），prompt 内嵌完整方法论文档（不依赖 skill 可用性）。分批 4×3 并行。每 subagent 产出写入 cases-2。主会话最后做 T13 汇总比对（跨案谱系，仅基于 12 份新报告）+ 提交 + push。

## 验收标准（每案）

1. 报告含真实命令输出（dmesg 行号可复核、crash 命令与输出引文）；禁止编造。
2. 寄存器/地址代数用 Python3 模 2^64 脚本复算，脚本与输出入附件。
3. crash 侧：完整 dump 必须实际加载并取证（bt/registers/内存真值对照/vtop）；incomplete dump 如实记录加载失败/降级。
4. 微架构级根因分析 + 芯片设计启示明确成节。
5. 三级置信标注贯穿。
