#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct tcp_server_thread_info
{
    char printer[50];
    int printer_port;
    int accepted_socket;
};

int tcp_create_socket()
{
    int s = -1;
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("create socket error!\n");
        exit(1);
    }
    return s;
}

void tcp_bind_and_listen(int socket, int port)
{
    const int LENGTH_OF_LISTEN_QUEUE = 10;
    sockaddr_in servaddr;

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htons(INADDR_ANY);

    if (bind(socket, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        printf("bind to port %d failure!\n", port);
        exit(1);
    }

    if (listen(socket, LENGTH_OF_LISTEN_QUEUE) < 0) {
        printf("call listen failure!\n");
        exit(1);
    }
}

int tcp_connect_to_printer(const char* printer, int port)
{
    int printer_socket = tcp_create_socket();
    hostent* printer_host = gethostbyname(printer);

    sockaddr_in printer_addr;
    bzero((char *)&printer_addr, sizeof(sockaddr_in));
    printer_addr.sin_family = AF_INET;
    bcopy((char *)printer_host->h_addr, (char *)&printer_addr.sin_addr.s_addr, printer_host->h_length);
    printer_addr.sin_port = htons(port);

    if (connect(printer_socket, (sockaddr *)&printer_addr, sizeof(printer_addr)) < 0) {
        printf("cannot connect to printer\n");
        exit(1);
    }
    return printer_socket;
}

int pass_data(int from_socket, int to_socket)
{
    const int BUF_SIZE = 64 * 1024;
    unsigned char buf[BUF_SIZE];
    int recvbytes = recv(from_socket, buf, BUF_SIZE, 0);
    if (recvbytes == -1) {
        printf("cannot recv data\n");
        exit(1);
    }
    int sentbytes = send(to_socket, buf, recvbytes, 0);
    return sentbytes;
}

int test_socket(int socket)
{
    char buf[1];
    return recv(socket, buf, 1, MSG_PEEK | MSG_DONTWAIT);
}

void* tcp_server_handler(void* arg)
{
    tcp_server_thread_info* pinfo = (tcp_server_thread_info *)arg;
    int printer_socket = tcp_connect_to_printer(pinfo->printer, pinfo->printer_port);

    fd_set sockets;
    timeval tv;

    int maxfd = pinfo->accepted_socket > printer_socket ? pinfo->accepted_socket : printer_socket;
    maxfd += 1;

    int running = 1;

    int total_bytes = 0;

    while (running) {
        FD_ZERO(&sockets);
        FD_SET(pinfo->accepted_socket, &sockets);
        FD_SET(printer_socket, &sockets);

        tv.tv_sec = 3;
        tv.tv_usec = 0;
        

        switch(select(maxfd, &sockets, NULL, NULL, &tv)) {
            case -1:
                printf("error in select.\n");
                exit(1);
            case 0:
                printf("no data.\n");
                break;
            default:
                int from = pinfo->accepted_socket;
                int to = printer_socket;
                if (FD_ISSET(printer_socket, &sockets)) {
                    from = printer_socket;
                    to = pinfo->accepted_socket;
                }
                int err = test_socket(from);
                if (err <= 0 and errno != EAGAIN) {
                    printf("client socket closed.\n");
                    running = 0;
                    break;
                }
                total_bytes += pass_data(from, to);
        }
    }

    printf("thread exit. passed %d bytes in this session.\n", total_bytes);

    close(printer_socket);

    delete pinfo;
}

void tcp_reverse_proxy(int server_port, const char* printer, int printer_port)
{
    int servfd, clifd;
    struct sockaddr_in servaddr, cliaddr;

    servfd = tcp_create_socket();
    tcp_bind_and_listen(servfd, server_port);

    while (1) {
        long timestamp;
        socklen_t length = sizeof(cliaddr);
        clifd = accept(servfd, (struct sockaddr*)&cliaddr, &length);
        if (clifd < 0) {
            printf("error comes when call accept!\n");
            break;
        }
        pthread_t id = 0;
        tcp_server_thread_info* pinfo = new tcp_server_thread_info();
        bzero(pinfo, sizeof(tcp_server_thread_info));

        strcpy(pinfo->printer, printer);
        pinfo->printer_port = printer_port;
        pinfo->accepted_socket = clifd;

        int ret = pthread_create(&id, NULL, tcp_server_handler, pinfo);
        if (ret != 0) {
            printf ("Create pthread error!\n");
            exit (1);
        }
    }
}
