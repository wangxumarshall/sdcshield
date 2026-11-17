#!/bin/bash
# nodepool_setup.sh — 跨机卸载三板一次性环境准备(spec 2026-11-13-nodepool-offload
# §3.2,全部幂等)。直连板 SSH 密码形态仅本脚本使用;运行时一律密钥免密。
#
# 凭据红线: 本仓公开(github wangxumarshall/sdcshield),板卡 OS 密码绝不入
# 库——密码/IP 运行时从 skill 注册表 boards.conf 读取(单一事实源,同
# boards.sh);gh token 只经 stdin 管道传递,不进 argv/日志。
#
# brief(计划文)相对修正(实测依据):
# 1. 运行时 task_loop_nodepool._ssh_argv 不传 -i,专用密钥 id_ed25519_nodepool
#    非 ssh 默认身份名——必须写 ~/.ssh/config Host 块,否则探测/五操作全部
#    免密失败(Phase 1b)。
# 2. brief 脚本字面量含板卡密码(公开仓 gitleaks 红线)→ 改经 boards.conf。
# 3. 构建段 `A || true && B` 吞错(meson 失败被 || true 吃掉、dep 循环中段
#    失败被后续成功覆盖)→ 改 bash -s 整脚本 set -euo pipefail 逐环失败即止;
#    zstd19 冒烟 `| tail -1` 同样吞 rc → 显式判 rc。
# 4. 0201 实测缺 cmake(sleef/acl 构建需要)→ 工具链预检 + dnf 自动补齐;
#    0201 实测 git 身份未配(agent 的 git commit 会挂)→ 幂等补 user.name/email。
# 5. brief/spec 的「克隆 remote 统一 https+gh 凭据」实测不可行: 板卡 DNS
#    (114.114.114.114) 对 github.com 间歇污染(实测 0102/0201 连续解析到
#    20.247.166.64 非法 IP,https clone 报「expected flush after ref listing」
#    /连接超时);而三板均已注册 wangxumarshall 账号 SSH 钥匙且板内
#    ~/.ssh/config 已映射 github.com→ssh.github.com:443(实测三板
#    `ssh -T git@github.com` 全部 "successfully authenticated")——克隆
#    remote 改用 SSH(spec 自己认可的 0102 备选形态,推广到三板)。
#    gh CLI 面(api.github.com 实测可达)仍走 token,pr create 等不受影响。
# 6. gh 二进制下载(brief 的板上 curl github releases)实测不可行: 板卡
#    DNS 污染 + 释放资产 release-assets.githubusercontent.com 连 0103 直连
#    也被中途重置/截断(SSL_ERROR_SYSCALL/HTTP/2 INTERNAL_ERROR,ghproxy
#    镜像亦截断出 incomplete tar)——改为 LAN 分发: 取 0102 已装的
#    gh 2.97.0(静态 Go 二进制,file 实证「not a dynamic executable」,
#    跨同构 openEuler 24.03 板卡安全;与 0102 同版即 brief 指定版),
#    0102 不可达时回落本机 gh。
# 7. vendored 依赖 build.sh 的幂等 skip 分支实测有 SIGPIPE 竞态
#    (`ldd|head|grep` 管道 rc=141 被 set -euo pipefail 判死,0102 复跑
#    实锤)——Phase 5 自带工件门,不依赖其 skip 分支(遗留 bug 见任务报告,
#    属 third-party 修复单元,不混入本提交)。
#
# 用法: bash scripts/nodepool_setup.sh [板号...]   # 缺省 = 0102 0101 0201
# 复跑 = 幂等自检(已有密钥/authorized_keys/gh/克隆/install/ 全跳过,ninja 增量)。
set -euo pipefail

BOARDS="${*:-0102 0101 0201}"
BOARDS_CONF="${BOARDS_CONF:-$HOME/.claude/skills/accessing-sdc-boards/scripts/boards.conf}"
SSH_KEY=~/.ssh/id_ed25519_nodepool
SSH_CFG=~/.ssh/config
AGENT_ENV="${TASKLOOP_STATE_DIR:-$HOME/.local/state/sdcshield-task-loop}/agent.env"
CLONE_DIR=/home/sdc/wangxu/sdcshield-autoci

