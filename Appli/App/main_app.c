/* USER CODE BEGIN Header */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Application */
#include "cmsis_os2.h"
#include "main.h"
#include "main_app.h"
#include "app_config.h"
#include "projdefs.h"
#include "w61_at_api.h"
#include "w6x_types.h"

#if (LOW_POWER_MODE > LOW_POWER_DISABLE)
#include "utilities_conf.h"
#include "stm32_lpm.h"
#endif /* LOW_POWER_MODE */

#include "w6x_api.h"
#include "spi_iface.h" /* SPI falling/rising_callback */
#include "logging.h"
#include "shell.h"
#include "common_parser.h"
#include "logshell_ctrl.h"
#include "app_freertos.h"
#include "app_networking.h"
#include "FreeRTOS.h"


/* display driver libraries */
#include "ILI9341_STM32_Driver.h"
#include "ili9341_touch.h"
#include "ILI9341_GFX.h"
#if (LOW_POWER_MODE == LOW_POWER_STDBY_ENABLE)
#error "low power standby mode not supported"
#endif /* LOW_POWER_MODE */

#define SERVER

/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Global variables ----------------------------------------------------------*/
/* USER CODE BEGIN GV */
/* USER CODE END GV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macros ------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  Wi-Fi event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  Network event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_net_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  MQTT event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  BLE event callback
  * @param  event_id: Event ID
  * @param  event_args: Event arguments
  */
static void APP_ble_cb(W6X_event_id_t event_id, void *event_args);

/**
  * @brief  W6X error callback
  * @param  ret_w6x: W6X status
  * @param  func_name: function name
  */
static void APP_error_cb(W6X_Status_t ret_w6x, char const *func_name);

/**
  * @brief  Initialize the low power manager
  */
void LowPowerManagerInit(void);

/* Functions Definition ------------------------------------------------------*/
void main_app(void)
{
  W6X_Status_t ret;
  /* USER CODE BEGIN main_app_1 */
  /* USER CODE END main_app_1 */

#if (LOW_POWER_MODE > LOW_POWER_DISABLE)
  LowPowerManagerInit();
#endif /* LOW_POWER_MODE */

  /* Initialize the logging utilities */
  LoggingInit();

  /* USER CODE BEGIN main_app_2 */
  /* USER CODE END main_app_2 */

  /* Register the application callback to received events from ST67W6X Driver */
  W6X_App_Cb_t App_cb = {0};
  App_cb.APP_wifi_cb = APP_wifi_cb;
  App_cb.APP_net_cb = APP_net_cb;
  App_cb.APP_ble_cb = APP_ble_cb;
  App_cb.APP_mqtt_cb = APP_mqtt_cb;
  App_cb.APP_error_cb = APP_error_cb;
  (void)W6X_RegisterAppCb(&App_cb);
  /* Initialize the ST67W6X Driver */
  ret = W6X_Init();
  if (ret != W6X_STATUS_OK)
  {
    LogError("Failed to initialize ST67W6X Driver, %" PRIi32 "\n", ret);
    goto _err;
  }

  /* USER CODE BEGIN main_app_3 */
  ret = W6X_WiFi_Init();
  if(ret != W6X_STATUS_OK) {
    LogError("initialization failure");
    goto _err;
  }

  ret = W6X_Net_Init();
  if(ret != W6X_STATUS_OK) {
    LogError("initialization failure");
    goto _err;
  }

  /* initialize the display */
  ILI9341_Init(); // must be done after kernel initialization to prevent collision with freeRTOS tick priority
  ILI9341_Fill_Screen(WHITE);

#ifdef SERVER
  W6X_WiFi_ApConfig_t ap_config = {0};
  ap_config.Channel = WIFI_SAP_CHANNEL;
  ap_config.Security = WIFI_SAP_SECURITY;
  ap_config.MaxConnections = WIFI_SAP_MAX_CONNECTIONS;
  ap_config.Protocol = W6X_WIFI_PROTOCOL_11AX;
  (void)strncpy((char *)ap_config.SSID, WIFI_SAP_SSID, W6X_WIFI_MAX_SSID_SIZE);
  (void)strncpy((char *)ap_config.Password, WIFI_SAP_PASSWORD, W6X_WIFI_MAX_PASSWORD_SIZE);
  ret = W6X_WiFi_AP_Start(&ap_config);

  if (ret != W6X_STATUS_OK)
  {
    LogError("Failed to start soft-AP, %" PRIi32 "\n", ret);
    goto _err;
  }
  else
  {
    LogInfo("Soft-AP started\n");
  }

  /* create child task that will manage drawing to the display */

  server_init();
#else
  /* client code */
  W6X_WiFi_Connect_Opts_t connect_opts = {0};
  strncpy((char *)connect_opts.SSID, WIFI_SAP_SSID, W6X_WIFI_MAX_SSID_SIZE);
  strncpy((char *)connect_opts.Password, WIFI_SAP_PASSWORD, W6X_WIFI_MAX_PASSWORD_SIZE);

  ret = W6X_WiFi_Connect(&connect_opts);
  if (ret != W6X_STATUS_OK)
  {
    LogError("Failed to join the soft-AP, %" PRIi32 "\n", ret);
    goto _err;
  }

  client_init();
#endif

  /* USER CODE END main_app_3 */
  LogInfo("##### Quitting the application\n");

  /* USER CODE BEGIN main_app_Last */
  /* USER CODE END main_app_Last */

_err:
  /* USER CODE BEGIN main_app_Err_1 */
  /* USER CODE END main_app_Err_1 */
  /* De-initialize the ST67W6X Driver */
  W6X_DeInit();

  /* USER CODE BEGIN main_app_Err_2 */
  /* USER CODE END main_app_Err_2 */
  LogInfo("##### Application end\n");
}

