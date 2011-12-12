#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/fcntl.h>
#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/file.h>
#include <linux/socket.h>



#define FTPFS_MAGIC 0x19980122

#define CONTROL_PORT 21
#define SND_BUFFER_SIZE 4096
#define RCV_BUFFER_SIZE 0xFFFF

static ssize_t ftpfs_read_file(struct file *filp,char *buf, size_t count, loff_t *offset);
static ssize_t ftpfs_write_file(struct file *filp, const char *buf, size_t count, loff_t *offset);
static int ftpfs_create_file(struct inode *inode,struct dentry * dentry,int, struct nameidata *nameidata);
static int ftpfs_mkdir(struct inode *inode,struct dentry* dentry,int unk);
static int ftpfs_rmdir(struct inode *inode, struct dentry* dentry); 
static int ftpfs_rename(struct inode* inode_src, struct dentry* dentry_src,struct inode* inode_dst,struct dentry* dentry_dst );
static int ftpfs_unlink(struct inode* inode, struct dentry* dentry); 


struct ftp_sb_params
{
    char username[256];
    char password[256];
    unsigned int  host;
    char initial_dir[256];
};

struct ftp_sb_info
{
    struct socket *control;
    struct socket *data;
    struct ftp_sb_params params; /* parameters */
};

static struct ftp_sb_info* extract_info(struct file* filp)
{
    return (struct ftp_sb_info*)filp->f_dentry->d_sb->s_fs_info;
}

static struct file_operations ftpfs_file_ops = {
    .read = ftpfs_read_file,
    .write = ftpfs_write_file,
};



static struct inode_operations ftpfs_inode_ops={
    .create = ftpfs_create_file,
    .mkdir = ftpfs_mkdir,
    .rmdir = ftpfs_rmdir, 
    .rename = ftpfs_rename, 
    .lookup  = simple_lookup,
    .unlink = ftpfs_unlink,
};

struct super_operations ftpfs_super_ops = {
    .statfs         = simple_statfs,
    .drop_inode     = generic_delete_inode,
};
static const struct super_operations simple_super_operations = {
    .statfs         = simple_statfs,
};