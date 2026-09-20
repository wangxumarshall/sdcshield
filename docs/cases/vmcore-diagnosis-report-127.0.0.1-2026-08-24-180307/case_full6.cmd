echo ===== ps grep sdc =====
ps | grep -i -E 'sdc|detector|patrol'
echo ===== ps count =====
ps | wc -l
echo ===== tasks on cpu 179 =====
ps -c 179
echo ===== per cpu 179 current =====
p __per_cpu_offset[179]
runq -c 179
echo ===== check active task list names around 1216xxx =====
ps | grep -E 'exe|mrn|Heap' | head -20
quit