log() { echo "[$(date +%H:%M:%S)] $*"; }

[ -f "$BOARDS_CONF" ] || { echo "缺板卡注册表 $BOARDS_CONF(密码/IP 数据源,凭据不入库)"; exit 1; }
field_of() { awk -F'|' -v b="$1" -v f="$2" '$1==b {print $f; exit}' "$BOARDS_CONF"; }
ip_of()   { field_of "$1" 3; }
pass_of() { field_of "$1" 6; }

# ssh 直连(密码形态,仅 setup): 密码经 SSHPASS env,不进 argv
BSSH() {
  local p; p=$(pass_of "$1")
  SSHPASS="$p" sshpass -e ssh -o StrictHostKeyChecking=no \
    -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 sdc@"$(ip_of "$1")" "${@:2}"
}
# ssh 密钥形态(运行时 task_loop_nodepool 同款免密)
NPSSH() {
  ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    -o ConnectTimeout=8 -o ServerAliveInterval=30 sdc@"$(ip_of "$1")" "${@:2}"
}

# ------------------------------------------------------- 1) 密钥分发 + ssh config
log "=== Phase 1/5 密钥分发(0103 专用密钥 → authorized_keys + ~/.ssh/config) ==="
[ -f "$SSH_KEY" ] || ssh-keygen -t ed25519 -N "" -f "$SSH_KEY" -C nodepool@0103
PUB=$(cat "${SSH_KEY}.pub")
for B in $BOARDS; do
  BSSH "$B" "mkdir -p ~/.ssh && chmod 700 ~/.ssh && \
    { grep -qF '$PUB' ~/.ssh/authorized_keys 2>/dev/null || echo '$PUB' >> ~/.ssh/authorized_keys; }; \
    chmod 600 ~/.ssh/authorized_keys; echo '$B authorized_keys OK'"
done

# 1b) ~/.ssh/config Host 块(运行时免密的成立条件,覆盖注册表三板——与
#     task_loop_nodepool.BOARD_NODES 的 IP 集合一致,不随本次板号参数缩水)
CFG_MARK="# nodepool-offload (nodepool_setup.sh)"
if ! grep -qF "$CFG_MARK" "$SSH_CFG" 2>/dev/null; then
  {
    echo ""
    echo "$CFG_MARK"
    echo "Host 172.168.160.42 172.168.177.97 172.168.178.81"
    echo "    User sdc"
    echo "    IdentityFile $SSH_KEY"
    echo "    IdentitiesOnly yes"
  } >> "$SSH_CFG"
  log "~/.ssh/config 已追加 nodepool Host 块"
else
  log "~/.ssh/config nodepool Host 块已在,跳过"
fi

# 1c) 主机密钥固定(运行时 StrictHostKeyChecking=accept-new 的验证前提):
#     ssh-keyscan 一次性摘三板公钥入 ~/.ssh/known_hosts——之后 MITM 改钥即被
#     拒(TOFU 引导,后续逐次校验);Phase 4 的 agent.env token scp 与每晚
#     refresh_env_remote 运行时通道同享这一 known_hosts 验证网。
KNOWNS=~/.ssh/known_hosts
for _ip in 172.168.160.42 172.168.177.97 172.168.178.81; do
  if ! ssh-keygen -F "$_ip" -f "$KNOWNS" >/dev/null 2>&1; then
    ssh-keyscan "$_ip" 2>/dev/null >> "$KNOWNS" || {
      echo "ssh-keyscan $_ip 失败(先确认板可达再跑)"; exit 1; }
  fi
done
log "~/.ssh/known_hosts 三板主机密钥已固定"

