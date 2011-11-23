#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <asm/uaccess.h>
#include <asm/fcntl.h>
#include <linux/stat.h>

MODULE_AUTHOR("Pawel Pikula, Michal Niec");
MODULE_DESCRIPTION("Ftp File System");


static int init_ftp_fs(void)
{
  printk("FTPFS initialized");
  return 0; 
}

static void exit_ftp_fs(void)
{
  printk("FTPFS exited");
}


module_init(init_ftp_fs);
module_exit(exit_ftp_fs);

MODULE_LICENSE("GPL");

