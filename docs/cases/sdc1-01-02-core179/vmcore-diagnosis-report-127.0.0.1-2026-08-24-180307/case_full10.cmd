echo ===== argv of mrn via user string =====
rd -u -a 0xffff0092bcb1f75 200
echo ===== envp too =====
p ((struct task_struct *)0xffff404003bb2a00)->mm->env_start
quit