# ------------------------------------------------------- 2) 免密自验(失败即止)
log "=== Phase 2/5 密钥免密自验(运行时全靠它) ==="
for B in $BOARDS; do
  NPSSH "$B" "hostname" >/dev/null || { echo "FAIL $B 免密未通"; exit 1; }
  log "$B 免密 OK"
done

# ------------------------------------------------------- 3) gh/克隆/工具链/身份
log "=== Phase 3/5 gh 登录 + 独立克隆 + 工具链补齐 ==="
TOKEN=$(gh auth token)   # 0103 现登 token;只经 stdin 下发,绝不回显
# 3a-pre) gh 二进制本地缓存: 取 0102 已装的 2.97.0(静态 Go 二进制,LAN 直拷
#         替代 github releases 下载——外网释放资产实测被重置,见头注 6)
GH_BIN=/tmp/gh.nodepool
if [ ! -x "$GH_BIN" ]; then
  if scp -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
       "sdc@$(ip_of 0102):/usr/bin/gh" "$GH_BIN" 2>/dev/null; then
    log "gh 二进制缓存: 取自 0102 /usr/bin/gh (brief 指定版 2.97.0)"
  else
    log "gh 二进制缓存: 0102 不可达,回落本机 gh(版本以实跑输出为准)"
    cp "$(command -v gh)" "$GH_BIN"
  fi
  "$GH_BIN" --version | head -1
fi
for B in $BOARDS; do
  # 3a) gh 二进制(0101/0201 实测缺;0102 已有);sudo 密码经 stdin,不进 argv
  if ! NPSSH "$B" "command -v gh >/dev/null"; then
    scp -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
        "$GH_BIN" "sdc@$(ip_of "$B"):/tmp/gh.nodepool"
    printf '%s\n' "$(pass_of "$B")" | NPSSH "$B" '
      read -r pw
      set -e
      echo "$pw" | sudo -S install -m 755 /tmp/gh.nodepool /usr/local/bin/gh
      rm -f /tmp/gh.nodepool
      gh --version | head -1'
  fi
  # 3b) 工具链预检: cmake 缺则 dnf 补(0201 实测);其余缺即响亮失败
  printf '%s\n' "$(pass_of "$B")" | NPSSH "$B" '
    read -r pw
    if ! command -v cmake >/dev/null; then
      echo "cmake 缺失,dnf 安装..."
      echo "$pw" | sudo -S dnf install -y cmake >/dev/null
    fi
    for t in git gcc g++ make perl cmake meson ninja python3 curl; do
      command -v "$t" >/dev/null || { echo "缺工具 $t(自动补齐仅覆盖 cmake)"; exit 1; }
    done
    echo "工具链 OK"'
  # 3c) gh 登录(token 经 stdin)+ setup-git(https push 凭据)
  if ! NPSSH "$B" "gh auth status >/dev/null 2>&1"; then
    printf '%s\n' "$TOKEN" | NPSSH "$B" "gh auth login --with-token" >/dev/null
  fi
  NPSSH "$B" "gh auth setup-git"
  log "$B gh: $(NPSSH "$B" "gh auth status 2>/dev/null | grep -o 'account [^ ]*' | head -1")"
  # 3d) git 身份(0201 实测 UNSET——agent 的 git commit -s 会挂;已配则不动)
  NPSSH "$B" "git config --global user.email >/dev/null 2>&1 || git config --global user.email 'wangxumarshall@qq.com'; \
git config --global user.name >/dev/null 2>&1 || git config --global user.name 'wangxu'; \
echo '$B git 身份: '\$(git config --global user.name)'<'\$(git config --global user.email)'>'"
  # 3e) state 目录 + GitHub SSH 通道自验 + 独立克隆(与板上开发克隆隔离)
  NPSSH "$B" "mkdir -p ~/sdcshield-autoci-state ~/wangxu && chmod 700 ~/sdcshield-autoci-state"
  NPSSH "$B" "timeout 15 ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -T git@github.com 2>&1 | grep -q 'successfully authenticated' || { echo '$B GitHub SSH 通道未通'; exit 1; }"
  NPSSH "$B" "[ -d $CLONE_DIR/.git ] && echo '$B 克隆已在' || { git clone git@github.com:wangxumarshall/sdcshield.git $CLONE_DIR && echo '$B 克隆完成'; }" 2>&1 | tail -1
