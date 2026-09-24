echo ===== find sdc tasks in task table =====
ps | grep -E 'sdc|long'
echo ===== comm search all tasks via foreach =====
foreach ps -R 1216562
echo ===== list claude tasks =====
ps | grep claude | head -5
echo ===== look at the full task table names histogram head =====
ps | awk '{print $NF}' | sort | uniq -c | sort -rn | head -25
quit
