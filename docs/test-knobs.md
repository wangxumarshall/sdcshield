# Test knob 总表（`-O` 运行期测试参数）

> **Provenance**：本文档自 README.md「Test knob」一节的总表与尾注抽离（2026-11-18，源 commit `727cc8d3`），
> 内容逐字搬运。README 主文保留语法、示例、两个易错点并链接至此。语法：`-O <测试ID>.<参数名>=<值>`
> （可重复传多个），参数值写入 YAML 日志（可复现）；框架以 `<测试ID>.<参数名>` 为查找键
> （`framework/test_knobs.cpp`）。

当前支持 knob 的测试全集（越界行为：标注 clamp 的为告警取整/钳制，其余 fail-loudly）：

| 测试 | 参数 | 合法域 | 默认 | 作用（档位→压力域） |
|---|---|---|---|---|
| `openblas_{d,s,z,c}gemm` | `.mdim` | 16..16384（越界告警并 clamp） | **内存自适应**（[128,8192]，85% 物理内存预算） | 矩阵尺寸：64→L1 / 256→L2 / 1024→L3 / 2048+→DRAM |
| `openblas_{d,s,z,c}gemm` | `.transab` | 0..3 | 0 | NN/NT/TN/TT（OpenBLAS 不同 packing 例程） |
| `openblas_{d,s,z,c}gemm` | `.beta_permille` | 0..10⁶ | 0 | β=P/1000；β≠0 命中 C 读改写路径 |
| `openblas_lu` | `.n` | 16..16384（clamp） | 内存自适应（[128,8192]） | LU 分解矩阵尺寸（64→32KB … 2048→32MB） |
| `sleef_neon` / `sleef_sve` | `.nelems` | 128..67108864（4 的倍数，非倍数告警取整） | 内存自适应 | 每函数族元素数（工作集 128KB→GB 级） |
| `pocketfft_fft` | `.n` | 512/1024/2048/4096/8192/16384（pow2）/ 4099 / **8191**（质数→Bluestein）/ 6144 / 10000（白名单外 fail-loudly） | 4096 | FFT 点数与因数结构（pow2=radix-4/2，4099/8191=Bluestein 质数） |
| `isal_igzip` | `.level` | 0..3（fail-loudly） | 1 | deflate 级别（不同匹配查找器；2/3 有独立 level_buf） |
| `zstd` / `zstd_aaa` | `.level` / `.maxbuffersize` | 1..22 / 4096.. | 按变体 | 压缩级别 / 每迭代缓冲上限 |
| `zstd1` / `zstd19` | `.maxbuffersize` | 4096.. | 按变体 | 每迭代缓冲上限（level 固定 1 / 19，无 level knob） |
| `zlib*`（NEON 家族） | `.level` / `.maxbuffersize` | 1..9 / 按变体 | 按变体 | zlib 压缩级别 / 缓冲上限（`zfuzz` 为随机 level 的 fuzz 往返、无 knob） |
| `eigen_gemm_*`（NEON 家族，含 `_dump` 变体；`eigen_gemm_double_ab_split` 例外——该用例未注册任何 knob，显式 `-O …mdim` 会被静默忽略） | `.mdim` | 16..4096（float 变体 8192；clamp） | 内存自适应 | 矩阵维度（覆盖自适应选择） |
| `eigen_gemm_double_max_arm` | `.mdim` | 1024..4096（clamp） | **按内存×时间联合自适应** | SVE GEMM 逐档变体的矩阵维度 |
| `eigen_svd_cdouble_sve` | `.mdim` | 300..6000（clamp） | **按内存×时间联合自适应** | 强制矩阵维度（预期内存警告目前覆盖 SVD 模板族，本用例显式路径暂无警告——T11b 将统一路由） |
| `eigen_svd{,_double,_double2,_fvectors}_sve` | `.mdim` | 64..2048（clamp） | **按内存×时间联合自适应** | SVE BDCSVD 系方阵维度（镜像各 NEON 原用例联合层参数口径） |
| `eigen_svd_jacobi{,_double,_fvectors,_cdouble}_sve` | `.mdim` | 64..2048（clamp） | **按内存×时间联合自适应** | SVE JacobiSVD 系方阵维度（SKIP 级，`--quality=-1` 显式运行） |
| `eigen_gemm_{float,double,cdouble}_dynamic_square_sve` / `eigen_gemm_double14_sve` | `.mdim` | 16..8192（float 8192，余 4096；clamp） | **按内存×时间联合自适应** | SVE GEMM 系方阵维度（镜像各 NEON 原用例） |
| `eigen_svd{,_double,_double2,_fvectors,_cdouble,_cdouble_noavx512}`（NEON 家族） | `.mdim` | 64..2048（clamp） | **按内存×时间联合自适应** | NEON 后端 BDCSVD 系方阵维度（`_cdouble_noavx512` 为同负载的门控变体，x86 命名沿用） |
| `eigen_svd_jacobi{,_double,_fvectors,_cdouble}`（NEON 家族） | `.mdim` | 64..2048（clamp） | **按内存×时间联合自适应** | NEON 后端 JacobiSVD 系方阵维度（SKIP 级，`--quality=-1` 显式运行） |
| `eigen_sparse` | `.n` | 64..8192（clamp） | 自适应 | 稀疏 Cholesky 规模 |
| `openssl_sha` / `openssl_sha3` | `.arena_bytes` | 1MiB..4GiB（clamp） | 内存自适应 | 哈希数据竞技场字节数（SHA-2 加法链 / Keccak 置换的工作集） |
| `openssl_sm3sm4` | `.data_size` | 1024..（自适应上限，clamp） | 自适应 | SM3/SM4 载荷尺寸 |
| `gather*` / `gatherscatter*` | `.data_size` | 按变体 | 自适应 | gather/scatter 工作集 |
| `random_access_sweep` | `.size_mb` / `.mode` / `.valmode` | 4..512（MiB，钳制）/ 0\|1 / 0\|1\|2（mode/valmode 域外值静默按 0 处理） | 内存自适应（60% MemAvailable÷线程，8..512MiB）/ 0 / 0 | 大 mmap 随机扫掠区大小（MiB）/ 地址模式（0=随机偏移，1=顺序）/ 值模式（0=随机，1=固定模式，2=边界特殊值） |
| `low_load_cache_pressure{,_neon}` | `.duty_us` | 500..3000（静默钳制） | 1000 | 低占空比（5-30%）跨 die 存→远端读扫掠的窗口占空时长（µs） |
| `ipsec_*`（46 个 NEON 变体） | `.datasize` | 1024..64MB（16 的倍数） | 1024 | 加密/解密/MAC 载荷尺寸：1024→L1 / 64KB→L2 / 1MB+→L3/DRAM |
| `ipsec_*_sve*`（17 个 HMAC/SHA2 变体） | `.data_size`（snake_case） | 1024..1900（sha384/512 变体上限 1830；HMAC 组装缓冲上限，clamp） | 内存自适应（流数=运行时 lane 数） | 多流 HMAC 载荷尺寸 |
| `ipsec_*_sve*`（7 个 CMAC/XCBC/GCM 变体：`*aes_cmac*` / `*xcbc*` / `*gcm*`） | `.data_size`（snake_case） | 1024..16MB（clamp，16B 对齐） | 内存自适应 | 加密/解密/MAC 载荷尺寸（单流 OpenSSL EVP 数据通路） |
| `sve512_{f32,f64}_chain_{arm,svd}` | `.bounded` | 0\|1 | 1 | 有界/无界操作数值域（SEVI Obs.19：SDC 频率差 245×） |
| `sve512_nt_reload_arm` | `.distance` | 1..4 | 2 | 栈重装载距离（NUMA3 VA 通路触发指令窗口） |
| `acl_gemm_sve` | `.dim` | 64..4096（clamp） | 内存自适应 | ACL NEGEMM F32 GEMM 方阵维度 |
| `adcx_sve` / `adcx_arm_sve` / `adox_sve` / `adox_arm_sve` / `adcx_adox_interleaved{,_arm}_sve` | `.count` | 1024..64M（clamp） | 内存自适应 | ADCX/ADOX 进位/借位链元素数 |
| `adcxlong{,_arm}_sve` | `.count` | 4096..64M（clamp） | 内存自适应 | 长进位链元素数 |
| `agu_stress_2src_sve` / `agu_stress_store_only_sve` | `.count` | 1024..32M（clamp） | 内存自适应 | AGU 吞吐压力（2-src ldr + 旋转 ALU / 纯 store）元素数 |
| `arm0102_{fma_f32,kreg_logic,kreg_mask,kreg_not,kreg_select}_sve` | `.count` | 1024..64M（clamp） | 内存自适应 | ARM0102 案例复现族（3-src ldr + 旋转 FMA/逻辑/掩码/选择）元素数 |
| `arm64_sdc_sve` | `.buffer_bytes` | 64KiB..1TiB（clamp） | 内存自适应 | CRC32/CRC64 向量扫掠工作集字节 |
| `crt_builtins_sve` | `.count` | 4096..64M（clamp） | 内存自适应 | compiler-rt 软浮点内建（__addsf3 等）操作数对数 |
| `gmp_bigadd_sve` | `.limbs` | 256..1M（clamp） | 内存自适应 | SVE 进位链大数加法操作数宽度（64-bit limb 数，256=2048-bit） |
| `iex_operand_combo_sve` | `.steps` | 1024..1M（clamp） | 内存自适应 | IEX 操作码组合链步数 |
| `operand_space_sve` | `.num_pairs` | 80..1M（clamp） | 内存自适应 | 操作数空间探针的操作数对数 |
| `mem_disambig_alu_sve` | `.buffer_bytes` | 4096..1TiB（clamp） | 内存自适应 | 内存消歧探针的地址跨度字节 |
| `mmu_split_tlb_sve` | `.pressure_pages` | 8192..1M（clamp） | 内存自适应 | 页色别名 TLB 压力跨度页数（4KiB/页） |
| `ooo_dep_chain_sve` | `.chain_len` / `.rob_depth` | 4096..16M / 2048..4M（clamp） | 内存自适应 | OoO 指针追逐依赖链长 / RW-hazard 槽深（兼作长 ALU 链深） |
| `fisttp_sve` | `.num_elements` | 1024..16M（clamp，上限随内存预算放开） | 内存自适应 | FCVTZS 截断转换元素数 |
| `fisttp_arm`（ACL 后端） | `.num_elements` | 1024..2×10⁹（clamp，上限随内存预算放开） | 内存自适应 | FCVTZS 截断转换元素数（需 third-party/acl 构建产物） |
| `movdq2q_sve` / `movq2dq_sve` / `movmskpspd_sve` / `neon_add_sve` | `.count` | 1024..64M（clamp） | 内存自适应 | z 寄存器↔GPR 搬运 / 符号位掩码提取 / SVE 加法的元素数 |
| `power_virus_dit_sve` | `.scale` | 1..256（clamp） | 1 | di/t 电压瞬态病毒 burst/stall 等比缩放（保持 1:1 占空比） |
| `mesh_upi_avx_sym` / `mesh_upi_avx512_sym` / `mesh_upi_avx512_symm_f64`（NEON/FP 变体，x86 命名沿用） | `.buffer_bytes` | 1MiB..16TiB（clamp） | 内存自适应 | mesh 互连对称压力工作集字节 |
| `mesh_upi_sve_*`（18 个 SVE 变体） | `.buffer_bytes` | 1MiB..16TiB（clamp） | 内存自适应 | SVE mesh 变体工作集字节 |
| `mite_sve` | `.count` | 1024..1M（非 16 倍数告警取整） | 1024 | add/eor/sub 常量链 + 谓词分支逻辑探针的缓冲元素数（诊断探针，默认固定设计几何） |
| `movbe_sve` / `movbe_l1d_sve` / `movbe_l2_sve` | `.count` | 16384..4M（非 16 倍数告警取整） | 16384 | 字节交换往返探针缓冲元素数（l1d/l2 = L1D 命中 / L2 重载足迹变体） |
| `movbe_slf_sweep_sve` | `.buffer_bytes` | 64KiB..256MiB（非 16 倍数告警取整） | 64KiB | 多宽度（16/32/64-bit）SLF 转发扫掠缓冲字节 |
| `mrn_{flags,nuke,nuke_2src_alu,nuke_3src_alu,nuke_4src_alu,pairs,pairs_2src_alu,reloaded,reloaded_2src_alu,rmw,rmw_dump,rmw_fwd,rmw_l1d,rmw_diff}_sve` | `.count` | 1024..4M（`mrn_reloaded_sve` 为 256..4M；非 8 倍数告警取整） | 1024（`mrn_reloaded_sve` 256） | 内存重命名/RMW 探针族缓冲元素数（u64，CPU179 触发配方，默认固定设计几何） |
| `mrn_nuke_dense_sve` | `.count` | 16384..4M（非 8 倍数告警取整） | 16384 | MRN 密集变体元素数 |
| `mrn_rmw_diff_{64k,1m}_sve` | `.base_count` | 8192..4M（非 8 倍数告警取整） | 8192 | RMW 足迹差分基计数（×16 得 64KB/1MB 跨度） |
| `mrn_rmw_nop{,1,4,8,16,64}_sve` | `.count` | 1024..4M（非 8 倍数告警取整） | 1024 | RMW 时序探针（nop 间隔变体）元素数 |
| `neon_rot_2src_sve` / `neon_rot_store_only_sve` | `.count` | 1024..4M（clamp） | 1024 | CPU179 触发配方（2-src ldr + 旋转 ALU / 纯 store）元素数 |
| `neon_rot_u8x16_sve` | `.buffer_bytes` | 16384..256MiB（clamp） | 16384 | 全 VL 字节 lane rotate 缓冲字节 |
| `neon_rot_ldr_at_top*_sve`（18 个变体；同名无 `_sve` 后缀的 NEON 版无 knob） | `.slots` | 1024..1M（k3res 变体 4096..4M；clamp） | 1024（k3res 4096） | 「顶部第 3 ldr z」位置探针槽位数（CPU179 LSU 触发配方族，默认固定设计几何） |
| `sve_rot_ldr_at_top*`（8 个变体） | `.slots` | 1024..1M（k3res 变体 4096 起；clamp） | 内存自适应 | SVE 版位置探针槽位数 |
| `spinlock_add_neon` | `.logverif` / `.logverif_retain` / `.selftest` | 0\|1 / 0\|1 / off\|stall\|data（错值 fail-loudly） | 0 / 0 / off | 逐次验证日志开关 / 绿跑日志保留 / 部署自检注入（stall=看门狗停顿，data=数据通路） |
| `memcpy_rewr` / `memcpy_rewr_neon` | 环境变量 `SANDSTONE_STRATEGY_INDEX` / `SANDSTONE_STRATEGY_CONF` | 0..2 / conf 路径 | — | MPSC 策略选择（env 而非 `-O` 机制） |

仍无 knob 的测试（`isal_crc*`×10 等）参数为编译期常量——参数审查（`docs/research/third-party-sdc-param-critique.md`）记录的已知状态（该文档已随 31f6a99e 清理出库，git 历史 `31f6a99e^` 可取回）。
