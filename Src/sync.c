#include <string.h>

#include "lwip.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#include "sync.h"
#include "signaling_diode.h"

void hang() {
        SD_SetState(FAILURE);
        for (;;) {}
}

void SY_TaskFunc(void* param){
        SD_SetState(CONNECTING);
        
        MX_LWIP_Init(); // todo: decancerify
        int sock;
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) hang();

        int status;
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        struct addrinfo* addr;
        if (getaddrinfo("192.168.1.188", "2137", &hints, &addr) != 0) {
                hang();
        }

        if (connect(sock, addr->ai_addr, addr->ai_addrlen) != 0) {
                hang();
        }

        SD_SetState(CONNECTED);

        for(;;) {
        }
}