/* coverity[misra_c_2012_rule_5_8_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  /* USER CODE BEGIN EXTI_Rising_Callback_1 */
  /* USER CODE END EXTI_Rising_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (GPIO_Pin == SPI_RDY_Pin)
  {
    (void)spi_on_txn_data_ready();
  }
  /* USER CODE BEGIN EXTI_Rising_Callback_End */
  /* USER CODE END EXTI_Rising_Callback_End */
}

/* coverity[misra_c_2012_rule_5_8_violation : FALSE] */
/* coverity[misra_c_2012_rule_8_6_violation : FALSE] */
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  /* USER CODE BEGIN EXTI_Falling_Callback_1 */
  /* USER CODE END EXTI_Falling_Callback_1 */
  /* Callback when data is available in Network CoProcessor to enable SPI Clock */
  if (GPIO_Pin == SPI_RDY_Pin)
  {
    (void)spi_on_header_ack();
  }
  /* USER CODE BEGIN EXTI_Falling_Callback_End */
  /* USER CODE END EXTI_Falling_Callback_End */
}

/* USER CODE BEGIN FD */
/* USER CODE END FD */

/* Private Functions Definition ----------------------------------------------*/
static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args)
{
  W6X_WiFi_CbParamData_t *cb_data;

  switch (event_id)
  {
    case W6X_WIFI_EVT_CONNECTED_ID:
      break;

    case W6X_WIFI_EVT_DISCONNECTED_ID:
      break;

    case W6X_WIFI_EVT_REASON_ID:
      LogInfo("Reason: %s\n", W6X_WiFi_ReasonToStr(event_args));
      break;

    case W6X_WIFI_EVT_DIST_STA_IP_ID:
      cb_data = (W6X_WiFi_CbParamData_t *)event_args;
      LogInfo("Station connected to soft-AP + IP : [" MACSTR "] IP: " IPSTR "\n",
              MAC2STR(cb_data->MAC), IP2STR(cb_data->IP));
      break;

    case W6X_WIFI_EVT_STA_CONNECTED_ID:
      cb_data = (W6X_WiFi_CbParamData_t *)event_args;
      LogInfo("Station connected to soft-AP : [" MACSTR "]\n", MAC2STR(cb_data->MAC));
      break;

    case W6X_WIFI_EVT_STA_DISCONNECTED_ID:
      cb_data = (W6X_WiFi_CbParamData_t *)event_args;
      LogInfo("Station disconnected from soft-AP : [" MACSTR "]\n", MAC2STR(cb_data->MAC));
      break;

    default:
      /* Wi-Fi events unmanaged */
      break;
  }
}

static void APP_net_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_net_cb_1 */
   W6X_Net_CbParamData_t *p_param_app_net_cb;

  switch (event_id)
  {
    case W6X_NET_EVT_SOCK_DATA_ID:
      p_param_app_net_cb = (W6X_Net_CbParamData_t *) event_args;
      LogInfo(" Cb informed app that Wi-Fi %" PRIu32 " bytes available on socket %" PRIu32 ".\n",
              p_param_app_net_cb->available_data_length, p_param_app_net_cb->socket_id);
      break;

    default:
      /* Net events unmanaged */
      break;
  }
  /* USER CODE END APP_net_cb_1 */
}

static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_mqtt_cb_1 */
  /* USER CODE END APP_mqtt_cb_1 */
}

static void APP_ble_cb(W6X_event_id_t event_id, void *event_args)
{
  /* USER CODE BEGIN APP_ble_cb_1 */
  /* USER CODE END APP_ble_cb_1 */
}

static void APP_error_cb(W6X_Status_t ret_w6x, char const *func_name)
{
  /* USER CODE BEGIN APP_error_cb_1 */
  /* USER CODE END APP_error_cb_1 */
  LogError("[%s] in %s API\n", W6X_StatusToStr(ret_w6x), func_name);
  /* USER CODE BEGIN APP_error_cb_2 */
  /* USER CODE END APP_error_cb_2 */
}

void LowPowerManagerInit(void)
{
  /* USER CODE BEGIN LowPowerManagerInit_1 */
  /* USER CODE END LowPowerManagerInit_1 */

#if (LOW_POWER_MODE > LOW_POWER_DISABLE)
  /* Init low power manager */
  UTIL_LPM_Init();

  /* USER CODE BEGIN LowPowerManagerInit_2 */
  /* USER CODE END LowPowerManagerInit_2 */

#if (DEBUGGER_ENABLED == 1)
  HAL_DBGMCU_EnableDBGStopMode();
  HAL_DBGMCU_EnableDBGStandbyMode();
#else
  HAL_DBGMCU_DisableDBGStopMode();
  HAL_DBGMCU_DisableDBGStandbyMode();
#endif /* DEBUGGER_ENABLED */

  /* USER CODE BEGIN LowPowerManagerInit_3 */
  /* USER CODE END LowPowerManagerInit_3 */

#if (LOW_POWER_MODE < LOW_POWER_STDBY_ENABLE)
  /* Disable Stand-by mode */
  UTIL_LPM_SetOffMode((1 << CFG_LPM_APPLI_ID), UTIL_LPM_DISABLE);
#endif /* LOW_POWER_MODE */
#if (LOW_POWER_MODE < LOW_POWER_STOP_ENABLE)
  /* Disable Stop Mode */
  UTIL_LPM_SetStopMode((1 << CFG_LPM_APPLI_ID), UTIL_LPM_DISABLE);
#endif /* LOW_POWER_MODE */
#endif /* LOW_POWER_MODE */

  /* USER CODE BEGIN LowPowerManagerInit_End */
  /* USER CODE END LowPowerManagerInit_End */
}

/* USER CODE BEGIN PFD */
/* USER CODE END PFD */
