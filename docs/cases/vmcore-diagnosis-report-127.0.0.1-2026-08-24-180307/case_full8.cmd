echo ===== files of mrn_rmw_diff 1216546 =====
files 1216546
echo ===== exe of mrn via mm exe file dentry =====
p ((struct task_struct *)0xffff404003bb2a00)->mm->exe_file->f_path.dentry->d_name.name
echo ===== parent =====
p ((struct task_struct *)0xffff404003bb2a00)->parent->comm
echo ===== env/argv: read arg_start =====
p ((struct task_struct *)0xffff404003bb2a00)->mm->arg_start
quit
