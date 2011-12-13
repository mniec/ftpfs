#include "ftpfs.h"
#include "net.h"
#include "operations.h"

MODULE_AUTHOR("Pawel Pikula, Michal Niec");
MODULE_DESCRIPTION("Ftp File System");

static ssize_t ftpfs_read_file(struct file *filp,char *buf, size_t count, loff_t *offset);
static ssize_t ftpfs_write_file(struct file *filp, const char *buf, size_t count, loff_t *offset);
static int ftpfs_create_file(struct inode *inode,struct dentry * dentry,int, struct nameidata *nameidata);
static int ftpfs_mkdir(struct inode *inode,struct dentry* dentry,int unk);
static int ftpfs_rmdir(struct inode *inode, struct dentry* dentry);
static int ftpfs_rename(struct inode* inode_src, struct dentry* dentry_src,struct inode* inode_dst,struct dentry* dentry_dst );
static int ftpfs_unlink(struct inode* inode, struct dentry* dentry);



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




struct tree_descr *  ftpfs_ls(struct ftp_sb_info *info)
{
    struct tree_descr *files;
    char *send_buffer,*recv_buffer,*tmp;
    int size;
    char file_name[256];
    int r;
    int i;

    char *creds;


    send_buffer = kmalloc(SND_BUFFER_SIZE,GFP_KERNEL);
    recv_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
    tmp = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
    creds=kmalloc(12,GFP_KERNEL);

    sprintf(tmp, "LIST\r\n");
    send_reply(info->control, tmp);
    r=read_response(info->control, recv_buffer);
    read_response(info->data, recv_buffer);

    printk("FTPFS::ls %s\n\n",recv_buffer);

    tmp = recv_buffer;

    i=1;
    while((tmp = strchr(tmp+1,'\n'))){
        i++;
    }


    files = kmalloc(sizeof(struct tree_descr)*(i+2),GFP_KERNEL);

    i=0;
    tmp=recv_buffer;
    while((tmp = strchr(tmp+1,'\n'))){
        memset(file_name,0,256);
        sscanf(tmp+1,"%s %*d %*d %*d %d %*s %*d %*d:%*d%s\n",creds,&size,file_name);
        printk("%s => %d\n",file_name,size);
        files[i].name = kmalloc(strlen(file_name)+1,GFP_KERNEL);
        strcpy(files[i].name,file_name);
        files[i].ops = &ftpfs_file_ops;
        files[i].mode =  S_IWUSR|S_IRUGO;
        if(creds[0] == 'd' ) files[i].mode |= S_IFDIR;
        else if(creds[0] =='-') files[i].mode |= S_IFREG;
        i++;
        if( unlikely(i==1)) {
            files[i].name=0;
            files[i].ops=NULL;
            files[i].mode=0;
            i++;
        }
    }
    files[i].name="";
    files[i].ops = NULL;
    files[i].mode = 0;

    read_response(info->control,recv_buffer);

    kfree(creds);
    kfree(send_buffer);
    kfree(recv_buffer);
    return files;
}

static int parse_address(const char* address,struct ftp_sb_params *params)
{
    const char *host;
    char host_buff[16];
    char *pass;
    char *init_dir;
    int i1,i2,i3,i4;

    memset(params->username,0,sizeof(params->username));
    memset(params->password,0,sizeof(params->password));
    memset(params->initial_dir,0,sizeof(params->initial_dir));

    host = strchr(address,'@');

    if(host){
        pass = strchr(address,':');
        memcpy(params->username,address,pass-address);
        memcpy(params->password,pass+1,host-pass-1);
    }else{
        host=address;
    }
    init_dir = strchr(host,':');

    if(init_dir){
        memcpy(host_buff,host+1,16);
		if(strlen(init_dir+1)!=0)
			strcpy(params->initial_dir,init_dir+1);
		else
			strcpy(params->initial_dir,"~");
    }else{
        strncpy(host_buff,host,16);
        strcpy(params->initial_dir,"/");
    }

    sscanf(host_buff,"%d.%d.%d.%d",&i1,&i2,&i3,&i4);
    params->host = (i1<<24)+(i2<<16)+(i3<<8)+i4;
    return 0;
}

