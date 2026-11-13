#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""task_loop_nodepool.py — 跨机卸载节点池(spec 2026-11-13-nodepool-offload)。

nightly pipeline 的远程执行面: 板注册表 + SSH 单点运行器 + 健康探测 +
pick_node 择优 + 远程五操作(spawn/探活/日志尾/击杀/worktree)。本机编排器
(task_loop_pipeline/task_loop) 经懒 import 使用本模块; nodepool 不反向
import 它们(锁死依赖方向: tl → tp → np)。

零第三方: SSH 一律系统 ssh/scp 子进程, ConnectTimeout=8(探测语义:
连不通=该节点本轮不可选,绝不长时间挂住 cycle)。凭据走 setup 分发的密钥
免密(nodepool_setup.sh), 密码/token 不进任何 argv 与代码。

安全阀: env PIPELINE_NODEPOOL=0 → pick_node 只返回本机(纯本机旧行为);
PIPELINE_FORCE_NODE=<host_id> → 强制单节点(冒烟/测试用)。
"""
import subprocess

# 板注册表(spec §3.1; 凭据不入库——连接经 setup 分发的 ~/.ssh/authorized_keys)
# 顺序即 pick_node 的注册表序(同分时先 0102)。0104 水银核永久排除;
# 0202/0203 异网隧道板排除(spec 决策表 #3)。
BOARD_NODES = [
    {"host_id": "0102", "ssh": "sdc@172.168.160.42:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 192, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
    {"host_id": "0101", "ssh": "sdc@172.168.177.97:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 126, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
    {"host_id": "0201", "ssh": "sdc@172.168.178.81:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 96, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
]
LOCAL_NODE = {"host_id": "local", "ssh": None,
              "clone_dir": "/home/sdc/wangxu/operator/sdcshield-autoci",
              "cores": None, "slots": {"issue_fix": 2, "pr_review": 2,
                                       "pr_repair": 2}}
REMOTE_STATE_DIR = "~/sdcshield-autoci-state"   # 板上日志/prompt/agent.env 目录
SSH_TIMEOUT_DEFAULT = 45


class NodeError(RuntimeError):
    """节点操作失败(SSH 不通/命令非零)。调用方按 spec §6 降级,不炸 cycle。"""


def node_by_host(host_id):
    for n in BOARD_NODES:
        if n["host_id"] == host_id:
            return n
    if host_id == "local":
        return LOCAL_NODE
    raise NodeError(f"未知节点: {host_id}")


def _ssh_argv(host_id, cmd):
    node = node_by_host(host_id)
    user_host, _, port = node["ssh"].rpartition(":")
    return ["ssh", "-o", "ConnectTimeout=8", "-o", "StrictHostKeyChecking=no",
            "-o", "UserKnownHostsFile=/dev/null", "-p", port, user_host, cmd]


def run_ssh(host_id, cmd, timeout=SSH_TIMEOUT_DEFAULT):
    """全模块唯一 ssh 出口(测试 monkeypatch 此单点,零真触网)。
    成功返回 stdout.strip(); 非零退出抛 NodeError(含退出码+stderr 摘要);
    超时同样归一为 NodeError(ConnectTimeout=8 只限 TCP 连接段,不覆盖已建
    连接的死寂——那是 subprocess 超时的职责),调用方只捕 NodeError 即可。"""
    try:
        proc = subprocess.run(_ssh_argv(host_id, cmd), capture_output=True,
                              timeout=timeout + 10)
    except subprocess.TimeoutExpired as exc:
        secs = exc.timeout if exc.timeout else timeout + 10
        cmd0 = exc.cmd[0] if isinstance(exc.cmd, (list, tuple)) else exc.cmd
        raise NodeError(f"节点 {host_id} 命令超时(>{secs}s): {cmd0} 无响应"
                        ) from exc
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", errors="replace")[:200]
        raise NodeError(f"节点 {host_id} 命令失败(rc={proc.returncode}): {err}")
    return proc.stdout.decode("utf-8", errors="replace").strip()


def put_file(host_id, local_path, remote_path):
    """scp 单点(同 run_ssh 的免密形态; agent.env/prompt 上板走这里)。
    超时同 run_ssh 归一为 NodeError。"""
    node = node_by_host(host_id)
    user_host, _, port = node["ssh"].rpartition(":")
    argv = ["scp", "-o", "ConnectTimeout=8", "-o", "StrictHostKeyChecking=no",
            "-o", "UserKnownHostsFile=/dev/null", "-P", port,
            local_path, f"{user_host}:{remote_path}"]
    try:
        proc = subprocess.run(argv, capture_output=True, timeout=60)
    except subprocess.TimeoutExpired as exc:
        secs = exc.timeout if exc.timeout else 60
        cmd0 = exc.cmd[0] if isinstance(exc.cmd, (list, tuple)) else exc.cmd
        raise NodeError(f"scp → {host_id} 超时(>{secs}s): {cmd0} 无响应"
                        ) from exc
    if proc.returncode != 0:
        raise NodeError(f"scp → {host_id} 失败(rc={proc.returncode}): "
                        f"{proc.stderr.decode(errors='replace')[:200]}")
