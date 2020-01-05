#include <string.h>

#include "lwip.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include <stm32f4xx_hal_gpio.h>
#include <stm32f4xx_hal_rtc.h>

#include "sync.h"
#include "signaling_diode.h"

#define SNTP_ADDR "192.168.0.88"

SemaphoreHandle_t connectionEnableSemaphore;


void hang() {
        SD_SetState(FAILURE);
        for (;;) {
                osDelay(420);
        }
}

void SY_TaskFunc(void* param){
        SD_SetState(CONNECTING);
        
        printf("Waiting for LWIP to be initialized...");
        if (xSemaphoreTake(connectionEnableSemaphore, 30000) != pdTRUE){
                hang();
        };
        printf(" done \r\n");


        struct netconn* conn;
        struct ip4_addr server_ip;
        int result, send_result, recv_result;
        u16_t recv_size;
        char text[] = "Hello there!";
        char* recv_data;
        struct netbuf *sendbuf, *recvbuf;

        ipaddr_aton(SNTP_ADDR, &server_ip); 
        
        printf("creating new LWIP connection...");
        conn = netconn_new(NETCONN_UDP);
        if (conn == NULL) {
                printf(" Connection error\r\n");
                hang();
        }
        printf(" done \r\n");

        printf("connecting to %s...", SNTP_ADDR);
        if (result = netconn_connect(conn, &server_ip, 2137) != ERR_OK) {
                printf(" error %d\r\n", result);
                hang();
        }
        printf(" done \r\n");
        SD_SetState(CONNECTED);

        sendbuf = netbuf_new();
        netbuf_ref(sendbuf, text, sizeof(text));

        while (1) {
                if (HAL_GPIO_ReadPin(GPIOC, USER_Btn_Pin)) {
                        printf("Synchronizing time... \r\n");
                        send_result = netconn_send(conn, sendbuf);
                        // HAL_RTC_GetTime
                        if (send_result != ERR_OK) {
                                printf("Sending error %d\r\n", send_result);
                                hang();
                        } 
                        printf("UDP message sent \r\n");
                        osDelay(100);

                        recv_result = netconn_recv(conn, &recvbuf);
                        if (recv_result != ERR_OK) {
                                printf("Receiving error %d\r\n", recv_result);
                                hang();
                        } 
                        printf("UDP message recieved \r\n");
                        netbuf_data(recvbuf, (void**) &recv_data, &recv_size);
                        printf("size %d; message %s\r\n", recv_size, recv_data);
                        // netconn_delete(sendbuf);
                        netbuf_delete(recvbuf);
                }

                osDelay(200); 
        }

        /* deallocate connection and netbuf */
        netconn_delete(conn);

        for(;;) {
                osDelay(420);   
        }
}