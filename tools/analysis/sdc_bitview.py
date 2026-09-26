#!/usr/bin/env python3
"""sdc_bitview.py — mismatch 位形态与空间聚集七视图 + 候选域提示
（v5 §11.3-11.4，M4 Task 2）。stdlib only、纯函数。

输入：事件 mismatch 块列表（M0 canonical schema）：
    {"type", "byte_offset", "lane", "actual_hex", "expected_hex",
     "xor_mask_hex", "popcount", "cpu", "seed",
     "core_id", "cluster_id", "socket_id"}
降级契约：缺字段的件不进依赖视图（该视图空/受限）+ views["notes"] 注记，
绝不抛。xor_mask_hex 缺而 actual/expected 可解析时由 actual^expected 重算
（注记）；xor_mask_hex 与 actual^expected 不一致、popcount 与重算不一致，
均以重算为准并注记。

七视图（views 顶层键，§11.3 逐条对应）：
  1. offset_lane_bit  byte_offset→lane→bit 频率表：三维度件数/bit 次数 +
                      P(bit|lane) 条件概率（§11.3"频率和条件概率"；bit 位次
                      为值内绝对位 LSB=0；lane=最低差异字节，reproducer 语义）
  2. single_bit_ratio / multi_bit_ratio / burst_ratio / fixed_mask_ratio /
     random_mask_ratio  单/多 bit 与 mask 形态比例（§11.3 五形态；除单+多=1
                      外不互斥不归一；分母 bit_ratio_n = xor 可算且非零件数）
  3. hamming_distribution  actual vs expected 的 Hamming 距离分布（含
                      xor=0 矛盾件的距离 0，注记不藏）
  4. field_clustering 符号-指数-尾数聚集：type 含 float/double → IEEE754
                      位段（64 位即 sign[63]/exp[62-52]/mantissa[51-0]；
                      16/32/128 位同构布局，bfloat16 exp=8；FP8 等 8 位变体
                      不支持——注记跳过）；int → 高/低半位段。件级计段
                      （一件跨段重复计段）。
  5. lane_fixedness  lane 固定性（同 seed ≥2 件才判）：lane 固定+地址变 /
                      地址固定+lane 变 / offset 低位（64B 行内，cache set
                      代理）固定+lane 变
  6. cpu_clustering  CPU 聚集：logical/core/cluster/socket 四级计数+最大占比
  7. seed_stability  seed 内稳定性（同 seed ≥2 件：错误值/偏移是否重复）

§11.3 未覆盖两问的诚实边界（mismatch canonical 块无对应字段，不判）：
  - 输入 operand bit 与输出错误 bit 的关联（需 operand 数据）；
  - 只在第一次/长驻留后出现（需事件时间戳）。

classification_hint(views) → {"hint", "confidence": "hint", "signals"}：
  lane 固定+地址变 → 执行/寄存器通路候选；cache set/地址固定（offset 低位
  聚集）+lane 变 → 存储层级候选；皆无 → 信号不足不定域。提示非定性（v5
  §11.4 红线：候选域裁定需 E2+ 干预证据），confidence 恒为 "hint"。

format_bit_report(views, hints) → str：Markdown 报告段（§12.3 口径：
分子/分母同示、缺失单独显示不零填充）。

用法：python3 sdc_bitview.py [mismatches.json]   # 缺省 stdin，打印报告
"""
import json
import re
import sys

CACHE_LINE_BITS = 6        # 64B 行——offset 低位固定性（cache set 代理）
_EXP_BITS = {16: 5, 32: 8, 64: 11, 128: 15}     # IEEE754 binaryN 指数位宽
_LEVELS = (("logical", "cpu"), ("core", "core_id"),
           ("cluster", "cluster_id"), ("socket", "socket_id"))
_SEG_ORDER = ("sign", "exponent", "mantissa", "int_high", "int_low")


# ---------------------------------------------------------------------------
# 字段提取与归一（缺字段 → None，绝不抛）

def _hex_int(v):
    """hex 值 → int；不可解析（None/空/"null"/非 hex 串）→ None。"""
    if isinstance(v, bool):
        return None
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        s = v.strip()
        if s[:2].lower() == "0x":
            s = s[2:]
        if s and s.lower() not in ("null", "none"):
            try:
                return int(s, 16)
            except ValueError:
                return None
    return None


