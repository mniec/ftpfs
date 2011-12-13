#ifndef FTPFS_H_
#define FTPFS_H_

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

static inline struct ftp_sb_info* extract_info(struct file* filp)
{
    return (struct ftp_sb_info*)filp->f_dentry->d_sb->s_fs_info;
}
#endif
