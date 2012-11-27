#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "tcp.h"
#include "udp.h"

int main()
{
	tcp_reverse_proxy(9100, "192.168.20.112", 9100);
	//udp_reverse_proxy(161, "192.168.20.112", 161);
}
