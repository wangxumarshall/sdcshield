#!/usr/bin/env python3
"""sdc_hypothesis.py — 十域假设矩阵评分器 + 反事实探针建议 + 证据等级守卫
（v5 §11.2/§11.8，M4 Task 3）。stdlib only、纯函数。

矩阵：configs/sdc-excite-reproduce/hypothesis_matrix.json = v5 §11.2 十域表
机器可读版（support_signals/counter_probe/alternative 三列逐字），load_matrix()
加载；域序即 v5 表序：测试-oracle / Integer ALU / FP-SIMD-Vector / 寄存器-旁路 /
LSU-L1D / L2-LLC-一致性 / TLB-PTW / 内存控制器-DRAM / 分支-前端 / 时序-供电边际。

score_hypotheses(evidence, matrix) → 每域 {"domain", "score", "supporting",
"contradicting", "probe", "alternative"} 按 score 降序（同分稳定保持矩阵序；
score=0 域全量保留排序尾部）。评分口径：支持信号 +1、反证信号 -2（v5 §11.2
「支持信号」「典型反证」两列映射，见 _EVIDENCE_RULES 注释）。evidence 为
人工/上游管线从事件链裁定的观察 dict，键语义（消费 T2 bit_views/
classification_hint 与事件流/ledger retest）：

  cpu_clustering     失败聚集于特定嫌疑核（T2 视图6 高占比 / 台账核聚集）
                     → +1 六个 core-local 域（§11.5 嫌疑核异常更支持 core-local）；
                       -2 测试-oracle（反证「仅嫌疑核复现」）、内存控制器-DRAM
                       （其支持信号「跨核共享但非核局部」之否定）
  n1_immune          极性键（-n 1 复测必有结论，True/False 都携带证据）：
                     True（-n 1 从不出错）→ +1 L2-LLC-一致性（§11.3 仅多核对打
                     时错→缓存一致性/互联）、时序-供电边际（同供电域 aggressor
                     必需）；False（-n 1 也复现）→ -2 L2-LLC-一致性（v5 反证
                     「victim 单核私有 L1 驻留仍稳定复现」；不反证时序域——
                     边际核单线程负载阶跃亦可失效）
  multicore_only     失败需要同簇/跨核并发 → +1 L2-LLC-一致性、时序-供电边际
  healthy_core_clean 健康核/机同二进制同 seed 无错 → -2 测试-oracle
  replaced_impl_fails 替换测试实现/独立实现仍发生 → -2 测试-oracle
  sanitizer_clean    sanitizer/TSAN 干净 → -2 测试-oracle（支持信号之否定）
  sanitizer_report / uniform_across_cpus / opt_level_dependent
                     → +1 测试-oracle（v5 支持信号三支）
  workset_dependent  失败随工作集/足迹档变化 → +1 L2-LLC-一致性、TLB-PTW
                     （工作集页数）、内存控制器-DRAM（§11.3 仅大足迹/DRAM 档
                     错→内存路径）；-2 Integer ALU、寄存器-旁路（反证「随 cache
                     环境变化」「与地址/cache set 更相关」）
  integer_only       纯整数依赖链失败、FP/load-store 微测正常 → +1 Integer ALU；
                     -2 FP-SIMD-Vector、LSU-L1D
  fp_only            仅 FP/NEON/SVE 失败 → +1 FP-SIMD-Vector；-2 Integer ALU
  bit_hints          str/list，T2 classification_hint 令牌：load_path（地址/
                     cache set 固定+lane 变→存储层级候选）→ +1 LSU-L1D、
                     -2 FP-SIMD-Vector/寄存器-旁路（「仅由存储路径破坏」「与
                     地址更相关」）；execute_path（lane 固定+地址变→执行/寄存
                     通路候选）→ +1 寄存器-旁路、-2 LSU-L1D；mantissa（仅尾数
                     位翻→乘加尾数路径，§11.3）→ +1 FP-SIMD-Vector。未知令牌忽略
  pmu_anomaly        str/list，异常计数器族名（spool pmu_core/pmu_uncore 切片）：
                     load stall/L1 refill 族→LSU-L1D；L3 retry/snoop 族→L2-LLC-
                     一致性；DTLB/translation 族→TLB-PTW；frontend/branch 族→
                     分支-前端；DDRC/EDAC/NUMA 族→内存控制器-DRAM；延迟尾/频率/
                     功耗族→时序-供电边际。单串多关键词命中同一域只计一次；
                     bare True（未指明计数器族）不做域判——非域判别性证据

顶层形状错误（evidence 非 dict / matrix 非 list / actions 非 list）ValueError
（T2 同口径）；条目级畸形（矩阵非 dict/缺 domain、动作非 dict/未知 type、
未知证据键/假值）降级跳过不抛。

evidence_level(actions_taken) → "E0"-"E4"（v5 §11.8 阶梯，取动作最高级）：
  E0 raw_fact/observation（原始事实已保存）；E1 correlation（时间/空间关联）；
  E2 intervention（单变量反事实干预）；E3 cross_validation（独立 oracle/工具/
  实现）；E4 platform_confirm（厂商遥测/margining/结构注入）。空 list → "E1"
  ——有事件才有此调用，事件本身已含台账时间/空间锚定；无动作 = 仅完成常规
  取证切片的基线。

format_ranking(scored, level) → Markdown（§11.8 输出规格：候选域排名、各自
支持/反证、证据等级、替代解释、最有信息量的下一项实验）。守卫：level < E2
时输出「仅可用『相关/候选』措辞」守卫行且全文不含定位性结论表述；E2+ 解锁
定位性结论（仍须列替代解释）。

诚实边界：评分是排序启发式（±整数票）非概率量化；名次只决定「下一项实验
先做哪个」，不构成根因裁定——E2 以下一切措辞受守卫约束。

用法：python3 sdc_hypothesis.py [evidence.json]   # 缺省 stdin，打印报告
      evidence.json = {"evidence": {...}, "actions_taken": [...]}
      （或直接传 evidence dict）
"""
import json
import sys
from pathlib import Path

