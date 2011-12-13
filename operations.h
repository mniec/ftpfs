#include <linux/fs.h>
#include "ftpfs.h"

extern int ftpfs_init_data_connection(struct ftp_sb_info *info);
extern int ftpfs_init_connection(struct ftp_sb_info *info);
extern int ftpfs_cd(char *path,struct ftp_sb_info *info);


