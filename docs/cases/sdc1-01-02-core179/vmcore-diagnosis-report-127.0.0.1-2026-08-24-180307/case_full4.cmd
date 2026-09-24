echo ===== bvec70 bv_page true value 64bit =====
rd -64 0xffff60401dabd460
echo ===== bvec69 bv_page =====
rd -64 0xffff60401dabd450
echo ===== bvec70 len offset words =====
rd -32 0xffff60401dabd468
echo ===== dump 16 qwords around bvec70 =====
rd 0xffff60401dabd440 12
echo ===== is 0x553c521da2e9b99f anywhere in bvec array page? search phys page 60401dabd000 =====
search -p 60401dabd000 553c521da2e9b99f
echo ===== check the vmemmap entry of bvec70 page: struct page ffffd810076af40? that is the page struct of the BVEC ARRAY page itself =====
rd -64 0xfffffd810076af40
quit
