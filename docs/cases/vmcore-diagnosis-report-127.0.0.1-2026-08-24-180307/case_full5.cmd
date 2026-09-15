echo ===== folio flags confirm x1 =====
rd -64 0xfffffd010d971400
echo ===== vmemmap region of true bv_page 0xfffffd012d055b80 =====
rd -64 0xfffffd012d055b80
echo ===== phys addr of true bvec70 page: vtop fffffd012d055b80? struct page => pfn =====
vtop 0xfffffd012d055b80
echo ===== kmem struct page fffffd012d055b80 =====
kmem fffffd012d055b80
quit