DEFAULT_MATRIX_PATH = (Path(__file__).resolve().parents[2] / "configs"
                       / "sdc-excite-reproduce" / "hypothesis_matrix.json")

_SUPPORT, _CONTRA = 1, -2        # v5 §11.2 两列映射权重：支持 / 反证

# 六个 core-local 域（§11.5：只有嫌疑核异常更支持 core-local 候选）
_CORE_LOCAL = ("Integer ALU", "FP-SIMD-Vector", "寄存器-旁路", "LSU-L1D",
               "TLB-PTW", "分支-前端")

# ---------------------------------------------------------------------------
# 证据键 → 域±贡献（键序即 supporting/contradicting 令牌的 canonical 序）

_EVIDENCE_RULES = {
    "cpu_clustering": {**{d: _SUPPORT for d in _CORE_LOCAL},
                       "测试-oracle": _CONTRA,        # 反证：仅嫌疑核复现
                       "内存控制器-DRAM": _CONTRA},   # 支持信号「跨核共享但非核局部」之否定
    # 极性键：True/False 分别映射（见模块 docstring）
    "n1_immune": {
        True: {"L2-LLC-一致性": _SUPPORT, "时序-供电边际": _SUPPORT},
        False: {"L2-LLC-一致性": _CONTRA},   # 反证：victim 单核私有 L1 驻留仍稳定复现
    },
    "multicore_only": {"L2-LLC-一致性": _SUPPORT,   # 需要同簇/跨核 aggressor
                       "时序-供电边际": _SUPPORT},   # 同供电域 aggressor 必需
    "healthy_core_clean": {"测试-oracle": _CONTRA},          # 健康机同二进制同 seed
    "replaced_impl_fails": {"测试-oracle": _CONTRA},         # 替换测试实现仍发生
    "sanitizer_clean": {"测试-oracle": _CONTRA},             # 「sanitizer/TSAN 报告」之否定
    "sanitizer_report": {"测试-oracle": _SUPPORT},
    "uniform_across_cpus": {"测试-oracle": _SUPPORT},        # 失败跨所有 CPU 一致
    "opt_level_dependent": {"测试-oracle": _SUPPORT},        # 特定优化级别或线程数出现
    "workset_dependent": {"L2-LLC-一致性": _SUPPORT,         # 私有 vs 共享工作集
                          "TLB-PTW": _SUPPORT,               # 工作集页数
                          "内存控制器-DRAM": _SUPPORT,       # §11.3 仅大足迹/DRAM 档错
                          "Integer ALU": _CONTRA,            # 反证：随 cache 环境变化
                          "寄存器-旁路": _CONTRA},           # 反证：与地址/cache set 更相关
    "integer_only": {"Integer ALU": _SUPPORT,                # 纯整数依赖链失败
                     "FP-SIMD-Vector": _CONTRA,              # 反证：整数同样失败
                     "LSU-L1D": _CONTRA},                    # FP/load-store 微测正常
    "fp_only": {"FP-SIMD-Vector": _SUPPORT,                 # 仅 FP/NEON/SVE
                "Integer ALU": _CONTRA},                    # 整数链干净
}
_POLAR_KEYS = {"n1_immune"}       # 显式 True/False 才计分（None/缺省不携带证据）

