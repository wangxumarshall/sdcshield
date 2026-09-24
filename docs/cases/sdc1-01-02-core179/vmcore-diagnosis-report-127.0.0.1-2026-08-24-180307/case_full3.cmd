echo ===== TRUE bvec[70] in memory =====
rd -16 0xffff60401dabd460
echo ===== bvec[69], [70], [71] =====
rd -48 0xffff60401dabd450
echo ===== bvec[0..3] =====
rd -64 0xffff60401dabd000
echo ===== vtop bvec70 page phys =====
vtop 0xffff60401dabd460
echo ===== kmem page of phys 60401dabd000 =====
kmem 60401dabd460
echo ===== the faulting bio again: bi_io_vec all 71 bvecs scan bv_page values =====
rd 0xffff60401dabd000 71
echo ===== folio mapping struct check (inode/address_space) =====
p ((struct address_space *)0xffff404420819ce0)->host
echo ===== x24 0xffff404420819b68 relate to mapping? =====
rd -32 0xffff404420819b68
quit