def _norm_seed(v):
    if isinstance(v, bool):
        return None
    if isinstance(v, int):
        return v
    if isinstance(v, str) and v.strip():
        return v.strip()
    return None


def _norm_id(v):
    """cpu/core/cluster/socket 标识：数字串→int，其余串原样，缺→None。"""
    if isinstance(v, bool):
        return None
    if isinstance(v, int):
        return v
    if isinstance(v, str):
        s = v.strip()
        if not s:
            return None
        try:
            return int(s, 10)
        except ValueError:
            return s
    return None


def _item_lane(m, xor):
    """lane：字段优先；缺则由 xor 最低差异位推导（//8 = 首个差异字节，
    reproducer 语义）；xor=0/不可算 → None。"""
    v = m.get("lane")
    if isinstance(v, bool):
        v = None
    if isinstance(v, int) and v >= 0:
        return v
    if isinstance(v, str) and v.strip().isdigit():
        return int(v)
    if xor:
        return ((xor & -xor).bit_length() - 1) // 8   # 最低差异 bit //8 = 字节 lane
    return None


def _item_offset(m):
    """byte_offset：int；容忍 reproducer "offset: [byte, lane]" 双元组形。"""
    v = m.get("byte_offset")
    if isinstance(v, bool):
        return None
    if isinstance(v, int):
        return v
    if isinstance(v, (list, tuple)) and len(v) == 2 and all(
            isinstance(x, int) and not isinstance(x, bool) for x in v):
        return v[0]
    if isinstance(v, str) and v.strip().isdigit():
        return int(v)
    return None


def _type_class(tname):
    """type 名 → (类 float/int/None, 位宽, 指数位宽)。
    float 判定 = 名含 float/double（double 即 IEEE754 二进制浮点）；位宽取
    名内数字（float64→64），float/double 裸名显式 32/64；缺数字时由调用方
    从 hex 长度推、再缺默认 64（canonical double 布局）。"""
    if not isinstance(tname, str) or not tname.strip():
        return (None, None, None)
    t = tname.strip().lower()
    is_float = "float" in t or "double" in t
    cls = "float" if is_float else ("int" if "int" in t else None)
    if cls is None:
        return (None, None, None)
    width = None
    if t == "float":
        width = 32
    elif t == "double":
        width = 64
    else:
        m = re.search(r"\d+", t)
        if m:
            width = int(m.group(0))
    exp = None
    if cls == "float" and width in _EXP_BITS:
        exp = 8 if (width == 16 and "bfloat" in t) else _EXP_BITS[width]
    return (cls, width, exp)


def _width_from_hex(v):
    """actual_hex 串长 ×4 → 已知浮点位宽（整字节数且 ∈ {16,32,64,128}）。"""
    if not isinstance(v, str):
        return None
    s = v.strip()
    if s[:2].lower() == "0x":
        s = s[2:]
    if s and len(s) % 2 == 0 and len(s) * 4 in _EXP_BITS:
        return len(s) * 4
    return None


def _extract(m):
    """单件 mismatch → 派生记录（全部缺字段安全）。"""
    rec = {"bad": not isinstance(m, dict),
           "seed": None, "cpu": None, "core_id": None, "cluster_id": None,
           "socket_id": None, "byte_offset": None, "lane": None,
           "xor": None, "xor_recomputed": False, "bits": None,
           "popcount": None, "popcount_field": None, "actual_norm": None,
           "cls": None, "width": None, "exp": None, "width_defaulted": False}
    if rec["bad"]:
        return rec
    xor = _hex_int(m.get("xor_mask_hex"))
    a, e = _hex_int(m.get("actual_hex")), _hex_int(m.get("expected_hex"))
    if xor is None and a is not None and e is not None:
        xor, rec["xor_recomputed"] = a ^ e, True
    elif xor is not None and a is not None and e is not None and a ^ e != xor:
        rec["xor_conflict"] = True          # 以 xor_mask_hex 为准，注记
    rec["xor"] = xor
    if xor is not None:
        rec["bits"] = [i for i in range(xor.bit_length()) if (xor >> i) & 1]
        rec["popcount"] = len(rec["bits"])
    pc = m.get("popcount")
    if isinstance(pc, int) and not isinstance(pc, bool) and pc >= 0:
        rec["popcount_field"] = pc
        if rec["popcount"] is None:
            rec["popcount"] = pc
    rec["lane"] = _item_lane(m, xor)
    rec["byte_offset"] = _item_offset(m)
    rec["seed"] = _norm_seed(m.get("seed"))
    for _name, src in _LEVELS:                # rec 按字段名（cpu/core_id/…）索引
        rec[src] = _norm_id(m.get(src))
    if a is not None:
        rec["actual_norm"] = a
    elif isinstance(m.get("actual_hex"), str) and m["actual_hex"].strip():
        rec["actual_norm"] = m["actual_hex"].strip().lower()
    cls, width, exp = _type_class(m.get("type"))
    if cls and width is None:
        width = _width_from_hex(m.get("actual_hex"))
    if cls and width is None:
        width, rec["width_defaulted"] = 64, True
    rec["cls"], rec["width"], rec["exp"] = cls, width, exp
    return rec


