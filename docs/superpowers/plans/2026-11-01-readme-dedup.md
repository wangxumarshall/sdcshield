# README 去冗余重构计划（言简意赅 + AI 味保留）

**Goal:** README 565 行 → 约 400 行：删除重复叙述、压缩巨型段落；唯一事实保留 canonical 一处；前置「质量保障：Agent 全流程自动化」保留并精简（AI 味核心）；修正用户 cc94232d 改标题后的内部引用不一致。

**方法:** 整文件重写（Write）+ git diff 逐块审查。禁止动外部引用的章节名。

## 硬约束（外部引用，章节名不可改）

grep 实测（docs/ .claude/ .trae/ CLAUDE.md AGENTS.md）：`测试用例与检测能力`×4、`延伸`×2、`质量保障与 CI 基础设施`×1；用户新标题 `一行命令检测CPU SDC`（cc94232d）；前置 `质量保障：Agent 全流程自动化` + PNG 引用必须保留。

## 事实归置表（删重不丢实——每条事实的 canonical 落点）

| 重复事实 | 现状出现处 | canonical 落点 | 其余位置处置 |
|---|---|---|---|
| sched_storm×4 / issue #374 PROD→BETA | L64/L138/L370/L378/L424 | 质量分级表 footnote（L424） | 各处删或一句话+指针 |
| 用例计数链 490→486、297→299 | L378/L385/L417-424 | 质量分级表 footnote | spec 策略节留 headline（486/496/500、299）+指针 |
| 源码全量 487+10+4=501 | L385/质量分级表 | 质量分级表 | L385 段删除（表已承载） |
| 旧二进制=eigen 修复前+旧白名单 | L77/L140/L152 | L140 blockquote（预构建节） | L77/L152 注释删细节 |
| 旧标题「检测鲲鹏 CPU SDC」内部引用 | L157 | 统一为新标题 | L157 改 |
| SEVI >92% FMA | L52/L275 | P1 行（L52） | vendored 节删引文 |
| vendored 缺席优雅降级机制 | L184/L275 | 从源码构建 blockquote（L184） | vendored 节删 |
| vendored 首次构建命令块 | L165-178/L287-293 | 从源码构建 | vendored 节改指针，保留 8806/8806 终验命令 |
| openblas/gemm knob 语义 | L295-297/L402/L513-518 | Test knob 总表（L513） | prose 压成两行；检测域表 cell 只留测试名+一句 |
| OpenSSL 构建命令 | L263-271/L165-176 | 从源码构建 | OpenSSL 节删命令块，留表格+两句 |
| -n 1 回归口径 | L154(过时理由)/L474 | 运行测试 L474 | L154 注释改指针（旧理由 flakiness 已过时） |
| 五 bot/task-loop/lts 组成 | 前置 QA 节/L242/L246/L250 | 前置=摘要；细节节=唯一事实 | 前置每层压 1-2 行；细节节删与前置重复的组成描述 |
| eigen SVD sizing/AR 表细节 | L87 | CLAUDE.md 平台怪癖（已有） | README 判读段压 60%，留结论级 |
| Eigen SVD 实现细节 | L541 | 保留但压缩 50%（逐档 payload/函数指针/#410 scatter 修复/gem5 四档/证据链指针留；spike 确证等删） | — |
| smi_count 论证细节 | L441 | 保留结论（CNTVCT/>100µs/永不失败/噪声地图定位/EL0VCTEN SKIP/--inject-idle 无效/MSR 0x34 对比）；删 CFS/steal/PMU 枚举细节 | 压 50% |
| 22.03/20.03 容器适配史 | L222 | 压 3 行（sed 史一句带过） | — |
| 多版本一键 | L228 | 压 4 行（15×PASS 硬闸/main 拒跑/--skip-base/指针留） | — |
| 检测域表巨型 cell | L409/410/412 | 测试名全保留，方法学 essay 压 50% | — |
| excite 要点/战役模式依据/模式序 | L107/L357/L322-371 | 压缩；模式序整理为 0,A,B,C,D,E,F | — |
| memcpy_rewr 数学细节 | L463-465 | 压缩（公式保留，推导注释删） | — |

## 执行步骤

1. `git checkout -b docs/readme-dedup`（自最新 main = cc94232d）
2. Write 重写 README.md（上表归置 + AI 味：导言加一句 agent 驱动开发定位）
3. 验证：python 链接/路径校验 `BROKEN: none`；4 个外部引用章节名 grep 命中；`git diff --stat` 仅 README
4. 回归 `-e zstd19 -t 2000 -n 1` → pass；build 无影响（docs-only）
5. `git -c user.name=wangxu commit -s`；push；PR（auto-pr 自动开）；用户指令后 merge
