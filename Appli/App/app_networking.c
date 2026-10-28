// based on the http server provided by ST Microelectronics
// https://wiki.st.com/stm32mcu/wiki/Connectivity:Wi-Fi_ST67W6X_HTTP_Server_Application
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include "cmsis_os2.h"
#include "logging.h"
#include "main.h"
#include "app_networking.h"

#include "projdefs.h"
#include "w6x_api.h"
#include "common_parser.h" /* Common Parser functions */

#include "app_config.h"
#include "app_freertos.h"
#include "event_groups.h"
#include "ili9341_touch.h"
#include "ILI9341_STM32_Driver.h"
#include "ILI9341_GFX.h"
#include "w6x_types.h"

static void wireless_screen_update_task(void *arg);

static void display_driver_task(void *arg);

static void mirror(int32_t connection, char *recv_buffer);

int sockopts(int32_t sock, int32_t timeout);

/* provides thread safety for the display driver */
osMutexId_t mutex;

void server_init() {
  const int32_t domain = AF_INET;
  int32_t sock = -1;
  struct sockaddr_in s_addr_in_t = { 0 };
  uint16_t port = PORT;
  uint8_t ip_address[4] = {0};
  uint8_t netmask_addr[4] = {0};
  int32_t timeout = (int32_t)pdMS_TO_TICKS(SOCKET_TIMEOUT_MS);
  
  /* initialize the mutex for guarding display-writes */
  mutex = osMutexNew(NULL);

  /* Get the soft-AP current IP address */
  if (W6X_Net_AP_GetIPAddress(ip_address, netmask_addr) != W6X_STATUS_OK)
  {
    LogError("Get soft-AP IP failed\n");
    goto _err;
  }

  LogInfo("Soft-AP IP address : " IPSTR "\n", IP2STR(ip_address));
  s_addr_in_t.sin_addr.s_addr = ATON(ip_address);
  s_addr_in_t.sin_port = PP_HTONS(port);
  s_addr_in_t.sin_family = AF_INET;

  /* Create a new TCP server socket with port HTTP_PORT */
  sock = W6X_Net_Socket(domain, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0)
  {
    LogInfo("Could not create the socket.\n");
    goto _err;
  }

  if(sockopts(sock, timeout) != 0) {
    goto _err; // error logging is already done in the function called
  }

  /* Bind the socket to the server address */
  if (W6X_Net_Bind(sock, (struct sockaddr *)&s_addr_in_t, sizeof(s_addr_in_t)) != 0)
  {
    LogInfo("\n LwIP Bind Fail\n");
    goto _err;
  }

  LogInfo("\n LwIP Bind Pass\n");

  /* Listen for incoming connections (TCP listen backlog = 5). */
  if (W6X_Net_Listen(sock, 5) != 0)
  {
    LogInfo("\n LwIP Listen Fail\n");
    goto _err;
  }

  LogInfo("\n LwIP Listen Pass\n");

  while (true)
  {
    struct sockaddr remotehost_t;
    uint32_t remotehost_size = sizeof(remotehost_t);

    /* Wait for an incoming client connection */
    int32_t newconn = W6X_Net_Accept(sock, (struct sockaddr *)&remotehost_t, (socklen_t *)&remotehost_size);
    if (newconn > 0)
    {
      /* Create a thread to process the incoming screen update */
      xTaskCreate((TaskFunction_t)display_driver_task, NULL,
            CHILD_TASK_STACK_SIZE >> 2U,
            &newconn, 10, NULL);  

      xTaskCreate((TaskFunction_t)wireless_screen_update_task, NULL,
                                CHILD_TASK_STACK_SIZE >> 2U,
                                &newconn, 11, NULL);
      
      /* Delay added to avoid that too many requests are processed in parallel */
      vTaskDelay(pdMS_TO_TICKS(50));
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(200));
      LogInfo("\n Failed to accept new client requests.\n");
    }
  }
  
  /* USER CODE BEGIN http_server_socket_2 */

  /* USER CODE END http_server_socket_2 */

  _err:
  /* Error case */
  if (sock >= 0)
  {
    if (W6X_Net_Shutdown(sock, 1) != 0)
    {
      LogError("Failed to close server socket\n");
    }
  }
  return;
}

