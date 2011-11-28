#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/fcntl.h>

MODULE_AUTHOR("Pawel Pikula, Michal Niec");
MODULE_DESCRIPTION("Ftp File System");

#define FTPFS_MAGIC 0x19980122

static int  ftpfs_open(struct inode *inode, struct file *filp) 
{
	printk("open file op\n");
	return 0;
}

static ssize_t ftpfs_read_file(struct file *filp, char *buf, size_t count, loff_t *offset) 
{
	printk("read file op\n");
	return 0; 
}

static ssize_t ftpfs_write_file(struct file *filp, const char *buf, size_t count, loff_t *offset) 
{
	printk("write file op\n");
	return 0;
}

static struct file_operations ftpfs_file_ops = {
	.open = ftpfs_open,
	.read = ftpfs_read_file,
	.write = ftpfs_write_file,
};

// Fill superblock 
struct tree_descr testFiles[] = {
	{NULL, NULL, 0}, 
	{
	 .name="test1",
	 .ops=&ftpfs_file_ops,
	 .mode = S_IWUSR|S_IRUGO
	},
	{
	 .name="test2",
	 .ops=&ftpfs_file_ops,
	 .mode = S_IWUSR|S_IRUGO
	},
	{"",NULL,0}
};

static int ftpfs_fill_super(struct super_block *sb, void *data, int silent)
{
	//TODO: initialize connection here and get file list
	return simple_fill_super(sb, FTPFS_MAGIC,testFiles);
}

// Params passed when registering filesystem
static struct dentry* ftpfs_get_super(struct file_system_type *fst, int flags, const char *devname, void *data) 
{
	return mount_single(fst, flags, data, ftpfs_fill_super);
}

static struct file_system_type ftpfs_type = {
	.owner 	= THIS_MODULE,
	.name= "ftpfs",
	.mount= ftpfs_get_super,
	.kill_sb= kill_litter_super,
};



static int init_ftp_fs(void)
{
  printk("FTPFS initialized\n");
  return register_filesystem(&ftpfs_type);
}

static void exit_ftp_fs(void)
{
  printk("FTPFS exited\n");
  unregister_filesystem(&ftpfs_type);
}


module_init(init_ftp_fs);
module_exit(exit_ftp_fs);

MODULE_LICENSE("GPL");

