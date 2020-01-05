#include <string.h>

#include "lwip.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/ip_addr.h"
#include <stm32f4xx_hal_gpio.h>
#include <stm32f4xx_hal_rtc.h>

#include <time.h>
#include "sync.h"
#include "signaling_diode.h"

#define SNTP_ADDR "192.168.0.88"
#define SNTP_PORT 123

SemaphoreHandle_t connectionEnableSemaphore;
extern RTC_HandleTypeDef hrtc;

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
        SNTP_Frame client_sntp_frame, server_sntp_frame;
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
        if (result = netconn_connect(conn, &server_ip, SNTP_PORT) != ERR_OK) {
                printf(" error %d\r\n", result);
                hang();
        }
        printf(" done \r\n");
        SD_SetState(CONNECTED);

        sendbuf = netbuf_new();
        client_sntp_frame.header = 3;
        netbuf_ref(sendbuf, &client_sntp_frame, sizeof(client_sntp_frame));

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
                        netbuf_data(recvbuf, (void**) &server_sntp_frame, &recv_size);

                        RTC_DateTypeDef date;
                        date = SNTP_TimestampToDate(server_sntp_frame.reference_timestamp);
                        HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN);

                        RTC_TimeTypeDef time;
                        time = SNTP_TimestampToTime(server_sntp_frame.reference_timestamp);
                        HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN);

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

RTC_DateTypeDef SNTP_TimestampToDate(SNTP_Timestamp timestamp) {
        struct tm* datetime;
        time_t seconds_time_t = timestamp.seconds;
        datetime = localtime(&seconds_time_t);

        RTC_DateTypeDef rtc_date;
        rtc_date.Date = datetime->tm_mday;
        rtc_date.Month = datetime->tm_mon;
        rtc_date.Year = datetime->tm_year % 100;
        rtc_date.WeekDay = datetime->tm_wday;
        return rtc_date;
}

RTC_TimeTypeDef SNTP_TimestampToTime(SNTP_Timestamp timestamp) {
        struct tm* datetime;
        time_t seconds_time_t = timestamp.seconds;
        datetime = localtime(&seconds_time_t);

        RTC_TimeTypeDef rtc_time;
        rtc_time.Hours = datetime->tm_hour;
        rtc_time.Minutes = datetime->tm_min;
        rtc_time.Seconds = datetime->tm_sec;
        rtc_time.SubSeconds = timestamp.seconds_fraction;
        rtc_time.SecondFraction = 0xFFFFFFFF;
        return rtc_time;
}