// extending simple_fill_super adds  inode operations
static int ftpfs_simple_fill_super(struct super_block *s,unsigned long magic,struct tree_descr* files)
{
	struct inode* root_inode;
	struct dentry* root;
	struct dentry *dentry;
	struct inode* inode;
	int  i;

	s->s_blocksize = PAGE_CACHE_SIZE;
	s->s_blocksize_bits = PAGE_CACHE_SHIFT;
	s->s_magic = magic;
	s->s_op = &simple_super_operations;
    s->s_time_gran = 1;

	root_inode = new_inode(s);
	if(!root_inode) return -ENOMEM;

	root_inode->i_op = &ftpfs_inode_ops;
	//root_inode->i_op = &simple_dir_inode_operations;
	root_inode->i_fop = &simple_dir_operations;
	root_inode->i_ino = 1;
	root_inode->i_mode = S_IFDIR | 0755;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = CURRENT_TIME;
	root_inode->i_nlink = 2;

	root = d_alloc_root(root_inode);
	if(!root){
		iput(root_inode);
		return -ENOMEM;
	}

	for (i = 0; !files->name || files->name[0]; i++, files++)
	{
		if (!files->name) continue;
		if (unlikely(i == 1))
			printk(KERN_WARNING "%s: %s passed in a files array with an index of 1!\n", __func__,s->s_type->name);
		dentry = d_alloc_name(root, files->name);
		if(!dentry) goto out;
		inode =new_inode(s);
		if(!inode) goto out;
		inode->i_mode = files->mode;
		inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
		inode->i_fop = files->ops;
	    inode->i_ino = i;
		d_add(dentry, inode);
	}

	s->s_root=root;
	return 0;
out:
	d_genocide(root);
	dput(root);
	return -ENOMEM;
}


static int ftpfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct ftp_sb_params params;
    struct ftp_sb_info *info;
	struct tree_descr *files;

    info = kmalloc(sizeof(struct ftp_sb_info),GFP_KERNEL);

    parse_address(data,&params);
    info->params = params;

    ftpfs_init_connection(info);
	ftpfs_cd(params.initial_dir,info);

    sb->s_fs_info = info;

    printk("FTPFS::params = username:%s, password:%s, host:%x, init:%s",params.username,params.password,params.host,params.initial_dir);

	files= ftpfs_ls(info);

    //return simple_fill_super(sb, FTPFS_MAGIC,files);
	return ftpfs_simple_fill_super(sb,FTPFS_MAGIC,files);
}

static struct dentry* ftpfs_mount(struct file_system_type *fst, int flags, const char *dev_name, void *data)
{
    return mount_nodev(fst, flags,(void *) dev_name, ftpfs_fill_super);
}

static struct file_system_type ftpfs_type = {
    .owner  = THIS_MODULE,
    .name= "ftpfs",
    .mount= ftpfs_mount,
    .kill_sb= kill_litter_super,
};


static ssize_t ftpfs_read_file(struct file *filp, char *buf, size_t count, loff_t *offset)
{
    int r;
    const char* filename=filp->f_dentry->d_name.name;
    char *recv_buffer,*command;
    struct ftp_sb_info* ftp_info;

    if(*offset==0){

        ftp_info = extract_info(filp);

        recv_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
        command = kmalloc(RCV_BUFFER_SIZE, GFP_KERNEL);

        sprintf(command,"TYPE I\r\n");
        send_reply(ftp_info->control, command);

        r=read_response(ftp_info->control, recv_buffer); /* "200 Switching
                                                          * to binary mode" */

        ftpfs_init_data_connection(ftp_info);           /* PASV stuff*/

        sprintf(command,"RETR %s\r\n",filename);
        send_reply(ftp_info->control, command);
        r=read_response(ftp_info->control, recv_buffer); /* "Opening BINARY blalal"*/

        r=read_response(ftp_info->data, recv_buffer); /* Actual data */

        *offset +=r;
		memcpy(buf,recv_buffer,r);

        read_response(ftp_info->control, recv_buffer); /* Transer
                                                        * complete        */
		kfree(command);
        kfree(recv_buffer);
        return r;
	}
	else
		return 0;
}

