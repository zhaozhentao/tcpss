#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/sock_diag.h>
#include <linux/inet_diag.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static int send_query(int fd) {
    struct sockaddr_nl nladdr = {
            .nl_family = AF_NETLINK
    };

    struct {
        struct nlmsghdr nlh;
        struct inet_diag_req_v2 r;
    } req = {
            .nlh = {
                    .nlmsg_len = sizeof(req),
                    .nlmsg_type = SOCK_DIAG_BY_FAMILY,
                    .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP
            },
            .r = {
                    .sdiag_family = AF_INET,
                    .sdiag_protocol = IPPROTO_TCP,
                    .idiag_states = (1 << TCP_ESTABLISHED),
            }
    };

    struct iovec iov = {
            .iov_base = &req,
            .iov_len = sizeof(req)
    };

    struct msghdr msg = {
            .msg_name = &nladdr,
            .msg_namelen = sizeof(nladdr),
            .msg_iov = &iov,
            .msg_iovlen = 1
    };

    for (;;) {
        if (sendmsg(fd, &msg, 0) < 0) {
            if (errno == EINTR)
                continue;

            perror("sendmsg");
            return -1;
        }

        printf("send success\n");
        return 0;
    }
}

const char *format_host(int af, const void *addr) {
    static char buf[256];

    return inet_ntop(af, addr, buf, 256);
}

int print_diag(const struct inet_diag_msg *diag, unsigned int len) {
    if (len < NLMSG_LENGTH(sizeof(*diag))) {
        fputs("short response\n", stderr);
        return -1;
    }

    if (diag->idiag_family != AF_INET) {
        fprintf(stderr, "unexpected family %u\n", diag->idiag_family);
        return -1;
    }

    const char * local = format_host(AF_INET, diag->id.idiag_src);
    const char * peer = format_host(AF_INET, diag->id.idiag_dst);

    printf("local %s peer %s state %d\n", local, peer, diag->idiag_state);

    return 0;
}

int receive_responses(int fd) {
    int flags = 0;
    long buf[8192 / sizeof(long)];
    struct sockaddr_nl nladdr;
    struct iovec iov = {
            .iov_base = buf,
            .iov_len = sizeof(buf)
    };

    for (;;) {
        struct msghdr msg = {
                .msg_name = &nladdr,
                .msg_namelen = sizeof(nladdr),
                .msg_iov = &iov,
                .msg_iovlen = 1
        };

        ssize_t ret = recvmsg(fd, &msg, flags);

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            perror("recvmsg");
            return -1;
        }

        if (ret == 0)
            return 0;

        if (nladdr.nl_family != AF_NETLINK) {
            fputs("!AF_NETLINK\n", stderr);
            return -1;
        }

        const struct nlmsghdr *h = (struct nlmsghdr *) buf;

        if (!NLMSG_OK(h, ret)) {
            fputs("!NLMSG_OK\n", stderr);
            return -1;
        }

        for (; NLMSG_OK(h, ret); h = NLMSG_NEXT(h, ret)) {
            if (h->nlmsg_type == NLMSG_DONE)
                return 0;

            if (h->nlmsg_type == NLMSG_ERROR) {
                const struct nlmsgerr *err = NLMSG_DATA(h);

                if (h->nlmsg_len < NLMSG_LENGTH(sizeof(*err))) {
                    fputs("NLMSG_ERROR\n", stderr);
                } else {
                    errno = -err->error;
                    perror("NLMSG_ERROR");
                }

                return -1;
            }

            if (h->nlmsg_type != SOCK_DIAG_BY_FAMILY) {
                fprintf(stderr, "unexpected nlmsg_type %u\n", (unsigned) h->nlmsg_type);
                return -1;
            }

            if (print_diag(NLMSG_DATA(h), h->nlmsg_len))
                return -1;
        }
    }
}

/*
 * The following example program prints inode number, peer's inode number,
 * and name of all UNIX domain sockets in the current namespace.
 */
int main(void) {
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);

    if (fd < 0) {
        perror("socket");
        return 1;
    }

    send_query(fd);

    receive_responses(fd);

    close(fd);

    return 0;
}
