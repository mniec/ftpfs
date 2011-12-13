#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <asm/fcntl.h>
#include <linux/net.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <linux/socket.h>


#define SND_BUFFER_SIZE 4096
#define RCV_BUFFER_SIZE 0xFFFF

extern int send_sync_buf (struct socket *sock, const char *buf,
						  const size_t length, unsigned long flags);
extern  void send_reply(struct socket *sock, char *str);

extern int read_response(struct socket *sock, char *str);

