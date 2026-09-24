echo ===== sys =====
sys
echo ===== set =====
set
echo ===== mach =====
mach
echo ===== bt -t =====
bt -t
echo ===== bt -r registers =====
bt -r
echo ===== irqbalance task =====
ps | grep -E 'irqbalance|kworker/u391'
echo ===== struct bio x19 =====
rd -64 0xffff60401b366738
echo ===== struct page for folio x22 =====
rd -32 0xfffffd010d971400
echo ===== bvec address x0 =====
rd -32 0xffff60401dabd460
echo ===== vtop bvec =====
vtop 0xffff60401dabd460
echo ===== vtop bio =====
vtop 0xffff60401b366738
echo ===== vtop far =====
vtop 0x003c521da2e9b99f
echo ===== vtop seq buf ffff40295ce62000 =====
vtop 0xffff40295ce62000
echo ===== irqbalance task struct =====
ps -m | head -30
echo ===== kmem -s biovec-32 skip (slow) =====
echo ===== runq =====
runq
echo ===== timer head =====
timer | head -40
quit
