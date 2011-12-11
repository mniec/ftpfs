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

MODULE_AUTHOR("Pawel Pikula, Michal Niec");
MODULE_DESCRIPTION("Ftp File System");

#define FTPFS_MAGIC 0x19980122

#define CONTROL_PORT 21
#define SND_BUFFER_SIZE 4096
#define RCV_BUFFER_SIZE 0xFFFF

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


/* struct sockaddr_in saddr, daddr; */
/* struct socket *new_sock = NULL */;

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


    //TODO return error if string is empty or badly formated
    return 0;
}
int send_sync_buf (struct socket *sock, const char *buf,
                   const size_t length, unsigned long flags)
{
    struct msghdr msg;
    struct iovec iov;
    int len, written = 0, left = length;
    mm_segment_t oldmm;

    msg.msg_name     = 0;
    msg.msg_namelen  = 0;
    msg.msg_iov      = &iov;
    msg.msg_iovlen   = 1;
    msg.msg_control  = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags    = flags;

    oldmm = get_fs(); set_fs(KERNEL_DS);

repeat_send:
    msg.msg_iov->iov_len = left;
    msg.msg_iov->iov_base = (char *) buf + written;

    len = sock_sendmsg(sock, &msg, left);
    if ((len == -ERESTARTSYS) || (!(flags & MSG_DONTWAIT) &&
                                  (len == -EAGAIN)))
        goto repeat_send;
    if (len > 0) {
        written += len;
        left -= len;
        if (left)
            goto repeat_send;
    }
    set_fs(oldmm);
    return written ? written : len;
}

void send_reply(struct socket *sock, char *str)
{
    send_sync_buf(sock, str, strlen(str), MSG_DONTWAIT);
}

int read_response(struct socket *sock, char *str)
{

    struct msghdr msg;
    struct iovec iov;
    int len;
    int max_size;
    mm_segment_t oldmm;

    max_size= RCV_BUFFER_SIZE;

    msg.msg_name     = 0;
    msg.msg_namelen  = 0;
    msg.msg_iov      = &iov;
    msg.msg_iovlen   = 1;
    msg.msg_control  = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags    = 0;

    msg.msg_iov->iov_base = str;
    msg.msg_iov->iov_len  = max_size;

    oldmm = get_fs(); set_fs(KERNEL_DS);
read_again:
    len = sock_recvmsg(sock, &msg, max_size, MSG_DONTWAIT); /*MSG_DONTWAIT); */
    if (len == -EAGAIN || len == -ERESTARTSYS) {
        goto read_again;
    }

    set_fs(oldmm);
	str[len]='\0';

    printk("FTPFS::server:: =  %s\n",str);
    return len;
}
static int ftpfs_init_data_connection(struct ftp_sb_info *info)
{
    struct sockaddr_in saddr;
    int r;
    char *send_buffer,*recv_buffer,*tmp;
    int i1,i2,i3,i4,p1,p2;
	unsigned int ip;
	unsigned int port;


    send_buffer = kmalloc(SND_BUFFER_SIZE,GFP_KERNEL);
    recv_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
    tmp = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);



    sprintf(tmp, "PASV\r\n");
    send_reply(info->control, tmp);
    r=read_response(info->control, recv_buffer);

	sscanf(recv_buffer,"%d %*s %*s %*s (%d,%d,%d,%d,%d,%d)",&r,&i1,&i2,&i3,&i4,&p1,&p2);
	ip = (i1<<0x18)+(i2<<0x10)+(i3<<0x8)+i4;
	port = 0x100*p1+p2;

	printk("FTPFS::data connection: %x:%x",ip,port);


	sock_create(PF_INET, SOCK_STREAM,IPPROTO_TCP, &info->data);

    memset(&saddr,0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr =htonl(ip);

    r = info->control->ops->connect(info->data,
                                    (struct sockaddr *) &saddr,
                                    sizeof(saddr), O_RDWR);

	kfree(send_buffer);
	kfree(recv_buffer);
	kfree(tmp);
	return 0;
}


static int ftpfs_init_connection(struct ftp_sb_info *info)
{
    struct sockaddr_in saddr;
	int r;
    char *send_buffer,*recv_buffer,*tmp;


    send_buffer = kmalloc(SND_BUFFER_SIZE,GFP_KERNEL);
    recv_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
    tmp = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);


    sock_create(PF_INET, SOCK_STREAM,IPPROTO_TCP, &info->control);

    memset(&saddr,0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(CONTROL_PORT);
    saddr.sin_addr.s_addr =htonl(info->params.host);

    r = info->control->ops->connect(info->control,
                              (struct sockaddr *) &saddr,
									sizeof(saddr), O_RDWR);

    if (r && (r != -EINPROGRESS)) {
        printk("FTPFS::connect error: %d\n", r);
    }

    r=read_response(info->control, recv_buffer);

    sprintf(tmp, "USER %s\r\n",info->params.username );
    send_reply(info->control, tmp);
    r=read_response(info->control, recv_buffer);

    sprintf(tmp, "PASS %s\r\n", info->params.password);
    send_reply(info->control, tmp);
	r=read_response(info->control, tmp);

	ftpfs_init_data_connection(info);

	kfree(send_buffer);
	kfree(recv_buffer);
	kfree(tmp);
    return 0;
}

struct tree_descr *  ftpfs_ls(struct ftp_sb_info *info)
{
	struct tree_descr *files;
    char *send_buffer,*recv_buffer,*tmp;
	int size;
	char file_name[256];
	int r;
	int i;


    send_buffer = kmalloc(SND_BUFFER_SIZE,GFP_KERNEL);
    recv_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
    tmp = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);

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


    files = kmalloc(sizeof(struct tree_descr)*(i+1),GFP_KERNEL);

	printk("files init,%x",files);

	i=0;
	tmp=recv_buffer;
	while((tmp = strchr(tmp+1,'\n'))){
		memset(file_name,0,256);
		sscanf(tmp+1,"%*s %*d %*d %*d %d %*s %*d %*d:%*d%s\n",&size,file_name);
		printk("%s => %d\n",file_name,size);
		files[i].name = kmalloc(strlen(file_name)+1,GFP_KERNEL);
        strcpy(files[i].name,file_name);
		files[i].ops = &ftpfs_file_ops;
        files[i].mode =  S_IWUSR|S_IRUGO;
		i++;

	}
	files[i].name="";
	files[i].ops = NULL;
	files[i].mode = 0;

    kfree(send_buffer);
    kfree(recv_buffer);
    return files;
}


static int ftpfs_cd(char *path,struct ftp_sb_info *info)
{
    char *send_buffer,*recv_buffer,*tmp;
	int r;

    send_buffer = kmalloc(SND_BUFFER_SIZE,GFP_KERNEL);
    recv_buffer = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);
    tmp = kmalloc(RCV_BUFFER_SIZE,GFP_KERNEL);

    sprintf(tmp, "CWD %s\r\n",path);
    send_reply(info->control, tmp);
    r=read_response(info->control, recv_buffer);

    kfree(send_buffer);
    kfree(recv_buffer);
    kfree(tmp);
	return 0;
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

    return simple_fill_super(sb, FTPFS_MAGIC,files);
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