# bit_hints 令牌（T2 classification_hint 语义）→ 域±贡献
_BIT_HINT_RULES = {
    "load_path": {"LSU-L1D": _SUPPORT,                      # 存储层级候选
                  "FP-SIMD-Vector": _CONTRA,                # 反证：仅由存储路径破坏
                  "寄存器-旁路": _CONTRA},                   # 反证：与地址/cache set 更相关
    "execute_path": {"寄存器-旁路": _SUPPORT,               # 执行/寄存器通路候选
                     "LSU-L1D": _CONTRA},                   # 两谱系竞争（CORE179 案例）
    "mantissa": {"FP-SIMD-Vector": _SUPPORT},               # §11.3 乘加尾数路径
}

# pmu_anomaly 计数器族关键词（子串匹配，小写）→ 域
_PMU_FAMILY_RULES = (
    ("LSU-L1D", ("load_stall", "l1_refill", "l1d", "ld_stall", "load_use")),
    ("L2-LLC-一致性", ("l3_retry", "l3c", "llc", "snoop", "back_invalid")),
    ("TLB-PTW", ("dtlb", "itlb", "tlb_walk", "tlb_refill", "translation", "ptw")),
    ("分支-前端", ("frontend", "branch_miss", "mispredict", "btb",
                  "i_cache", "icache")),
    ("内存控制器-DRAM", ("ddrc", "dram", "edac", "memory_ctrl", "memory_bw",
                        "numa")),
    ("时序-供电边际", ("latency_tail", "tail_latency", "frequency", "power",
                      "thermal")),
)

# actions_taken 的 type → 证据等级（v5 §11.8）
_LEVEL_BY_TYPE = {
    "raw_fact": 0, "observation": 0,
    "correlation": 1, "temporal_correlation": 1, "spatial_correlation": 1,
    "intervention": 2,
    "cross_validation": 3,
    "platform_confirm": 4,
}
_LEVEL_DESC = {
    "E0": "原始事实已保存（观察）",
    "E1": "可重复的时间/空间关联",
    "E2": "单变量反事实干预改变失败率",
    "E3": "独立 oracle/工具/实现交叉验证",
    "E4": "厂商遥测/实验室 margining/结构注入平台确认",
}


# ---------------------------------------------------------------------------
# 矩阵加载

def load_matrix(path=None):
    """加载假设矩阵（缺省 configs/sdc-excite-reproduce/hypothesis_matrix.json，
    v5 §11.2 十域表机器可读版）→ list[dict]。文件缺失/非法 JSON 异常照抛
    （自有配置非用户数据，坏配置应响亮失败）；顶层缺 domains 数组 ValueError。"""
    src = Path(path) if path is not None else DEFAULT_MATRIX_PATH
    with open(src, encoding="utf-8") as f:
        data = json.load(f)
    if isinstance(data, list):                  # 容忍裸 domains 数组
        return data
    if not isinstance(data, dict) or not isinstance(data.get("domains"), list):
        raise ValueError(f"{src}：顶层须为 domains 数组（或裸数组）")
    return data["domains"]


