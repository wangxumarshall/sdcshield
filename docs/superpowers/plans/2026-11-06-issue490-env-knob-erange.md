# Issue #490: sdc_env_knob_int 的 ERANGE 死代码修复 Implementation Plan

**Goal:** `sdc_env_knob_int`(framework/sdc_stress_config.h)里 `errno = 0` 变成活代码——
`strtoll` 溢出(ERANGE)不再把饱和值 LLONG_MAX/LLONG_MIN 当"解析成功"送进 clamp 通道,
溢出输入按函数自身契约(「找到并解析**成功**返回 1」)拒绝,并告警**用户真实输入**后
回退默认/自适应通道。

**根因(Audit 判定,本机复现证实):** `:533-536` 先 `errno = 0` 再 `strtoll`,之后只查
解析形态(`end == v || *end != '\0'`)不查 errno——ERANGE 时 strtoll 返回饱和值
LLONG_MAX/LLONG_MIN,形态检查照样通过,返回 1 把用户从未输入的值送进
`sdc_env_knob_int_clamped` 的 clamp 分支。

**取证结论(2026-11-06 本机实测,23 个 9 作为输入):**

- env `SDC_EIGEN_SPARSE_N=99999999999999999999999` → strtoll 饱和 LLONG_MAX →
  clamp 到 hi=8192,越界告警打出 `n=9223372036854775807`(饱和值,非用户真实输入);
  随后 8192 维稀疏 init 在 `-t 2000` 下 hung 5 min 被 SIGQUIT,`exit: invalid`。
- 对照 `-O eigen_sparse.n=99999999999999999999999` → test_knobs.cpp:121 `strtoull`
  饱和 ULLONG_MAX → int64 回绕成 **-1** → clamp 到 lo=64,告警打出 `n=-1`
  (同为用户从未输入的值),`exit: pass`。
- 即 issue 所指「与 -O/config 通道的语义等价性在溢出输入上失真」:同一输入两通道
  **反向** clamp、都打失真值、一个 pass 一个 hung。

**修法设计(最小改动):**

- 形态检查之后补 `errno == ERANGE` 检查:命中则 `log_warning` 打**原始 env 字符串**
  (用户真实输入)后 `return 0`(按未配置处理 → 调用方回退默认/自适应)。
- 放在形态检查**之后**:溢出且带尾随垃圾的输入(如 `999…xyz`)保持今天的静默
  拒绝路径不变,行为增量收窄到恰好是缺陷路径(形态合法 + ERANGE)。
- 依据 1——函数自身契约(:511-513「返回值: 找到并解析**成功**返回 1」):ERANGE =
  值不可表示 = 解析不成功,`return 0` 是契约本义,现有代码反而是契约违约。
- 依据 2——仓库先例(issue #325,2026-11-01 计划 :397):env 通道无效值(彼处为
  strtoull 回绕)的处置即「告警后走自适应」;本修同型。
- 依据 3——框架自身 ERANGE 设计语义(sandstone_opts.cpp:400-409 ParseIntArgument):
  检查 `errno == ERANGE` 且越界错误打印**原始入参串**;本修对齐该诚实口径(日志
  必须能对上用户真实输入),拒绝而非 clamp 则跟随 env 兜底通道「无效 = 未配置」
  的既有设计。
- `-O` 通道自身的溢出行为(test_knobs.cpp strtoull 回绕成 -1)是上游 OpenDCDiag
  代码的无符号回绕事故,非设计语义,且属 x86-64 共用框架核心(rule 5 不动)——
  **不在本卡范围**,如实记录边界。
- ERANGE 常量可用性:`errno = 0` 已在用,errno/cerrno 必然传递可见,同一头文件
  定义 ERANGE,无需新增 include(构建零告警为证)。

**行为增量边界(全部受影响路径):**

- 变化:形态合法 + ERANGE(纯溢出)→ 由「饱和值进 clamp(失真告警 + 极端端点)」
  改为「告警真实值 + 回退自适应」;132 个 env knob 调用点共享本函数,全部受益。
- 不变:畸形串(`abc`/`12x`/空)仍静默 return 0;可正常表示的越界值(如 100000)
  仍走 clamp + 告警(设计行为);恰为 LLONG_MAX 的可表示输入仍 clamp(且告警值
  恰为真实输入,不再失真)。

**Global Constraints:**

- 一个逻辑单元一个 commit(修复 + 本计划 + 注释同步,先例 d6dc42e7)。
- `git commit -s`,尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`;只 push
  `fix/issue-490`,绝不 push main。
- 构建零新警告(`./scripts/build.sh`);行为验证引用真实输出;回归
  `-e eigen_sparse -t 2000 -n 1` → `exit: pass`。
- x86-64 非回归:sdc_stress_config.h 本身两 arch 共用,改动是纯增的输入校验分支,
  不含任何 arch 条件——对 x86-64 是同一缺陷的同一修复(诚实声明:x86 行为同步
  变化,变化方向 = 拒绝不可表示输入,与 ARM 一致)。
- 本机(无 SVE 的 920)原生可跑 env knob 行为,无需 gem5-fi(不涉及 HWCAP/VL/SVL
  能力面)。