def _kv_key(k):
    """混合 int/str 键的确定性排序（int 数值序在前，str 字典序在后）。"""
    if isinstance(k, int) and not isinstance(k, bool):
        return (0, k, "")
    return (1, 0, str(k))


def _sort_count(d):
    return dict(sorted(d.items(), key=lambda kv: (-kv[1], _kv_key(kv[0]))))


# ---------------------------------------------------------------------------
# 七视图

def _view_offset_lane_bit(recs):
    """视图1：byte_offset→lane→bit 频率 + P(bit|lane) 条件概率。
    n = xor 可算件数（含 xor=0 矛盾件）；by_offset/by_lane 计件数，
    by_bit 计 bit 次数（多 bit 一件计多）。"""
    by_offset, by_lane, by_bit = {}, {}, {}
    lane_bit, triples = {}, {}
    n = bit_occ = 0
    for r in recs:
        if r["bits"] is None:
            continue
        n += 1
        if r["byte_offset"] is not None:
            by_offset[r["byte_offset"]] = by_offset.get(r["byte_offset"], 0) + 1
        if r["lane"] is not None:
            by_lane[r["lane"]] = by_lane.get(r["lane"], 0) + 1
        if not r["bits"]:
            continue
        bit_occ += len(r["bits"])
        for b in r["bits"]:
            by_bit[b] = by_bit.get(b, 0) + 1
        if r["lane"] is not None:
            for b in set(r["bits"]):
                d = lane_bit.setdefault(r["lane"], {})
                d[b] = d.get(b, 0) + 1
            if r["byte_offset"] is not None:
                for b in r["bits"]:
                    k = (r["byte_offset"], r["lane"], b)
                    triples[k] = triples.get(k, 0) + 1
    p_bl = {}
    for lane, d in lane_bit.items():
        tot = sum(d.values())
        p_bl[lane] = _sort_count({b: c / tot for b, c in d.items()})
    top = sorted(triples.items(), key=lambda kv: (-kv[1], _kv_key(kv[0][0]),
                                                  kv[0][1], kv[0][2]))[:8]
    return {"n": n, "bit_occurrences": bit_occ,
            "by_offset": _sort_count(by_offset), "by_lane": _sort_count(by_lane),
            "by_bit": _sort_count(by_bit), "p_bit_given_lane": p_bl,
            "top_triples": [(*k, c) for k, c in top]}


def _contiguous(bits):
    return len(bits) > 1 and max(bits) - min(bits) + 1 == len(bits)


def _view_bit_ratios(recs):
    """视图2：单/多 bit + burst/固定/随机 mask 五比例（同一分母：xor 可算
    且非零件数；五形态除单+多=1 外不互斥不归一）。"""
    base = [r for r in recs if r["xor"] is not None and r["xor"] != 0]
    n = len(base)
    cnt = {"single": 0, "multi": 0, "burst": 0, "fixed": 0, "random": 0}
    xor_counts = {}
    for r in base:
        xor_counts[r["xor"]] = xor_counts.get(r["xor"], 0) + 1
    for r in base:
        pc = r["popcount"]
        burst = _contiguous(r["bits"])
        fixed = xor_counts[r["xor"]] >= 2
        if pc == 1:
            cnt["single"] += 1
        elif pc > 1:
            cnt["multi"] += 1
            if burst:
                cnt["burst"] += 1
            if not burst and not fixed:
                cnt["random"] += 1
        if fixed:
            cnt["fixed"] += 1
    ratio = {k: (v / n if n else 0.0) for k, v in cnt.items()}
    return {"counts": cnt, "n": n,
            "single_bit_ratio": ratio["single"],
            "multi_bit_ratio": ratio["multi"],
            "burst_ratio": ratio["burst"],
            "fixed_mask_ratio": ratio["fixed"],
            "random_mask_ratio": ratio["random"]}


