"""81 机部署打包器（deploy_81machine.sh，v5 §14.2 补丁单元 13）测试。

背景（v5 附录 B + M5 计划约束）：81 机（RCSIT TG225 B1）不可从本机直达——
交付物是"repo 快照 tarball + 入役清单"，实际入役是用户远端操作（不虚报已部署）。

打包器契约（本文件即规格）：
  生产   git archive HEAD（~204MB/5745 文件量级）→ dist/sdc-excite-reproduce-<git12>.tar.gz
         + dist/<git12>-deploy-checklist.md（离线依赖检查/入役清单——五段：
         rasdaemon 启用前置/known_faults 预填/无 cpufreq 降级/温度基线差异/巡检新路径）。
  测试   --from-tree <小树> --dist <tmp>：只替换快照来源步骤（tar 组装/deploy
         附加件/checklist 生成路径与生产同一代码）——不打真实大包（M5 约束）。
         pytest 绝不以无参跑生产模式（204MB 真包属交付验证，进 task 报告）。
  附加件 tarball 内 deploy/：
         DEPLOY-CHECKLIST.md（清单副本——包自含）、
         known_faults.tg225b1.csv（预填模板：FAN3 0rpm + SEL #0x84 两行 log_only，
         取自 repo configs/ 源文件——安装到 81 机数据根后 monitor 每周期重读=热更新）。
"""
import os
import subprocess
import tarfile

TESTS = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TESTS, "..", "..", ".."))
SCRIPT = os.path.join(REPO, "scripts", "sdc-excite-reproduce",
                      "deploy_81machine.sh")
KF_TEMPLATE = os.path.join(REPO, "configs", "sdc-excite-reproduce",
                           "known_faults.tg225b1.csv")

# 快照关键文件（tarball 内容清单必须含——install/手册/配置/采集器/分析器五类代表）
KEY_SNAPSHOT_FILES = (
    "scripts/sdc-excite-reproduce/install.sh",
    "scripts/sdc-excite-reproduce/NEW_BOARD_ONBOARDING.md",
    "scripts/sdc-excite-reproduce/sdc_monitor.sh",
    "configs/sdc-excite-reproduce/known_faults.csv",
    "tools/telemetry/sdc_collector.py",
    "tools/analysis/sdc_report.py",
)
DEPLOY_EXTRAS = ("deploy/DEPLOY-CHECKLIST.md", "deploy/known_faults.tg225b1.csv")


def _git12():
    return subprocess.run(["git", "-C", REPO, "rev-parse", "--short=12", "HEAD"],
                          capture_output=True, text=True, check=True
                          ).stdout.strip()


def _make_fixture_tree(base):
    """小树快照（--from-tree 用）：镜像 repo 关键布局，内容为标记文本。"""
    tree = base / "fixture-tree"
    for rel in KEY_SNAPSHOT_FILES + ("README.md",):
        p = tree / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(f"# fixture {rel}\n")
    return tree


def _run_packer(tree, dist, *extra):
    return subprocess.run(
        ["bash", SCRIPT, "--from-tree", str(tree), "--dist", str(dist), *extra],
        capture_output=True, text=True, timeout=120, cwd=REPO)


def _tar_names(tarball):
    """tar 成员名规范化（tar -C dir . 产 './' 前缀——归一后与 git archive 同口径）。"""
    with tarfile.open(tarball, "r:gz") as tf:
        return [n.lstrip("./") for n in tf.getnames()]


def _read_member(tarball, name):
    with tarfile.open(tarball, "r:gz") as tf:
        return tf.extractfile(name).read().decode()


def test_tarball_exists_and_content_manifest(tmp_path):
    """tarball 存在 + 内容清单含关键文件（快照布局保持 + deploy 附加件在场）+
    stdout 产物清单（tarball/entries/checklist 三要素）。"""
    dist = tmp_path / "dist"
    r = _run_packer(_make_fixture_tree(tmp_path), dist)
    assert r.returncode == 0, r.stdout + r.stderr

    g12 = _git12()
    tarball = dist / f"sdc-excite-reproduce-{g12}.tar.gz"
    assert tarball.is_file(), f"缺 tarball: {tarball}"
    assert (dist / f"{g12}-deploy-checklist.md").is_file(), "缺 checklist"

    names = _tar_names(tarball)
    for f in KEY_SNAPSHOT_FILES:
        assert f in names, f"快照缺关键文件: {f}"
    for f in DEPLOY_EXTRAS:
        assert f in names, f"缺 deploy 附加件: {f}"
    # stdout 产物清单：tarball 名 + entries 计数 + checklist 名（人读核对用）
    assert f"sdc-excite-reproduce-{g12}.tar.gz" in r.stdout
    assert f"{g12}-deploy-checklist.md" in r.stdout
    assert "entries" in r.stdout