void client_init() {
  int32_t ret;
  int sock = -1;
  struct sockaddr_in addr = {0};
  uint8_t server_ip[] = {10, 19, 96, 1}; // specified to always be the same by the server
  addr.sin_family = AF_INET;
  addr.sin_port = PP_HTONS(PORT);
  addr.sin_addr.s_addr = ATON(server_ip);
  int32_t timeout = (int32_t)pdMS_TO_TICKS(SOCKET_TIMEOUT_MS);

  /* initialize the mutex for guarding display-writes */
  mutex = osMutexNew(NULL);

  /* create the socket */
  sock = W6X_Net_Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // needs error checking and sockopts set
  
  if(sockopts(sock, timeout) != 0) {
    goto _err; // error logging is already done in the function called
  }
 
  /* connect to the soft-AP */
  ret = W6X_Net_Connect(sock, (struct sockaddr *)&addr, sizeof(addr));
  if(ret != 0) {
    LogError("client failed to connect to soft-AP" PRIi32 "\n", ret);
    goto _err;
  }

  /* create the tasks for managing display mirroring over wifi */
  xTaskCreate((TaskFunction_t)display_driver_task, NULL,
            CHILD_TASK_STACK_SIZE >> 2U,
            &sock, 10, NULL); 
            
  xTaskCreate((TaskFunction_t)wireless_screen_update_task, NULL,
            CHILD_TASK_STACK_SIZE >> 2U,
            &sock, 11, NULL);

  while(1) vTaskDelay(pdMS_TO_TICKS(1000)); // replace with task notification
  _err:
    /* clean up */
    W6X_Net_DeInit();

    W6X_WiFi_DeInit();

    W6X_DeInit();

    LogError("client initialization failed");
}

static void wireless_screen_update_task(void *arg) {
  int32_t connection = *((int32_t *)arg);
  int32_t bytes_received;

  ssize_t recv_buffer_len = CHILD_TASK_BUFFER_SIZE;
  char *recv_buffer = (char *)pvPortMalloc(recv_buffer_len);
  if (recv_buffer == NULL)
  {
    LogError("Unable to allocate recv buffer\n");
    goto _err;
  }

  /* Read in the request */
  recv_buffer[0] = 0;
  do
  {
    bytes_received = W6X_Net_Recv(connection, (uint8_t *)recv_buffer, 4, 0);
    if (bytes_received <= 0) /* No data received or error */
    {
      continue;
    }
    
    mirror(connection, recv_buffer);      
  } while (true);
    
  _err:
    vPortFree(recv_buffer);

    (void)W6X_Net_Close(connection);

    vTaskDelete(NULL);

}

static void display_driver_task(void *arg) {
  int32_t connection = *((int32_t *)arg);
  uint16_t x, y;
  uint16_t tx_draw_data[2];

  while (1)
  {
    if(t_pressed()) {
      /* get the input coordinates and save them in the transfer buffer */
      t_getcoordinates(&x, &y);
      tx_draw_data[0] = x;
      tx_draw_data[1] = y;

      /* acquire a lock here: display driver is NOT thread safe */
      if (osMutexAcquire(mutex, osWaitForever) != osOK) continue;

      ILI9341_Draw_Rectangle(x, y, 2, 2, PINK); // draw where the input is detected
      
      /* release it: only display writes need to be guarded */
      osMutexRelease(mutex);

      if (W6X_Net_Send(connection, (uint8_t *)tx_draw_data, 4, 0) == -1) {
        //W6X_Net_GetConnectionStatus(uint8_t socket, uint8_t *protocol, uint32_t *remote_ip, uint32_t *remote_port, uint32_t *local_port, uint8_t *type)
      }
    } else vTaskDelay(pdMS_TO_TICKS(100)); // yield to prevent collisions with higher priority tasks
  }    
}

static void mirror(int32_t conection, char *recv_buffer) {
  uint16_t x_value;    
  uint16_t y_value;

  uint16_t *screen_data = (uint16_t*)recv_buffer;
  x_value = screen_data[0];
  y_value = screen_data[1];

  /* ensure mutual exlcusion with the display driver task */
  if (osMutexAcquire(mutex, osWaitForever) == osOK) {

    ILI9341_Draw_Rectangle(x_value, y_value, 2, 2, PINK);
      
    /* release immediately so the display driver task can resume */
    osMutexRelease(mutex);
  }
}

int sockopts(int32_t sock, int32_t timeout) {
  
  /* Set the socket Receive timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)) != 0)
  {
    LogError("Could not set the socket options.\n");
    return -1;
  }

  /* Set the socket Send timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout)) != 0)
  {
    LogError("Could not set the socket options.\n");
    return -1;
  }

  /* Set the socket Send timeout option */
  if (W6X_Net_Setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&timeout, sizeof(timeout)) != 0)
  {
    LogError("Could not set the socket options.\n");
    return -1;
  }
  return 0;
}