done

# ------------------------------------------------------- 4) agent.env 就位
log "=== Phase 4/5 agent.env 就位(板上初始凭据 0600;refresh_env_remote 每派发前刷新) ==="
[ -f "$AGENT_ENV" ] || { echo "缺 $AGENT_ENV(见 task-loop.md §3.3 凭据部署)"; exit 1; }
for B in $BOARDS; do
  scp -i "$SSH_KEY" -o StrictHostKeyChecking=accept-new \
      "$AGENT_ENV" "sdc@$(ip_of "$B"):sdcshield-autoci-state/agent.env"
  NPSSH "$B" "chmod 600 ~/sdcshield-autoci-state/agent.env && stat -c '%a %n' ~/sdcshield-autoci-state/agent.env"
done

# ------------------------------------------------------- 5) vendored 依赖+构建+冒烟
log "=== Phase 5/5 vendored 依赖 + meson/ninja 构建 + 冒烟(install/ 已有即跳过) ==="
for B in $BOARDS; do
  log "--- $B 构建+冒烟 开始 ---"
  NPSSH "$B" "bash -s" <<'REMOTE'
set -euo pipefail
cd /home/sdc/wangxu/sdcshield-autoci
# vendored 依赖幂等门(镜像各 build.sh 自己的工件检查,不调用其 skip 分支):
# 实测 build.sh 的 skip 分支 `ldd|head|grep` 管道在快机上触发 SIGPIPE(rc=141)
# 被 set -euo pipefail 判死(0102 复跑实锤,静默空日志)——本门先查工件再决定
# 是否调 build.sh,确定性跳过,不受该竞态影响(build 路径无此管道)。工件
# 路径若与 build.sh 漂移,最坏方向=漏跳转全量重建,响亮可恢复。
dep_artifact() {
  case "$1" in
    openssl)  echo install/lib/libcrypto.a ;;
    openblas) echo install/lib/libopenblas.a ;;
    sleef)    echo install/lib/libsleef.a ;;
    isa-l)    echo install/lib/libisal.a ;;
    acl)      echo install/lib/libarm_compute-core.a ;;
  esac
}
for dep in openssl openblas sleef isa-l acl; do
  art="third-party/$dep/$(dep_artifact "$dep")"
  if [ -f "$art" ]; then
    echo "[dep] $dep: 工件已在($(basename "$art")),跳过 ($(date +%H:%M:%S))"
  elif JOBS=32 bash "third-party/$dep/build.sh" > "/tmp/dep-$dep.log" 2>&1; then
    echo "[dep] $dep: $(tail -1 "/tmp/dep-$dep.log") ($(date +%H:%M:%S))"
  else
    echo "[dep] $dep 构建失败,尾 5 行:"; tail -5 "/tmp/dep-$dep.log"; exit 1
  fi
done
if [ ! -f builddir/build.ninja ]; then
  PKG_CONFIG_PATH="$PWD/third-party/eigen5" meson setup builddir --buildtype=release
else
  echo "[meson] builddir 已配置,跳过 setup"
fi
ninja -C builddir -j 32
n=$(./builddir/sdcshield --list-tests | wc -l)
echo "list-tests: $n 行"
[ "$n" -ge 439 ] || { echo "list-tests $n < 439"; exit 1; }
if out=$(timeout 60 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1); then
  rc=0
else
  rc=$?
fi
echo "zstd19 冒烟: $(echo "$out" | tail -1) (rc=$rc)"
[ "$rc" -eq 0 ] || exit "$rc"
REMOTE
  log "--- $B 构建+冒烟 完成 ---"
done

log "=== setup 完成: $BOARDS ==="
