echo ===== search kernel for llc_domain_campaign string =====
search -k llc_domain
echo ===== mrn_rmw_diff log file dentry =====
p ((struct file *)0xffff4044310f6f00)->f_path.dentry->d_name.name
p ((struct file *)0xffff4044310f6f00)->f_path.dentry->d_parent->d_name.name
echo ===== check bo output file too =====
p ((struct file *)0xffff60202c532100)->f_path.dentry->d_name.name
quit