def test_known_faults_template_two_prefilled_lines(tmp_path):
    """known_faults 模板两行预填（v5 附录 B 画像）：FAN3 0rpm + SEL #0x84，
    均 log_only；tarball 内副本与 repo 源文件逐字节一致。"""
    dist = tmp_path / "dist"
    r = _run_packer(_make_fixture_tree(tmp_path), dist)
    assert r.returncode == 0, r.stdout + r.stderr
    g12 = _git12()
    tarball = dist / f"sdc-excite-reproduce-{g12}.tar.gz"

    body = _read_member(tarball, "deploy/known_faults.tg225b1.csv")
    assert body == open(KF_TEMPLATE).read(), "tarball 副本与 repo 源不一致"

    lines = [l for l in body.splitlines()
             if l and not l.startswith("#") and not l.startswith("source,")]
    assert len(lines) == 2, f"预填应恰两行（FAN3+SEL#0x84），实得 {lines}"
    for l in lines:
        assert l.rstrip().endswith(",log_only"), f"非 log_only: {l}"
    joined = "\n".join(lines)
    assert "FAN3" in joined and "0rpm" in joined, "缺 FAN3 0rpm 行"
    assert "0x84" in joined and "Slot/Connector" in joined, "缺 SEL #0x84 行"
    # 两行须可被 monitor load_known_faults 消费：source,type,desc,action 四列
    for l in lines:
        assert len(l.split(",")) == 4, f"非四列: {l}"


def test_checklist_key_sections(tmp_path):
    """checklist 五段在场：rasdaemon 启用前置/known_faults 预填/无 cpufreq 降级/
    温度基线差异/巡检新路径 + 诚实性要素（远端执行不虚报 + 风扇联锁用户决策点）。"""
    dist = tmp_path / "dist"
    r = _run_packer(_make_fixture_tree(tmp_path), dist)
    assert r.returncode == 0, r.stdout + r.stderr
    g12 = _git12()
    text = (dist / f"{g12}-deploy-checklist.md").read_text()

    assert g12 in text, "checklist 须含包身份（git12）"
    # §1 rasdaemon 启用前置（81 机画像 inactive——缺项）
    assert "rasdaemon" in text and "systemctl enable --now rasdaemon" in text
    # §2 known_faults 预填（两行 log_only + 数据根安装路径 + 风扇联锁 as-built 注记）
    assert "known_faults" in text and "log_only" in text
    assert "FAN3" in text and "0x84" in text
    assert "sdc-excite-reproduce/known_faults.csv" in text  # 数据根安装落点
    assert "联锁" in text and "用户决策" in text  # 风扇模拟量联锁不查白名单的诚实注记
    # §3 无 cpufreq 列降级（平台固定 2.6GHz——预期行为非缺陷）
    assert "cpufreq" in text and "2.6" in text
    # §4 温度基线差异（独有传感器 + 实测基线 56→91°C）
    assert "1711 Core Temp" in text and "温度" in text
    # §5 巡检模板新路径（M5 runbook——替换过时 scripts/campaign 模板）
    assert "operations-runbook.md" in text
    # 诚实性：远端执行、不虚报已部署
    assert "远端" in text and "不虚报" in text


def test_packer_rejects_missing_from_tree(tmp_path):
    """--from-tree 指向不存在目录 → 干净报错退出，不留半成品 tarball。"""
    dist = tmp_path / "dist"
    r = _run_packer(tmp_path / "no-such-tree", dist)
    assert r.returncode != 0
    assert "no-such-tree" in (r.stdout + r.stderr)
    assert not list(dist.glob("*.tar.gz")) if dist.exists() else True
