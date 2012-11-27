#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct udp_server_thread_info
{
    int server_socket;
    int printer_socket;
    sockaddr_in printer_addr;
};

void sprint_ipv4(char* buf, const sockaddr_in* addr)
{
    sprintf(buf, "%d.%d.%d.%d:%d", 
            (ntohl(addr->sin_addr.s_addr) & 0xff000000) >> 24,
            (ntohl(addr->sin_addr.s_addr) & 0x00ff0000) >> 16,
            (ntohl(addr->sin_addr.s_addr) & 0x0000ff00) >>  8,
            (ntohl(addr->sin_addr.s_addr) & 0x000000ff),
            ntohs(addr->sin_port));
}

int udp_create_socket()
{
    int s = -1;
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("create udp socket error!\n");
        exit(1);
    }
    return s;
}

void udp_bind(int socket, int port)
{
    sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);

    if (bind(socket, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        printf("bind to port %d failure!\n", port);
        exit(1);
    }
    printf("udp bind %d to *:%d\n", socket, port);
}

void pass_data(int from_socket, int to_socket, sockaddr* from_addr, const sockaddr* to_addr)
{
    char buf[1024];
    ssize_t recv_len = 0;
    socklen_t socklen = sizeof(sockaddr_in);

    if ((recv_len = recvfrom(from_socket, buf, 1024, 0, from_addr, &socklen)) < 0) {
        printf("error in recvfrom.\n");
        exit(1);
    }
    if (sendto(to_socket, buf, recv_len, 0, to_addr, sizeof(sockaddr_in)) < 0) {
        printf("error in sendto.\n");
        exit(1);
    }
}

void* udp_server_handler(void* arg)
{
    udp_server_thread_info* pinfo = (udp_server_thread_info *)arg;

    fd_set sockets;
    timeval tv;

    int maxfd = pinfo->server_socket > pinfo->printer_socket ? pinfo->server_socket : pinfo->printer_socket;
    maxfd += 1;

    sockaddr_in host_addr;
    bzero(&host_addr, sizeof(host_addr));

    while (1) {
        FD_ZERO(&sockets);
        FD_SET(pinfo->server_socket, &sockets);
        FD_SET(pinfo->printer_socket, &sockets);
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        switch(select(maxfd, &sockets, NULL, NULL, &tv)) {
            case -1:
                exit(1);
            case 0:
                break;
            default:
                int from_socket, to_socket;
                sockaddr_in *from_addr, *to_addr;
                if (FD_ISSET(pinfo->printer_socket, &sockets)) {
                    from_socket = pinfo->printer_socket;
                    to_socket = pinfo->server_socket;
                    from_addr = &pinfo->printer_addr;
                    to_addr = &host_addr;
                } else {
                    from_socket = pinfo->server_socket;
                    to_socket = pinfo->printer_socket;
                    from_addr = &host_addr;
                    to_addr = &pinfo->printer_addr;
                }
                pass_data(from_socket, to_socket, (sockaddr *) from_addr, (sockaddr *) to_addr);
        }
    }

    delete pinfo;
}

void udp_reverse_proxy(int server_port, const char* printer, int printer_port)
{
    int server_socket = udp_create_socket();
    udp_bind(server_socket, server_port);

    int printer_socket = udp_create_socket();

    hostent* printer_host = gethostbyname(printer);

    sockaddr_in printer_addr;
    bzero((char *)&printer_addr, sizeof(sockaddr_in));
    printer_addr.sin_family = AF_INET;
    bcopy((char *)printer_host->h_addr, (char *)&printer_addr.sin_addr.s_addr, printer_host->h_length);
    printer_addr.sin_port = htons(printer_port);

    udp_server_thread_info* pinfo = new udp_server_thread_info();
    pinfo->server_socket = server_socket;
    pinfo->printer_socket = printer_socket;
    bcopy((char *)&printer_addr, (char *)&pinfo->printer_addr, sizeof(printer_addr));

    pthread_t id = 0;
    int ret = pthread_create(&id, NULL, udp_server_handler, pinfo);
    pthread_join(id, NULL);
}
