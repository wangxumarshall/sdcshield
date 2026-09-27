"""sdc_bitview 单元测试（M4 Task 2）——位形态与空间聚集七视图（v5 §11.3）。

fixture 口径（brief Step 1 裁定）：
  MISMATCHES     mantissa 位翻 3 件 + exponent 1 件（bit 55）+ 多 bit 1 件
                 （exp 下边界 52 + mantissa 10）——field_clustering/比例视图
                 主 fixture；seed 互异（固定性/稳定性视图不可判路径）；
  LANE_FIXED     固定 lane（5）+ 地址变（0/64/128），同 seed 42——执行通路提示；
  MEM_FIXED      offset 低位固定（64/128/192，均 &0x3F==0，cache set 代理）
                 + lane 变（1/2/3），同 seed 99——存储层级提示；
  SAME_SEED_SET  同 seed 7 稳定错误值（同 xor 0x400 3 件）——seed 稳定性 +
                 固定 mask 比例 = 1.0；
  INT_SET        uint64_t 高半 2 件（60；63+48）+ 低半 2 件（0-7 burst；5）
                 ——int 高/低聚集 + burst/随机 mask 形态。

降级契约（M0 canonical schema 缺字段）：依赖视图空/受限 + notes 注记，不抛。
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_bitview as bv


def _flip(expected_hex, *bits, **kw):
    """canonical mismatch 构造器：expected + 翻转位集 → 自洽的 actual/
    xor_mask/popcount/lane（lane = 最低差异位//8，reproducer 首个差异字节
    语义）。"""
    mask = 0
    for b in bits:
        mask |= 1 << b
    exp = int(expected_hex, 16)
    d = {"type": "double", "byte_offset": 0, "lane": min(bits) // 8,
         "actual_hex": hex(exp ^ mask), "expected_hex": hex(exp),
         "xor_mask_hex": hex(mask), "popcount": len(bits),
         "cpu": 179, "seed": 1, "core_id": 4, "cluster_id": 138,
         "socket_id": 0}
    d.update(kw)
    return d


MISMATCHES = [
    _flip("0x3ff0000000000000", 10, byte_offset=0, seed=101),
    _flip("0x4008000000000000", 20, byte_offset=8, seed=102),
    _flip("0x3ff0000000000000", 3, byte_offset=16, seed=103),
    _flip("0x3ff0000000000000", 55, byte_offset=24, seed=104,
          cpu=66, core_id=9, cluster_id=654, socket_id=1),
    _flip("0x3ff0000000000000", 52, 10, byte_offset=32, seed=105,
          cpu=66, core_id=9, cluster_id=654, socket_id=1),
]

LANE_FIXED = [
    _flip("0x3ff0000000000000", 40, byte_offset=0, seed=42),
    _flip("0x3ff0000000000000", 43, byte_offset=64, seed=42),
    _flip("0x3ff0000000000000", 46, byte_offset=128, seed=42),
]

MEM_FIXED = [
    _flip("0x3ff0000000000000", 10, byte_offset=64, seed=99),
    _flip("0x3ff0000000000000", 20, byte_offset=128, seed=99),
    _flip("0x3ff0000000000000", 30, byte_offset=192, seed=99),
]

SAME_SEED_SET = [
    _flip("0x3ff0000000000000", 10, byte_offset=0, seed=7, cpu=10),
    _flip("0x3ff0000000000000", 10, byte_offset=8, seed=7, cpu=11),
    _flip("0x3ff0000000000000", 10, byte_offset=16, seed=7, cpu=12),
]

INT_SET = [
    _flip("0x0", 60, type="uint64_t"),
    _flip("0x0", 63, 48, type="uint64_t"),
    _flip("0x0", *range(8), type="uint64_t"),
    _flip("0x0", 5, type="uint64_t"),
]

PARTIAL = [
    {"type": "double", "byte_offset": 0,
     "actual_hex": "0x3ff0000000000400", "expected_hex": "0x3ff0000000000000",
     "popcount": 1},                                   # xor 缺 → actual^expected 重算
    {"type": "zstd_block", "byte_offset": 8, "popcount": 3},   # 仅 popcount
    {"byte_offset": 16},                               # 无位信息
]


# ---------------------------------------------------------------------------
# brief Step 1 三测试（逐字）

def test_views_mantissa_clustering():
    views = bv.bit_views(MISMATCHES)          # fixture 中 3 件 mantissa 位
    assert views["field_clustering"]["mantissa"] >= 3
    assert views["multi_bit_ratio"] < 0.5


def test_lane_fixedness_hint():
    # 固定 lane+地址变 → 执行通路提示
    h = bv.classification_hint(bv.bit_views(LANE_FIXED))
    assert "执行" in h["hint"] and h["confidence"] == "hint"


def test_seed_stability_view():
    views = bv.bit_views(SAME_SEED_SET)       # 同 seed 稳定错误值
    assert views["seed_stability"]["stable"] is True


# ---------------------------------------------------------------------------
# 七视图逐项

def test_offset_lane_bit_frequency():
    """视图1：三维度计数（件数/bit 次数）+ P(bit|lane) 条件概率归一。"""
    v = bv.bit_views(MISMATCHES)["offset_lane_bit"]
    assert v["n"] == 5 and v["bit_occurrences"] == 6
    assert v["by_offset"] == {0: 1, 8: 1, 16: 1, 24: 1, 32: 1}
    assert v["by_lane"] == {0: 1, 1: 2, 2: 1, 6: 1}
    assert v["by_bit"] == {3: 1, 10: 2, 20: 1, 52: 1, 55: 1}
    assert abs(v["p_bit_given_lane"][1][10] - 2 / 3) < 1e-12
    assert abs(v["p_bit_given_lane"][1][52] - 1 / 3) < 1e-12
    for d in v["p_bit_given_lane"].values():
        assert abs(sum(d.values()) - 1.0) < 1e-12


def test_bit_ratio_details():
    """视图2：单+多=1；burst/固定/随机 mask 形态（§11.3 五形态不互斥）。"""
    v1 = bv.bit_views(MISMATCHES)
    assert v1["bit_ratio_n"] == 5
    assert abs(v1["single_bit_ratio"] - 0.8) < 1e-12
    assert abs(v1["multi_bit_ratio"] - 0.2) < 1e-12
    assert abs(v1["single_bit_ratio"] + v1["multi_bit_ratio"] - 1.0) < 1e-12
    v2 = bv.bit_views(INT_SET)                # 多 bit 2/4：63+48 非连续、0-7 burst
    assert v2["bit_ratio_n"] == 4
    assert abs(v2["burst_ratio"] - 0.25) < 1e-12
    assert abs(v2["fixed_mask_ratio"] - 0.0) < 1e-12
    assert abs(v2["random_mask_ratio"] - 0.25) < 1e-12
    v3 = bv.bit_views(SAME_SEED_SET)          # 同 xor 3 件 → 固定 mask 满比
    assert abs(v3["fixed_mask_ratio"] - 1.0) < 1e-12


def test_hamming_distribution():
    """视图3：popcount 分布/均值/最大/众数。"""
    h = bv.bit_views(MISMATCHES)["hamming_distribution"]
    assert h["dist"] == {1: 4, 2: 1} and h["n"] == 5
    assert h["max"] == 2 and h["mode"] == 1
    assert abs(h["mean"] - 1.2) < 1e-12


def test_int_high_low_clustering():
    """视图4 int 路径：uint64_t 高半（≥bit32）2 件、低半 2 件。"""
    fc = bv.bit_views(INT_SET)["field_clustering"]
    assert fc["int_high"] == 2 and fc["int_low"] == 2 and fc["int_n"] == 4
    assert fc["mantissa"] == 0 and fc["sign"] == 0 and fc["exponent"] == 0
    assert fc["dominant"] in ("int_high", "int_low")


def test_lane_fixedness_view():
    """视图5：固定 lane+地址变判真；lane 各异（MEM_FIXED）不触发该向。"""
    lf = bv.bit_views(LANE_FIXED)["lane_fixedness"]
    assert lf["judged"] is True and lf["n_groups"] == 1
    assert lf["lane_fixed_address_varying"] is True
    assert lf["address_fixed_lane_varying"] is False
    g = lf["groups"][0]
    assert g["seed"] == 42 and g["n"] == 3
    assert g["lanes"] == [5] and g["offsets"] == [0, 64, 128]


def test_cpu_clustering_four_levels():
    """视图6：logical/core/cluster/socket 四级计数+最大占比。"""
    cc = bv.bit_views(MISMATCHES)["cpu_clustering"]
    assert cc["logical"]["counts"] == {179: 3, 66: 2}
    assert cc["logical"]["max_key"] == 179
    assert abs(cc["logical"]["max_share"] - 0.6) < 1e-12
    assert cc["core"]["counts"] == {4: 3, 9: 2}
    assert cc["cluster"]["counts"] == {138: 3, 654: 2}
    assert cc["socket"]["counts"] == {0: 3, 1: 2}
    assert abs(cc["socket"]["max_share"] - 0.6) < 1e-12


def test_seed_stability_unstable_detail():
    """视图7 不稳定路径：LANE_FIXED 同 seed 但错误值各异 → stable=False。"""
    v = bv.bit_views(LANE_FIXED)["seed_stability"]
    assert v["judged"] is True and v["stable"] is False
    g = v["groups"][0]
    assert g["n"] == 3 and g["distinct_actual"] == 3


# ---------------------------------------------------------------------------
# 候选域提示（v5 §11.4 红线：提示非定性）

def test_classification_hint_storage():
    h = bv.classification_hint(bv.bit_views(MEM_FIXED))
    assert "存储" in h["hint"] and h["confidence"] == "hint"
    assert "执行" not in h["hint"]            # lane 各异——执行向不触发


def test_classification_hint_insufficient():
    h = bv.classification_hint(bv.bit_views(MISMATCHES))   # seed 互异 → 无可判组
    assert h["confidence"] == "hint" and "不足" in h["hint"]
    assert h["signals"]["judged_groups"] == 0


# ---------------------------------------------------------------------------
# 降级契约（缺字段不抛 + 注记）与边界

def test_degradation_missing_fields():
    views = bv.bit_views(PARTIAL)             # 不抛即降级契约
    assert views["n"] == 3
    # item1 xor 由 actual^expected 重算；item2 仅 popcount 进 Hamming
    assert views["offset_lane_bit"]["by_bit"] == {10: 1}
    assert views["offset_lane_bit"]["by_lane"] == {1: 1}   # lane 缺→由 xor 最低位//8 推导
    assert views["hamming_distribution"]["dist"] == {1: 1, 3: 1}
    assert views["bit_ratio_n"] == 1 and views["multi_bit_ratio"] == 0.0
    assert views["seed_stability"]["judged"] is False
    assert views["lane_fixedness"]["judged"] is False
    assert views["cpu_clustering"]["logical"]["n"] == 0
    joined = "\n".join(views["notes"])
    for kw in ("seed", "重算", "popcount", "cpu"):      # 降级注记四要素
        assert kw in joined


def test_empty_input():
    views = bv.bit_views([])
    assert views["n"] == 0
    for k in ("offset_lane_bit", "single_bit_ratio", "multi_bit_ratio",
              "burst_ratio", "fixed_mask_ratio", "random_mask_ratio",
              "hamming_distribution", "field_clustering", "lane_fixedness",
              "cpu_clustering", "seed_stability", "notes"):
        assert k in views
    assert views["seed_stability"]["stable"] is False
    h = bv.classification_hint(views)
    assert h["confidence"] == "hint" and "不足" in h["hint"]


def test_format_bit_report():
    views = bv.bit_views(LANE_FIXED)
    rep = bv.format_bit_report(views, bv.classification_hint(views))
    assert rep.startswith("## ")
    for sec in ("offset→lane→bit", "单/多 bit", "Hamming", "符号-指数-尾数",
                "lane 固定性", "CPU 聚集", "seed 内稳定性", "候选域提示"):
        assert sec in rep
    assert "执行/寄存器通路候选" in rep and "hint" in rep
    assert "样本：3 件" in rep                  # §12.3：样本量显式同示
