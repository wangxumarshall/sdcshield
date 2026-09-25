/* cntvct_helper.c — 读 ARM64 Generic Timer（v5 §4.2）。独立编译，不入 meson。
 * 输出: cntvct=<u64> cntfrq_hz=<u64>   编译: cc -O2 -o cntvct_helper cntvct_helper.c
 */
#include <stdio.h>
#include <stdint.h>
#if !defined(__aarch64__)
#error "aarch64 only（x86-64 参考架构不走本通道，v5 §4.2）"
#endif
static inline uint64_t read_cntvct(void) {
    uint64_t v; __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v)); return v;
}
static inline uint64_t read_cntfrq(void) {
    uint64_t v; __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v)); return v;
}
int main(void) {
    printf("cntvct=%llu cntfrq_hz=%llu\n",
           (unsigned long long)read_cntvct(),
           (unsigned long long)read_cntfrq());
    return 0;
}
