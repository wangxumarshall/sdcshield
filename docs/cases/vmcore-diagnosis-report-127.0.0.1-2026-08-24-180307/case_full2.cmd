echo ===== struct bio full =====
struct bio 0xffff60401b366738
echo ===== struct page folio x22 =====
struct page 0xfffffd010d971400
echo ===== rd folio wider =====
rd -64 0xfffffd010d971400
echo ===== rd bvec wider =====
rd -64 0xffff60401dabd460
echo ===== struct page of bvec page ffffd810076af40 =====
struct page 0xfffffd810076af40
echo ===== search stack for x3 garbage 553c521da2e9b99f =====
search -t 2077673 553c521da2e9b99f
echo ===== search for ffff40295ce624d4 (last irqbalance fault addr) =====
search -t 9665 ffff40295ce624d4
echo ===== irqbalance bt =====
bt 9665
echo ===== per-cpu offset cpu179 =====
p __per_cpu_offset[179]
echo ===== mem Section verify: p x19 orig bio bi_io_vec =====
p ((struct bio *)0xffff60401b366738)->bi_io_vec
p ((struct bio *)0xffff60401b366738)->bi_vcnt
p ((struct bio *)0xffff60401b366738)->bi_iter
echo ===== read bio at panic =====
p ((struct bio *)0xffff60401b366738)->bi_flags
echo ===== struct folio x22 mapping =====
p ((struct folio *)0xfffffd010d971400)->mapping
p ((struct folio *)0xfffffd010d971400)->_mapcount
p ((struct folio *)0xfffffd010d971400)->_refcount
quit
