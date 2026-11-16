#!/bin/bash
# gen_agent_envs.sh — 从 ~/.bashrc 提取三把 GLM key(key3/wang/ding)生成
# agent-key-N.env 池文件(0600,幂等)。派发层 round-robin 消费(spec 决策 #9:
# 8 并发 agent 均摊三 key,单 key 速率压力 1/3——降时延)。
# token 绝不打印,只经文件传递;失败即止(缺一个 key 就少一个池文件,如实报告)。
set -euo pipefail
STATE_DIR="${TASKLOOP_STATE_DIR:-$HOME/.local/state/sdcshield-task-loop}"
BASE="$STATE_DIR/agent.env"
[ -f "$BASE" ] || { echo "缺基准 $BASE"; exit 1; }
[ -f "$HOME/.bashrc" ] || { echo "缺 ~/.bashrc(三 key 函数所在)"; exit 1; }
# 基准须带 token 行,否则 sed 替换不命中、池文件静默等同基准(轮换失效):
# 提前 fail-fast 如实报告部署问题。
grep -q '^export ANTHROPIC_AUTH_TOKEN=' "$BASE" \
  || { echo "基准 $BASE 缺 export ANTHROPIC_AUTH_TOKEN 行"; exit 1; }
# 幂等清理旧池再生成: 编号按提取到的 key 顺序重排,不清残留则删掉的 key
# 旧池文件会留场续用(已吊销 token 永不退役),破坏幂等。
rm -f "$STATE_DIR"/agent-key-*.env
i=0
for fn in set-glm-qwen-key3 set-glm-qwen-key-wang set-glm-qwen-key-ding; do
  tok=$(sed -n "/^${fn}()/,/^}/p" "$HOME/.bashrc" \
        | grep -oP 'ANTHROPIC_AUTH_TOKEN="\K[^"]+' | head -1 || true)
  if [ -z "$tok" ]; then echo "[warn] $fn 无 token,跳过"; continue; fi
  i=$((i+1))
  out="$STATE_DIR/agent-key-${i}.env"
  sed -E "s|^export ANTHROPIC_AUTH_TOKEN=.*|export ANTHROPIC_AUTH_TOKEN=\"${tok}\" #pool-${i}|" \
      "$BASE" > "$out"
  chmod 600 "$out"
done
echo "生成 ${i} 个池文件于 $STATE_DIR(期望 3)"