# ---------------------------------------------------------------------------
# 评分

def _as_tokens(value, lower=False):
    """值 → 去重令牌列表：str → [strip]；list/tuple/set → 各非空 str 项；
    其余（含 bare True/False/None）→ []。保序去重。"""
    if isinstance(value, str):
        items = [value.strip()]
    elif isinstance(value, (list, tuple, set)):
        items = [v.strip() for v in value if isinstance(v, str) and v.strip()]
    else:
        return []
    if lower:
        items = [v.lower() for v in items]
    seen, out = set(), []
    for v in items:
        if v not in seen:
            seen.add(v)
            out.append(v)
    return out


def _evidence_tokens(evidence):
    """evidence dict → (令牌, {域: ±贡献}) 流。
    序：_EVIDENCE_RULES 键序（canonical）→ bit_hints 令牌序 → pmu_anomaly 项序
    ——与调用方 dict 键序无关，输出确定。"""
    for key, rule in _EVIDENCE_RULES.items():
        if key not in evidence:
            continue
        val = evidence[key]
        if key in _POLAR_KEYS:
            if isinstance(val, bool) and val in rule:
                yield f"{key}={val}", rule[val]
            continue
        if not val:                             # 假值键不携带证据
            continue
        yield key, rule
    for tok in _as_tokens(evidence.get("bit_hints"), lower=True):
        if tok in _BIT_HINT_RULES:
            yield f"bit_hints:{tok}", _BIT_HINT_RULES[tok]
    for item in _as_tokens(evidence.get("pmu_anomaly"), lower=True):
        sub = {dom: _SUPPORT for dom, kws in _PMU_FAMILY_RULES
               if any(kw in item for kw in kws)}
        if sub:                                 # 单串多关键词命中同一域只计一次
            yield f"pmu_anomaly:{item}", sub


def score_hypotheses(evidence, matrix):
    """evidence dict × 矩阵 → 每域评分排名（score 降序，同分保持矩阵序）。
    无匹配证据的域 score=0 全量保留；反证令牌记入 contradicting。"""
    if not isinstance(evidence, dict):
        raise ValueError("evidence 必须为 dict（证据键→观察值）；无证据请传 {}")
    if matrix is None or isinstance(matrix, (str, bytes, dict)):
        raise ValueError("matrix 必须为域 dict 的列表（load_matrix() 产物）")
    try:
        items = list(matrix)
    except TypeError:
        raise ValueError("matrix 必须为可迭代序列") from None

    entries, seen = [], set()                   # 畸形条目跳过；重名域保留首个
    for m in items:
        if not isinstance(m, dict):
            continue
        dom = m.get("domain")
        if not isinstance(dom, str) or not dom.strip() or dom in seen:
            continue
        seen.add(dom)
        entries.append(m)
    if not entries:
        return []

    score = {m["domain"]: 0 for m in entries}
    supporting = {m["domain"]: [] for m in entries}
    contradicting = {m["domain"]: [] for m in entries}
    for token, sub in _evidence_tokens(evidence):
        for dom, delta in sub.items():
            if dom not in score:                # 矩阵无此域——规则不适用
                continue
            score[dom] += delta
            (supporting if delta > 0 else contradicting)[dom].append(token)

    out = [{"domain": m["domain"],
            "score": score[m["domain"]],
            "supporting": supporting[m["domain"]],
            "contradicting": contradicting[m["domain"]],
            "probe": m.get("counter_probe") or "",
            "alternative": m.get("alternative") or ""}
           for m in entries]
    out.sort(key=lambda d: -d["score"])         # 稳定排序：同分保持矩阵序
    return out


# ---------------------------------------------------------------------------
# 证据等级（v5 §11.8）

