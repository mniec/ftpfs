#include "operations.h"
#include "net.h"

int ftpfs_init_data_connection(struct ftp_sb_info *info)
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


int ftpfs_init_connection(struct ftp_sb_info *info)
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

int ftpfs_cd(char *path,struct ftp_sb_info *info)
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
