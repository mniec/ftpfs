#include "net.h"
#include <linux/kernel.h>
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