def _view_hamming(recs):
    """视图3：Hamming 距离分布（popcount 可知件：xor 重算或字段；含
    xor=0 件的距离 0）。"""
    pcs = [r["popcount"] for r in recs if r["popcount"] is not None]
    dist = {}
    for p in pcs:
        dist[p] = dist.get(p, 0) + 1
    dist = dict(sorted(dist.items()))
    mode = None
    if dist:
        mode = max(dist, key=lambda k: (dist[k], -k))
    return {"dist": dist, "n": len(pcs),
            "mean": (sum(pcs) / len(pcs)) if pcs else 0.0,
            "max": max(pcs) if pcs else None, "mode": mode}


def _view_field_clustering(recs, notes):
    """视图4：符号-指数-尾数（float IEEE754 位段）/int 高低半聚集——件级
    计段（一件跨段重复计段）。不支持宽度（如 FP8）与宽度未知默认均注记。"""
    out = {k: 0 for k in _SEG_ORDER}
    out.update({"float_n": 0, "int_n": 0, "opaque_n": 0, "dominant": None})
    unsupported = defaulted = 0
    for r in recs:
        if not r["bits"]:
            continue                            # 无 xor 或 xor=0——不进位段
        cls, width, exp = r["cls"], r["width"], r["exp"]
        if cls is None:
            out["opaque_n"] += 1
            continue
        if cls == "float" and exp is None:
            unsupported += 1
            continue
        if r["width_defaulted"]:
            defaulted += 1
        segs = set()
        for b in r["bits"]:
            if cls == "float":
                if b == width - 1:
                    segs.add("sign")
                elif b >= width - 1 - exp:
                    segs.add("exponent")
                else:
                    segs.add("mantissa")
            else:
                segs.add("int_high" if b >= (width or 64) // 2 else "int_low")
        out["float_n" if cls == "float" else "int_n"] += 1
        for s in segs:
            out[s] += 1
    if any(out[k] for k in _SEG_ORDER):
        out["dominant"] = max(_SEG_ORDER, key=lambda k: out[k])
    if unsupported:
        notes.append(f"{unsupported} 件 float 位宽不支持位段切分（如 FP8 8 位，"
                     "指数位宽无法从 type 名裁定）——跳过位段聚集")
    if defaulted:
        notes.append(f"{defaulted} 件 float/int 宽度未知——按 64 位 canonical "
                     "布局切（sign[63]/exp[62-52]/mantissa[51-0]）")
    if out["opaque_n"]:
        notes.append(f"{out['opaque_n']} 件 type 缺失或非 float/int——"
                     "不做位段聚集（byte 数据无 IEEE754/整数结构）")
    return out


def _seed_groups(recs, need):
    """按 seed 分组（仅含满足 need 的件）；seed 缺失件不入组。"""
    groups = {}
    for r in recs:
        if r["seed"] is None or not need(r):
            continue
        groups.setdefault(r["seed"], []).append(r)
    return groups


def _view_lane_fixedness(recs):
    """视图5：lane 固定性——同 seed ≥2 件（lane 与 byte_offset 俱全）才判。
    offset 低位固定 = 组内全部 byte_offset & 0x3F 同值（64B 行内同位，
    cache set 代理——mismatch 块无全量地址，低位聚集为最近似）。"""
    groups = _seed_groups(
        recs, lambda r: r["lane"] is not None and r["byte_offset"] is not None)
    detail = []
    for seed in sorted(groups, key=_kv_key):
        g = groups[seed]
        if len(g) < 2:
            continue
        lanes = sorted({r["lane"] for r in g})
        offsets = sorted({r["byte_offset"] for r in g})
        low = {o & ((1 << CACHE_LINE_BITS) - 1) for o in offsets}
        lf = len(lanes) == 1 and len(offsets) >= 2
        af = len(offsets) == 1 and len(lanes) >= 2
        olf = len(low) == 1 and len(lanes) >= 2
        detail.append({"seed": seed, "n": len(g), "lanes": lanes,
                       "offsets": offsets, "lane_fixed": lf,
                       "address_fixed": af, "offset_low_fixed": olf})
    n_lf = sum(1 for d in detail if d["lane_fixed"])
    n_af = sum(1 for d in detail if d["address_fixed"])
    n_olf = sum(1 for d in detail if d["offset_low_fixed"])
    return {"judged": bool(detail), "n_groups": len(detail),
            "lane_fixed_address_varying": n_lf > 0,
            "address_fixed_lane_varying": n_af > 0,
            "offset_low_fixed_lane_varying": n_olf > 0,
            "n_lane_fixed": n_lf, "n_address_fixed": n_af,
            "n_offset_low_fixed": n_olf, "groups": detail}


def _view_cpu_clustering(recs):
    """视图6：logical/core/cluster/socket 四级计数 + 最大占比（各级 n =
    带该字段件数——缺失单独显示，不零填充）。"""
    out = {}
    for name, field in _LEVELS:
        counts = {}
        for r in recs:
            if r[field] is not None:
                counts[r[field]] = counts.get(r[field], 0) + 1
        n = sum(counts.values())
        sc = _sort_count(counts)
        out[name] = {"n": n, "counts": sc,
                     "max_key": next(iter(sc)) if sc else None,
                     "max_share": (next(iter(sc.values())) / n) if n else 0.0}
    return out


def _view_seed_stability(recs):
    """视图7：同 seed ≥2 件（带 actual_hex）——错误值是否稳定重复、偏移
    是否稳定。整体 stable = 可判且全部可判组稳定。"""
    groups = _seed_groups(recs, lambda r: r["actual_norm"] is not None)
    detail = []
    for seed in sorted(groups, key=_kv_key):
        g = groups[seed]
        if len(g) < 2:
            continue
        distinct_actual = len({r["actual_norm"] for r in g})
        offs = [r["byte_offset"] for r in g]
        offs_known = [o for o in offs if o is not None]
        offset_stable = (len(set(offs_known)) == 1
                         if len(offs_known) == len(offs) and offs else None)
        detail.append({"seed": seed, "n": len(g),
                       "distinct_actual": distinct_actual,
                       "distinct_offset": (len(set(offs_known))
                                           if offset_stable is not None else None),
                       "stable": distinct_actual == 1,
                       "offset_stable": offset_stable})
    stable_n = sum(1 for d in detail if d["stable"])
    return {"judged": bool(detail), "n_groups": len(detail),
            "stable": bool(detail) and stable_n == len(detail),
            "stable_groups": stable_n,
            "unstable_groups": len(detail) - stable_n, "groups": detail}


# ---------------------------------------------------------------------------
# 公开 API

def bit_views(mismatches):
    """mismatch 列表（M0 canonical schema）→ 七视图 dict。
    缺字段降级（视图空/受限 + notes 注记），绝不抛；非列表输入 ValueError。"""
    if mismatches is None or isinstance(mismatches, (str, bytes, dict)):
        raise ValueError("mismatches 必须为 mismatch dict 的可迭代序列"
                         "（M0 canonical schema），非 dict 本身")
    try:
        items = list(mismatches)
    except TypeError:
        raise ValueError("mismatches 必须为可迭代序列") from None

    recs = [_extract(m) for m in items]
    n = len(recs)
    good = [r for r in recs if not r["bad"]]
    n_dict = len(good)
    notes = []
    if n == 0:
        notes.append("输入为空——所有视图为空结构（降级不抛）")
    if len(recs) != n_dict:
        notes.append(f"{n - n_dict} 件非 dict——跳过全部视图")

    n_xor = sum(1 for r in good if r["xor"] is not None)
    n_recomputed = sum(1 for r in good if r["xor_recomputed"])
    n_conflict = sum(1 for r in good if r.get("xor_conflict"))
    n_zero = sum(1 for r in good if r["xor"] == 0)
    n_pc_only = sum(1 for r in good
                    if r["xor"] is None and r["popcount"] is not None)
    n_no_bit = sum(1 for r in good
                   if r["xor"] is None and r["popcount"] is None)
    n_pc_conflict = sum(1 for r in good
                        if r["popcount"] is not None
                        and r["popcount_field"] is not None
                        and r["popcount"] != r["popcount_field"])
    if n_recomputed:
        notes.append(f"{n_recomputed} 件 xor_mask_hex 缺失/不可解析——"
                     "由 actual^expected 重算")
    if n_conflict:
        notes.append(f"{n_conflict} 件 xor_mask_hex ≠ actual^expected——"
                     "以 xor_mask_hex 为准（口径裁定）")
    if n_pc_conflict:
        notes.append(f"{n_pc_conflict} 件 popcount 字段与重算不一致——"
                     "以重算为准")
    if n_pc_only:
        notes.append(f"{n_pc_only} 件仅 popcount（xor 不可算）——只进 "
                     "Hamming 分布，不进 bit 频率/位段/比例视图")
    if n_no_bit:
        notes.append(f"{n_no_bit} 件无 xor/popcount——不进位形态视图"
                     "（空间/seed 视图按字段参与）")
    if n_zero:
        notes.append(f"{n_zero} 件 xor=0（actual==expected，矛盾件）——"
                     "计入 Hamming 距离 0，不进比例/位段分母")

    xor_recs = [r for r in good if r["xor"] is not None]
    miss_lane = sum(1 for r in xor_recs if r["lane"] is None)
    miss_off = sum(1 for r in xor_recs if r["byte_offset"] is None)
    if miss_lane:
        notes.append(f"{miss_lane} 件（xor 可算）lane 缺失且不可推导"
                     "（xor=0）——lane 维度不计")
    if miss_off:
        notes.append(f"{miss_off} 件（xor 可算）缺 byte_offset——"
                     "offset 维度不计")

    v1 = _view_offset_lane_bit(good)
    v2 = _view_bit_ratios(good)
    v3 = _view_hamming(good)
    v4 = _view_field_clustering(good, notes)
    v5 = _view_lane_fixedness(good)
    v6 = _view_cpu_clustering(good)
    v7 = _view_seed_stability(good)

    n_miss_seed = sum(1 for r in good if r["seed"] is None)
    if n_dict > 0 and not v5["judged"]:
        cause = (f"全部 {n_dict} 件缺 seed 字段" if n_miss_seed == n_dict
                 else "无 ≥2 件同 seed 的 lane/byte_offset 完整组")
        notes.append(f"lane 固定性不可判：{cause}")
    if n_dict > 0 and not v7["judged"]:
        cause = (f"全部 {n_dict} 件缺 seed 字段" if n_miss_seed == n_dict
                 else "无 ≥2 件同 seed 且带 actual_hex 的组")
        notes.append(f"seed 稳定性不可判：{cause}")
    for name, field in _LEVELS:
        if n_dict > 0 and v6[name]["n"] == 0:
            notes.append(f"CPU 聚集 {name} 级空：0/{n_dict} 件带 {field} 字段")

    return {
        "n": n,
        "counts": {"n": n, "n_dict": n_dict, "n_non_dict": n - n_dict,
                   "n_xor": n_xor, "n_xor_recomputed": n_recomputed,
                   "n_xor_zero": n_zero, "n_popcount_only": n_pc_only,
                   "n_no_bit_info": n_no_bit},
        # 视图1
        "offset_lane_bit": v1,
        # 视图2（五比例标量 + 计数/分母）
        "single_bit_ratio": v2["single_bit_ratio"],
        "multi_bit_ratio": v2["multi_bit_ratio"],
        "burst_ratio": v2["burst_ratio"],
        "fixed_mask_ratio": v2["fixed_mask_ratio"],
        "random_mask_ratio": v2["random_mask_ratio"],
        "bit_ratio_counts": v2["counts"],
        "bit_ratio_n": v2["n"],
        # 视图3-7
        "hamming_distribution": v3,
        "field_clustering": v4,
        "lane_fixedness": v5,
        "cpu_clustering": v6,
        "seed_stability": v7,
        "notes": notes,
    }


def classification_hint(views):
    """位形态视图 → 候选域提示（v5 §11.3 判读 + §11.4 红线：提示非定性）。
    两规则：lane 固定+地址变 → 执行/寄存器通路候选；cache set/地址固定
    （offset 低位聚集）+lane 变 → 存储层级候选。confidence 恒 "hint"。"""
    views = views or {}
    lf = views.get("lane_fixedness") or {}
    exec_sig = bool(lf.get("lane_fixed_address_varying"))
    mem_sig = bool(lf.get("address_fixed_lane_varying")
                   or lf.get("offset_low_fixed_lane_varying"))
    signals = {
        "lane_fixed_address_varying": exec_sig,
        "address_fixed_lane_varying": bool(lf.get("address_fixed_lane_varying")),
        "offset_low_fixed_lane_varying": bool(
            lf.get("offset_low_fixed_lane_varying")),
        "judged_groups": lf.get("n_groups", 0),
        "dominant_field": (views.get("field_clustering") or {}).get("dominant"),
    }
    parts = []
    if exec_sig:
        parts.append(f"固定 lane+地址变化（{lf.get('n_lane_fixed', 0)} 组同 "
                     f"seed）→ 执行/寄存器通路候选")
    if mem_sig:
        parts.append("固定 cache set/地址（offset 低位聚集）+lane 变化 → "
                     "存储层级候选")
    if parts:
        hint = "；".join(parts) + ("。两信号并存——竞争候选域，需分探针裁定"
                                   if exec_sig and mem_sig else "")
        hint += ("。提示非定性（confidence: hint）——候选域裁定需反事实探针"
                 "（v5 §11.2 矩阵 / §11.4 红线：E2+ 干预证据）")
    else:
        hint = ("位形态信号不足——无 ≥2 件同 seed 可判组或判据均不成立，"
                "不定候选域；补充同 seed 复测样本后再判（提示非定性，"
                "confidence: hint）")
    return {"hint": hint, "confidence": "hint", "signals": signals}


# ---------------------------------------------------------------------------
# 报告

def _pct(x):
    return f"{x * 100:.1f}%"


def _fmt_map(d, limit=12):
    if not d:
        return "—"
    items = list(d.items())[:limit]
    s = ", ".join(f"{k}={v}" for k, v in items)
    if len(d) > limit:
        s += f", …（共 {len(d)} 键）"
    return s


def _num(x, fmt="{:g}"):
    if isinstance(x, bool):
        return str(x)
    return fmt.format(x) if isinstance(x, (int, float)) else "—"


def format_bit_report(views, hints):
    """七视图 + 候选域提示 → Markdown 报告段（§12.3：分子/分母同示、
    缺失单独显示不零填充、确定性排序）。"""
    views = views or {}
    hints = hints or {}
    c = views.get("counts", {})
    lf = views.get("lane_fixedness", {})
    ss = views.get("seed_stability", {})
    brc = views.get("bit_ratio_counts", {})
    n2 = views.get("bit_ratio_n", 0)
    lines = []
    a = lines.append
    a("## 位形态与空间聚集（v5 §11.3）")
    a("")
    a(f"- 样本：{views.get('n', 0)} 件 mismatch；xor 可算 {c.get('n_xor', 0)}"
      f"（重算 {c.get('n_xor_recomputed', 0)}、xor=0 矛盾件 "
      f"{c.get('n_xor_zero', 0)}）；仅 popcount {c.get('n_popcount_only', 0)}"
      f"；无位信息 {c.get('n_no_bit_info', 0)}")
    a("- 口径：§11.3 的「输入 operand bit 关联」「首次/长驻留出现」两问需 "
      "operand/时间戳字段——mismatch canonical 块不含，本报告不判")
    notes = views.get("notes", [])
    if notes:
        a("")
        a("注记（降级/口径）：")
        for nt in notes:
            a(f"- {nt}")

    v1 = views.get("offset_lane_bit", {})
    a("")
    a(f"### 1. offset→lane→bit 频率（件数 {v1.get('n', 0)}，"
      f"bit 次数 {v1.get('bit_occurrences', 0)}）")
    a(f"- byte_offset 件数：{_fmt_map(v1.get('by_offset', {}))}")
    a(f"- lane 件数：{_fmt_map(v1.get('by_lane', {}))}")
    a(f"- bit 次数（值内绝对位，LSB=0）：{_fmt_map(v1.get('by_bit', {}))}")
    pbl = v1.get("p_bit_given_lane", {})
    if pbl:
        segs = "; ".join(
            f"lane {l}: " + ", ".join(f"{b}={p:.3f}" for b, p in d.items())
            for l, d in sorted(pbl.items(), key=lambda kv: _kv_key(kv[0])))
        a(f"- P(bit|lane)：{segs}")

    a("")
    a("### 2. 单/多 bit 与 mask 形态比例")
    a(f"- 单 bit {brc.get('single', 0)}/{n2}"
      f"（{_pct(views.get('single_bit_ratio', 0.0))}）；"
      f"多 bit {brc.get('multi', 0)}/{n2}"
      f"（{_pct(views.get('multi_bit_ratio', 0.0))}）")
    a(f"- 连续 burst {brc.get('burst', 0)}/{n2}"
      f"（{_pct(views.get('burst_ratio', 0.0))}）；"
      f"固定 mask（同 xor ≥2 件）{brc.get('fixed', 0)}/{n2}"
      f"（{_pct(views.get('fixed_mask_ratio', 0.0))}）；"
      f"随机 mask {brc.get('random', 0)}/{n2}"
      f"（{_pct(views.get('random_mask_ratio', 0.0))}）")
    a("- 口径：分母 = xor 可算且非零件数；五比例除「单+多=1」外不互斥不归一")

    h = views.get("hamming_distribution", {})
    a("")
    a(f"### 3. Hamming 距离分布（actual vs expected，n={h.get('n', 0)}）")
    a(f"- 分布 {{距离: 件数}}：{_fmt_map(h.get('dist', {}))}")
    a(f"- mean={_num(h.get('mean'), '{:.3g}')}；max={_num(h.get('max'))}；"
      f"mode={_num(h.get('mode'))}")

    fc = views.get("field_clustering", {})
    a("")
    a("### 4. 符号-指数-尾数聚集（float 按 IEEE754 位段；int 按高/低半位段）")
    a(f"- float {fc.get('float_n', 0)} 件：sign {fc.get('sign', 0)}、"
      f"exponent {fc.get('exponent', 0)}、mantissa {fc.get('mantissa', 0)}"
      "（一件跨段重复计段）")
    a(f"- int {fc.get('int_n', 0)} 件：高半 {fc.get('int_high', 0)}、"
      f"低半 {fc.get('int_low', 0)}")
    if fc.get("opaque_n"):
        a(f"- {fc['opaque_n']} 件 type 非 float/int——不切位段")
    a(f"- 主导段：{fc.get('dominant') or '—'}")

    a("")
    a("### 5. lane 固定性（同 seed ≥2 件才判）")
    if lf.get("judged"):
        a(f"- 可判组 {lf.get('n_groups', 0)}：lane 固定+地址变 "
          f"{lf.get('n_lane_fixed', 0)} 组；地址固定+lane 变 "
          f"{lf.get('n_address_fixed', 0)} 组；offset 低位固定+lane 变 "
          f"{lf.get('n_offset_low_fixed', 0)} 组（cache set 代理：64B 行内同位）")
        for g in lf.get("groups", []):
            a(f"  - seed={g['seed']}: n={g['n']}, lanes={g['lanes']}, "
              f"offsets={g['offsets']}")
    else:
        a("- 可判组 0——不定（无 ≥2 件同 seed 的 lane/byte_offset 完整组）")

    cc = views.get("cpu_clustering", {})
    a("")
    a("### 6. CPU 聚集（logical/core/cluster/socket 四级）")
    for name in ("logical", "core", "cluster", "socket"):
        d = cc.get(name, {})
        a(f"- {name}: n={d.get('n', 0)}, max={d.get('max_key')}"
          f"（{_pct(d.get('max_share', 0.0))}），"
          f"分布：{_fmt_map(d.get('counts', {}))}")

    a("")
    a("### 7. seed 内稳定性（同 seed ≥2 件才判）")
    if ss.get("judged"):
        a(f"- stable={ss.get('stable')}（稳定组 "
          f"{ss.get('stable_groups', 0)}/{ss.get('n_groups', 0)}）")
        for g in ss.get("groups", []):
            a(f"  - seed={g['seed']}: n={g['n']}, "
              f"distinct_actual={g['distinct_actual']}, "
              f"distinct_offset={_num(g.get('distinct_offset'))}, "
              f"stable={g['stable']}, "
              f"offset_stable={_num(g.get('offset_stable'))}")
    else:
        a("- 可判组 0——不定（无 ≥2 件同 seed 且带 actual_hex 的组）")

    a("")
    a("### 候选域提示（confidence: hint——提示非定性，v5 §11.4 红线）")
    a(f"- {hints.get('hint', '—')}")
    a(f"- confidence: {hints.get('confidence', '—')}")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    src = open(sys.argv[1], encoding="utf-8") if len(sys.argv) > 1 else sys.stdin
    data = json.load(src)
    if isinstance(data, dict):                 # 容忍单件包一层
        data = [data]
    v = bit_views(data)
    print(format_bit_report(v, classification_hint(v)))
