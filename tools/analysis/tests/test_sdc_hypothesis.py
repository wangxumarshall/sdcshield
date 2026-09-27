"""sdc_hypothesis 单元测试（M4 Task 3）——十域假设矩阵与探针建议（v5 §11.2/§11.8）。

fixture 口径（brief Step 1 裁定，CORE179 真实画像）：
  CORE179_EV   cpu 聚集 + n1 非免疫（-n 1 也复现）+ 多核依赖 + load-path
               位提示 + 健康核干净 → LSU-L1D 居首（+2 = cpu_clustering
               + bit_hints:load_path）；L2-LLC-一致性 混合证据（multicore_only
               +1 被 n1_immune=False -2 压过）；测试-oracle 双反证 -4。

评分权重（实现裁定，v5 §11.2「支持信号/典型反证」两列映射）：支持信号
+1、反证信号 -2；n1_immune 为极性键（True/False 都携带证据）；同分保持
矩阵序；无匹配证据域 score=0 全量保留排序尾部。
"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_hypothesis as hyp


TEN_DOMAINS = ("测试-oracle", "Integer ALU", "FP-SIMD-Vector", "寄存器-旁路",
               "LSU-L1D", "L2-LLC-一致性", "TLB-PTW", "内存控制器-DRAM",
               "分支-前端", "时序-供电边际")

CORE179_EV = {"cpu_clustering": True, "n1_immune": False, "multicore_only": True,
              "bit_hints": "load_path", "healthy_core_clean": True}


# ---------------------------------------------------------------------------
# brief Step 1 两测试

def test_core179_profile_ranks_lsu_high():
    ev = {"cpu_clustering": True, "n1_immune": False, "multicore_only": True,
          "bit_hints": "load_path", "healthy_core_clean": True}
    scored = hyp.score_hypotheses(ev, hyp.load_matrix())
    top = scored[0]["domain"]
    assert top in ("LSU-L1D", "L2-LLC-一致性", "寄存器-旁路")
    assert any(d["domain"] == "测试-oracle" and d["score"] < 0 for d in scored)


def test_evidence_level_guard():
    assert hyp.evidence_level([]) == "E1"          # 只有观察
    assert hyp.evidence_level([{"type": "intervention"}]) == "E2"
    scored = hyp.score_hypotheses(CORE179_EV, hyp.load_matrix())
    out = hyp.format_ranking(scored, "E1")         # 无 E2
    assert "相关/候选" in out and "根因已定位" not in out


# ---------------------------------------------------------------------------
# 矩阵加载（v5 §11.2 十域表逐字机器可读版）

def test_load_matrix_ten_domains():
    matrix = hyp.load_matrix()
    assert [m["domain"] for m in matrix] == list(TEN_DOMAINS)   # v5 表序
    for m in matrix:
        assert isinstance(m["support_signals"], list) and m["support_signals"]
        assert all(isinstance(s, str) and s for s in m["support_signals"])
        assert isinstance(m["counter_probe"], str) and m["counter_probe"]
        assert isinstance(m["alternative"], str) and m["alternative"]
    by = {m["domain"]: m for m in matrix}
    # 逐字照 v5 §11.2 抽查（列内容不翻译不改写）
    assert by["LSU-L1D"]["support_signals"] == [
        "load/store/memcpy 聚集", "load stall/L1 refill 异常", "错误 offset/对齐相关"]
    assert "成组移除 aggressor（cpuset→offline）" in by["L2-LLC-一致性"]["counter_probe"]
    assert by["测试-oracle"]["alternative"] == "仅嫌疑核复现且替换测试实现仍发生"
    assert "延迟尾部先变化" in by["时序-供电边际"]["support_signals"]
    assert by["寄存器-旁路"]["counter_probe"] == \
        "插入独立指令/NOP、改变寄存器分配、spill/reload、交换 operand"


# ---------------------------------------------------------------------------
# CORE179 画像评分明细（反证加权 + 混合证据）

def test_core179_ranking_detail():
    scored = hyp.score_hypotheses(CORE179_EV, hyp.load_matrix())
    assert len(scored) == 10
    by = {d["domain"]: d for d in scored}
    # LSU-L1D：cpu_clustering +1 + bit_hints:load_path +1 → +2 居首
    assert scored[0]["domain"] == "LSU-L1D" and scored[0]["score"] == 2
    assert scored[0]["supporting"] == ["cpu_clustering", "bit_hints:load_path"]
    assert scored[0]["contradicting"] == []
    assert "store-forwarding" in scored[0]["probe"]      # 反事实探针（v5 逐字）
    # 混合证据：multicore_only(+1) 被 n1_immune=False(-2) 压过 → 一致性弱化
    l2 = by["L2-LLC-一致性"]
    assert l2["score"] == -1
    assert l2["supporting"] == ["multicore_only"]
    assert l2["contradicting"] == ["n1_immune=False"]
    # 测试-oracle：cpu_clustering + healthy_core_clean 双反证 → -4
    oracle = by["测试-oracle"]
    assert oracle["score"] == -4
    assert sorted(oracle["contradicting"]) == ["cpu_clustering", "healthy_core_clean"]
    # 降序不变式（同分稳定保持矩阵序）
    assert all(scored[i]["score"] >= scored[i + 1]["score"] for i in range(9))


def test_replaced_impl_contradicts_oracle():
    ev = {"replaced_impl_fails": True, "sanitizer_clean": True,
          "uniform_across_cpus": False}           # 假值键不携带证据（非极性键）
    scored = hyp.score_hypotheses(ev, hyp.load_matrix())
    oracle = next(d for d in scored if d["domain"] == "测试-oracle")
    assert oracle["score"] == -4
    assert oracle["contradicting"] == ["replaced_impl_fails", "sanitizer_clean"]
    # 其余域无匹配证据 → score=0 且全量保留
    assert len(scored) == 10
    assert all(d["score"] == 0 for d in scored if d["domain"] != "测试-oracle")


def test_no_evidence_domains_score_zero_tail():
    scored = hyp.score_hypotheses({"bit_hints": "mantissa"}, hyp.load_matrix())
    assert len(scored) == 10
    assert scored[0]["domain"] == "FP-SIMD-Vector" and scored[0]["score"] == 1
    assert scored[0]["supporting"] == ["bit_hints:mantissa"]
    # 无匹配证据的域 score=0、supporting/contradicting 空、保留排序尾部
    assert all(d["score"] == 0 and not d["supporting"] and not d["contradicting"]
               for d in scored[1:])


# ---------------------------------------------------------------------------
# 证据等级 E0-E4 阶梯（v5 §11.8）

def test_evidence_level_ladder():
    assert hyp.evidence_level([]) == "E1"               # 有事件才有此调用——基线
    assert hyp.evidence_level(None) == "E1"
    assert hyp.evidence_level([{"type": "raw_fact"}]) == "E0"
    assert hyp.evidence_level([{"type": "correlation"}]) == "E1"
    assert hyp.evidence_level([{"type": "spatial_correlation"}]) == "E1"
    assert hyp.evidence_level([{"type": "intervention"}]) == "E2"
    assert hyp.evidence_level([{"type": "cross_validation"}]) == "E3"
    assert hyp.evidence_level([{"type": "platform_confirm"}]) == "E4"
    # 最高等级胜出；未知类型/非 dict 动作忽略
    assert hyp.evidence_level([{"type": "raw_fact"},
                               {"type": "intervention"}]) == "E2"
    assert hyp.evidence_level([{"type": "noise"}, "junk", {}, None,
                               {"type": "cross_validation"}]) == "E3"


# ---------------------------------------------------------------------------
# 证据键权重（v5 §11.2 支持信号/典型反证列映射）

def test_evidence_key_weights():
    m = hyp.load_matrix()
    by = {d["domain"]: d for d in hyp.score_hypotheses({"workset_dependent": True}, m)}
    assert by["L2-LLC-一致性"]["score"] == 1
    assert by["TLB-PTW"]["score"] == 1
    assert by["内存控制器-DRAM"]["score"] == 1
    assert by["Integer ALU"]["score"] == -2        # 反证：随 cache 环境变化
    assert by["寄存器-旁路"]["score"] == -2        # 反证：与地址/cache set 更相关
    # 极性键 n1_immune：True 支持一致性/时序；False 反证一致性（v5 反证列）
    by = {d["domain"]: d for d in hyp.score_hypotheses({"n1_immune": True}, m)}
    assert by["L2-LLC-一致性"]["score"] == 1 and by["时序-供电边际"]["score"] == 1
    by = {d["domain"]: d for d in hyp.score_hypotheses({"n1_immune": False}, m)}
    assert by["L2-LLC-一致性"]["score"] == -2
    assert by["L2-LLC-一致性"]["contradicting"] == ["n1_immune=False"]
    assert by["时序-供电边际"]["score"] == 0       # 单线程复现不构成时序域反证
    # integer_only / fp_only 互斥反证
    by = {d["domain"]: d for d in hyp.score_hypotheses({"integer_only": True}, m)}
    assert by["Integer ALU"]["score"] == 1
    assert by["FP-SIMD-Vector"]["score"] == -2 and by["LSU-L1D"]["score"] == -2
    by = {d["domain"]: d for d in hyp.score_hypotheses({"fp_only": True}, m)}
    assert by["FP-SIMD-Vector"]["score"] == 1 and by["Integer ALU"]["score"] == -2


def test_bit_hints_and_pmu_anomaly_rules():
    m = hyp.load_matrix()
    # bit_hints：execute_path（T2 lane 固定+地址变）支持寄存器、反证 LSU
    scored = hyp.score_hypotheses({"bit_hints": "execute_path"}, m)
    assert scored[0]["domain"] == "寄存器-旁路" and scored[0]["score"] == 1
    by = {d["domain"]: d for d in scored}
    assert by["LSU-L1D"]["score"] == -2
    # list 多提示并存：load_path + mantissa
    scored = hyp.score_hypotheses({"bit_hints": ["load_path", "mantissa"]}, m)
    by = {d["domain"]: d for d in scored}
    assert by["LSU-L1D"]["score"] == 1
    assert by["FP-SIMD-Vector"]["score"] == -1
    assert by["FP-SIMD-Vector"]["supporting"] == ["bit_hints:mantissa"]
    assert by["FP-SIMD-Vector"]["contradicting"] == ["bit_hints:load_path"]
    # 未知 token 忽略
    assert all(d["score"] == 0
               for d in hyp.score_hypotheses({"bit_hints": "nonsense"}, m))
    # pmu_anomaly 计数器族：LSU / 一致性各 +1，同分稳定序 LSU 在前
    scored = hyp.score_hypotheses({"pmu_anomaly": ["l1_refill", "l3_retry"]}, m)
    by = {d["domain"]: d for d in scored}
    assert by["LSU-L1D"]["score"] == 1
    assert by["LSU-L1D"]["supporting"] == ["pmu_anomaly:l1_refill"]
    assert by["L2-LLC-一致性"]["score"] == 1
    assert by["L2-LLC-一致性"]["supporting"] == ["pmu_anomaly:l3_retry"]
    assert scored[0]["domain"] == "LSU-L1D"
    # 单串多关键词命中同一域只计一次
    by = {d["domain"]: d for d in
          hyp.score_hypotheses({"pmu_anomaly": "l1d_load_stall_high"}, m)}
    assert by["LSU-L1D"]["score"] == 1
    # bare True（未指明计数器族）不做域判
    assert all(d["score"] == 0
               for d in hyp.score_hypotheses({"pmu_anomaly": True}, m))


# ---------------------------------------------------------------------------
# 报告与守卫（v5 §11.8：无 E2 以上不写定位性结论）

def test_format_ranking_structure_and_e2_gate():
    scored = hyp.score_hypotheses(CORE179_EV, hyp.load_matrix())
    out = hyp.format_ranking(scored, "E1")
    assert out.startswith("## ")
    assert "证据等级：E1" in out
    for dom in TEN_DOMAINS:
        assert dom in out                         # 十域全量在场
    assert "store-forwarding" in out              # 探针列（v5 逐字）
    assert "最有信息量的下一项实验" in out
    assert "LSU-L1D" in out.split("最有信息量的下一项实验", 1)[1]
    assert "未达 E2" in out                       # 守卫行在场
    # E2+ 解锁：守卫改写，不再强制「相关/候选」措辞
    out2 = hyp.format_ranking(scored, "E3")
    assert "未达 E2" not in out2 and "E2+" in out2
    # 空 scored / 最小 entry 不抛；E0 也受守卫
    out3 = hyp.format_ranking([], "E0")
    assert "相关/候选" in out3 and "根因已定位" not in out3
    out4 = hyp.format_ranking([{"domain": "X", "score": 0}], "E1")
    assert "X" in out4 and "根因已定位" not in out4


# ---------------------------------------------------------------------------
# 降级契约与形状错误（T2 同口径：顶层形状错误 ValueError，条目级降级不抛）

def test_degradation_and_shape_errors():
    m = hyp.load_matrix()
    for bad in (None, "x", 42, ["cpu_clustering"]):
        try:
            hyp.score_hypotheses(bad, m)
            raise AssertionError(f"应 ValueError：{bad!r}")
        except ValueError:
            pass
    # 未知证据键/假值/极性键 None → 全零但十域保留
    scored = hyp.score_hypotheses(
        {"unknown_key": True, "cpu_clustering": False, "n1_immune": None}, m)
    assert len(scored) == 10 and all(d["score"] == 0 for d in scored)
    # 畸形矩阵条目跳过（非 dict/缺 domain），同名域仍按规则评分
    scored = hyp.score_hypotheses({"cpu_clustering": True},
                                  [{"domain": "LSU-L1D"}, "junk", {"nodomain": 1}])
    assert [d["domain"] for d in scored] == ["LSU-L1D"]
    assert scored[0]["score"] == 1
    try:
        hyp.score_hypotheses({}, "not-a-list")
        raise AssertionError("matrix 非 list 应 ValueError")
    except ValueError:
        pass
    try:
        hyp.evidence_level({"type": "intervention"})   # 单 dict 非 list
        raise AssertionError("actions 非 list 应 ValueError")
    except ValueError:
        pass


# ---------------------------------------------------------------------------
# CORE179 脱敏回归 fixture（M4 Task 6——真实案例锚定，docs/cases 公开形态学）
# fixture = fixtures/core179_case.json：evidence 与上方 CORE179_EV 同源同值
# （计划 T3 brief 裁定口径），另含真实案例的动作阶梯（E0-E4 全 rung）与
# 双谱系竞争注记。回归语义：「LSU/一致性/寄存器 前列稳定」= 该画像 top 域
# 稳定落于两竞争根因谱系 + 一致性域三域（trio），且本画像实际输出 LSU-L1D
# 居首；一致性/寄存器两域的证据态被钉住（混合/被反证）——对应真实案例
# 排除矩阵对 L3/互连的排除与寄存器谱系的削弱。矩阵/权重/规则漂移导致
# 该画像排名翻转即此测试失败。

CORE179_FIXTURE = os.path.join(os.path.dirname(__file__), "fixtures",
                               "core179_case.json")
CORE179_TRIO = ("LSU-L1D", "L2-LLC-一致性", "寄存器-旁路")


def _load_core179_case():
    with open(CORE179_FIXTURE, encoding="utf-8") as f:
        return json.load(f)


def test_core179_fixture_shape_and_desensitization():
    """fixture 可直接喂 sdc_hypothesis CLI（{"evidence","actions_taken"} 形状）；
    脱敏纪律：不含原始 seed/原始十六进制值/IP 形态（127.0.0.1 等）。"""
    case = _load_core179_case()
    assert set(case["evidence"]) == {"cpu_clustering", "n1_immune",
                                     "multicore_only", "bit_hints",
                                     "healthy_core_clean"}
    # 与 CORE179_EV 同源同值（计划 T3 brief 裁定口径，双处一致守卫）
    assert case["evidence"] == CORE179_EV
    assert isinstance(case["actions_taken"], list) and case["actions_taken"]
    with open(CORE179_FIXTURE, encoding="utf-8") as f:      # 上下文管理器（T5 评审口径）
        raw = f.read()
    assert "127.0.0.1" not in raw and "seed" not in case["evidence"]
    for a in case["actions_taken"]:
        assert a["type"] in hyp._LEVEL_BY_TYPE and a.get("desc")


def test_core179_fixture_ranking_stability():
    """真实案例锚定的排序回归：top 域稳定落于 LSU/一致性/寄存器 trio，
    实际输出 LSU-L1D 居首（+2 = cpu_clustering + load_path 双支持）；
    一致性混合证据、寄存器被装载形态反证、测试-oracle 双反证——三域
    证据态逐一钉住（漂移即失败）。"""
    case = _load_core179_case()
    scored = hyp.score_hypotheses(case["evidence"], hyp.load_matrix())
    assert len(scored) == 10
    assert all(scored[i]["score"] >= scored[i + 1]["score"] for i in range(9))
    # top 域 ∈ trio（两竞争谱系 + 一致性域）且稳定输出 LSU-L1D 居首
    assert scored[0]["domain"] in CORE179_TRIO
    assert scored[0]["domain"] == "LSU-L1D" and scored[0]["score"] == 2
    assert scored[0]["supporting"] == ["cpu_clustering", "bit_hints:load_path"]
    # 与 CORE179_EV 同证据 → 排序逐项一致（双 fixture 单一口径）
    assert scored == hyp.score_hypotheses(CORE179_EV, hyp.load_matrix())
    by = {d["domain"]: d for d in scored}
    # 一致性域混合证据钉住：multicore_only(+1) 被 n1_immune=False(-2) 压过
    # ——对应真实案例排除矩阵「L3/互连（同胞核零事件）」
    l2 = by["L2-LLC-一致性"]
    assert l2["score"] == -1
    assert l2["supporting"] == ["multicore_only"]
    assert l2["contradicting"] == ["n1_immune=False"]
    # 寄存器谱系被装载通路形态学反证（双谱系竞争的当方态）
    assert "bit_hints:load_path" in by["寄存器-旁路"]["contradicting"]
    # 测试-oracle 双反证——真实案例非测试工件
    assert by["测试-oracle"]["score"] < 0
    # pmu_anomaly 不设（真实案例 RAS/PMU 全程静默——fixture 如实不含该键）
    assert "pmu_anomaly" not in case["evidence"]


def test_core179_fixture_evidence_ladder_full():
    """真实案例动作阶梯 E0-E4 全 rung（观察→关联→干预→交叉→结构注入）
    → E4；E4 解锁定位性结论措辞（仍列替代解释），E1 照常受守卫。"""
    case = _load_core179_case()
    lvl = hyp.evidence_level(case["actions_taken"])
    assert lvl == "E4"                       # 结构注入（最高级胜出）
    assert {a["type"] for a in case["actions_taken"]} == \
        {"raw_fact", "spatial_correlation", "intervention",
         "cross_validation", "platform_confirm"}
    scored = hyp.score_hypotheses(case["evidence"], hyp.load_matrix())
    out4 = hyp.format_ranking(scored, lvl)
    assert "允许定位性结论" in out4 and "相关/候选" not in out4
    out1 = hyp.format_ranking(scored, "E1")
    assert "相关/候选" in out1 and "允许定位性结论" not in out1
