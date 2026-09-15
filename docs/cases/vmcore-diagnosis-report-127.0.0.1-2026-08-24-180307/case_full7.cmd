echo ===== mrn_rmw_diff task =====
ps -l | grep mrn_rmw_diff | head -5
echo ===== files of one HeapHelper =====
files 1213226
echo ===== files of exe =====
ps | grep -w exe | head -3
echo ===== vm stats =====
kmem -i | head -25
echo ===== nr CPUs online =====
p nr_cpu_ids
p cpu_online_mask.bits[0]
quit
