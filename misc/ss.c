#include <stdio.h>
#include <sys/socket.h>
#include <linux/netlink.h>

int main(void) {
    int fd;

    /*
     * obtaining information about sockets
     * @see https://man7.org/linux/man-pages/man7/sock_diag.7.html
     */
    fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);

    if (fd < 0) {
        perror("socket");
        return 1;
    }

    return 0;
}