static ssize_t ftpfs_write_file(struct file *filp, const char *buf, size_t count, loff_t *offset)
{
	int r;
	const char* filename=filp->f_dentry->d_name.name;
	char *recv_buffer,*command,*send_buffer;
    struct ftp_sb_info* ftp_info = extract_info(filp);
	if( *offset==0 && count < SND_BUFFER_SIZE ){
		recv_buffer = kmalloc(RCV_BUFFER_SIZE, GFP_KERNEL);
		send_buffer = kmalloc(SND_BUFFER_SIZE, GFP_KERNEL);
		command = kmalloc(RCV_BUFFER_SIZE, GFP_KERNEL);

		memset(send_buffer,0,SND_BUFFER_SIZE);

		sprintf(command,"TYPE I\r\n");
		send_reply(ftp_info->control,command);

		r=read_response(ftp_info->control, recv_buffer); /* switching to binary mode */

		ftpfs_init_data_connection(ftp_info); /*PASV stuff*/

		sprintf(command,"STOR %s\r\n",filename);
		send_reply(ftp_info->control,command);
		r=read_response(ftp_info->control,recv_buffer); /* opening binary*/

		memcpy(send_buffer,buf,count);
		send_reply(ftp_info->data,send_buffer);
		sock_release(ftp_info->data);
		r=read_response(ftp_info->control,recv_buffer);


		kfree(command);
		kfree(send_buffer);
		kfree(recv_buffer);
		return count;
	}else
	    return 0;
}

static int ftpfs_create_file(struct inode *inode,struct dentry * dentry,int mode, struct nameidata *nameidata)
{
	const char* filename;
	int r;
	char *command,*receive_buffer;
	//retreive control socket
	struct ftp_sb_info* ftp_info = (struct ftp_sb_info*)inode->i_sb->s_fs_info;
	filename=dentry->d_name.name;
	command = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
	receive_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);

	sprintf(command,"APPE %s\r\n",filename);
	send_reply(ftp_info->control,command);
	r=read_response(ftp_info->control,receive_buffer);
	ftpfs_init_data_connection(ftp_info);
	sock_release(ftp_info->data);
	r=read_response(ftp_info->control,receive_buffer);

	kfree(command);
	kfree(receive_buffer);

	return 0;
}

static int ftpfs_mkdir(struct inode *inode,struct dentry* dentry,int mode)
{
	const char* dirname;
	int r;
	char *command,*receive_buffer;
	//retreive control socket
	struct ftp_sb_info* ftp_info = (struct ftp_sb_info*)inode->i_sb->s_fs_info;
	dirname=dentry->d_name.name;
	command = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
	receive_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);

	sprintf(command,"MKD %s\r\n",dirname);
	send_reply(ftp_info->control,command);
	r=read_response(ftp_info->control,receive_buffer);

	kfree(command);
	kfree(receive_buffer);

	return 0;
}

static int ftpfs_rmdir(struct inode *inode, struct dentry* dentry)
{
	const char* dirname;
	int r;
	char *command,*receive_buffer;
	//retreive control socket
	struct ftp_sb_info* ftp_info = (struct ftp_sb_info*)inode->i_sb->s_fs_info;
	dirname=dentry->d_name.name;
	command = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
	receive_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);

	sprintf(command,"RMD %s\r\n",dirname);
	send_reply(ftp_info->control,command);
	r=read_response(ftp_info->control,receive_buffer);

	kfree(command);
	kfree(receive_buffer);


	return 0;
}

static int ftpfs_rename(struct inode* inode_src, struct dentry* dentry_src,struct inode* inode_dst,struct dentry* dentry_dst )
{
	const char* src;
	const char* dst;
	int r;
	char *command,*receive_buffer;
	//retreive control socket
	struct ftp_sb_info* ftp_info = (struct ftp_sb_info*)inode_src->i_sb->s_fs_info;
	src=dentry_src->d_name.name;
	dst=dentry_dst->d_name.name;
	command = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
	receive_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);

	sprintf(command,"RNFR %s\r\n",src);
	send_reply(ftp_info->control,command);
	r=read_response(ftp_info->control,receive_buffer);

	sprintf(command,"RNTO %s\r\n",dst);
	send_reply(ftp_info->control,command);
	r=read_response(ftp_info->control,receive_buffer);

	kfree(command);
	kfree(receive_buffer);

	return 0;
}
static int ftpfs_unlink(struct inode *inode,struct dentry *dentry)
{
	const char* filename;
	int r;
	char *command,*receive_buffer;
	//retreive control socket
	struct ftp_sb_info* ftp_info = (struct ftp_sb_info*)inode->i_sb->s_fs_info;
	filename=dentry->d_name.name;
	command = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
	receive_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);

	sprintf(command,"DELE %s\r\n",filename);
	send_reply(ftp_info->control,command);
	r=read_response(ftp_info->control,receive_buffer);

	kfree(command);
	kfree(receive_buffer);

	return 0;
}

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