def evidence_level(actions_taken):
    """动作记录列表 → 最高证据等级 "E0"-"E4"；空/全忽略 → "E1"
    （有事件才有此调用——事件本身已含台账时间/空间锚定）。"""
    if actions_taken is None:
        return "E1"
    if isinstance(actions_taken, (str, bytes, dict)):
        raise ValueError("actions_taken 必须为 action dict 的列表（非 str/dict）")
    try:
        items = list(actions_taken)
    except TypeError:
        raise ValueError("actions_taken 必须为可迭代序列") from None
    best = None
    for a in items:
        if not isinstance(a, dict):
            continue
        lvl = _LEVEL_BY_TYPE.get(a.get("type"))
        if lvl is not None and (best is None or lvl > best):
            best = lvl
    return f"E{best}" if best is not None else "E1"


# ---------------------------------------------------------------------------
# 报告

def _level_num(level):
    """"E2" → 2；非 E<n> 形 → None（守卫按低等级处理）。"""
    if isinstance(level, str) and len(level) >= 2 and level[0] == "E" \
            and level[1:].isdigit():
        return int(level[1:])
    return None


def format_ranking(scored, level="E1"):
    """评分排名 + 证据等级 → Markdown（§11.8 输出规格）。
    level < E2 输出「相关/候选」守卫行且全文不含定位性结论表述；
    条目缺键降级（—/空），空 ranked 不抛。"""
    rows = [e for e in (scored or []) if isinstance(e, dict)]
    label = level if isinstance(level, str) and level else "E1"
    desc = _LEVEL_DESC.get(label, "—")
    lines = []
    a = lines.append
    a("## 候选域排名（v5 §11.2 假设矩阵 × 证据评分）")
    a("")
    a(f"- 证据等级：{label}——{desc}")
    a("- 评分口径：支持信号 +1 / 反证信号 -2（v5 §11.2「支持信号」「典型反证」"
      "两列映射）；score 降序，同分保持矩阵序；score=0 域保留（无匹配证据）")
    a("")
    a("| # | 候选域 | score | 支持证据 | 反证证据 | 反事实探针（下一项实验）"
      " | 典型反证/替代解释 |")
    a("|---|---|---|---|---|---|---|")
    if rows:
        for i, e in enumerate(rows, 1):
            sc = e.get("score", 0)
            sc_s = (f"{sc:+d}" if isinstance(sc, int) and not isinstance(sc, bool)
                    else str(sc) if sc != "" else "—")
            a(f"| {i} | {e.get('domain') or '—'} | {sc_s} "
              f"| {'、'.join(e.get('supporting') or []) or '—'} "
              f"| {'、'.join(e.get('contradicting') or []) or '—'} "
              f"| {e.get('probe') or '—'} | {e.get('alternative') or '—'} |")
    else:
        a("| — | （无候选域——evidence 为空或矩阵为空） | — | — | — | — | — |")
    a("")
    if rows:
        top = rows[0]
        a(f"- 最有信息量的下一项实验（score 最高域 {top.get('domain') or '—'}）："
          f"{top.get('probe') or '—'}")
    if _level_num(label) is None or _level_num(label) < 2:
        a("- ⚠ 证据守卫：当前等级未达 E2（干预实验记录）——所有候选域仅可用"
          "「相关/候选」措辞，不得使用定位性结论表述（v5 §11.8）")
    else:
        a("- 证据守卫：已有 E2+ 干预/交叉验证记录——允许定位性结论，但仍须列"
          "替代解释与下一项实验（v5 §11.8）")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    src = open(sys.argv[1], encoding="utf-8") if len(sys.argv) > 1 else sys.stdin
    data = json.load(src)
    if isinstance(data, dict) and ("evidence" in data or "actions_taken" in data):
        ev = data.get("evidence") or {}
        actions = data.get("actions_taken") or []
    else:
        ev, actions = (data if isinstance(data, dict) else {}), []
    print(format_ranking(score_hypotheses(ev, load_matrix()),
                         evidence_level(actions)))
