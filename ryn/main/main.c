// how to set eclipse with gdb.
// https://esp32.com/viewtopic.php?f=13&t=336&hilit=gdb&start=10

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "esp_attr.h"
#include <time.h>
#include <sys/time.h>
#include "lwip/err.h"
#include "apps/sntp/sntp.h"

#include <errno.h>
#include <sys/fcntl.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "spiffs_vfs.h"
#include <math.h>
#include "test1_html.h"

#define MG_ENABLE_FILESYSTEM  1

#include "mongoose.h"
//#include "web.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/dac.h"
#include "rom/ets_sys.h"
#include "driver/timer.h"
#include "soc\sens_reg.h"

#include "define.h"
//#include "bmw8bit8k.h"
//#include "reduced_bmw8bit8K.h"
//#include "ingreemakefield.h"
//#include "ingress04.h"
//#include "adrees_update_done.h"
//#include "addrdone2.h"
#include "welcome.h"


#define  SPIFFS_PATH 		"/spiffs/images"
#define  TO_SPIFFS_PATH(s)  SPIFFS_PATH#s

#define WEB_ROOT 				"/spiffs/images"
#define WEB_ROOT_INDEX_FILE  	"update.shtml"

TaskHandle_t xHandleMongoose = NULL;
TaskHandle_t xHandleDmxFrame = NULL;

uint8_t bStationConnected = 0;


typedef struct {
	char old_address[8];
	char new_address[8];
	char target_temp[8];
	char adc[8];
	char brightness[8];
} user_params_t;

typedef enum {
	PUSH_IDLE 					= 0x00,
    PUSH_PLAYSCENE 				= 0x01,
    PUSH_ADDRESS_UPDATE			= 0x02,
	PUSH_ADDRESS_TARGET_UPDATE	= 0x04,
	PUSH_CONFIG_UPDATE          = 0x08,
	PUSH_TEST                   = 0x10,
} dmx_push_state_t;


static const char tag[] = "[SPIFFS example]";

struct device_settings {
  char setting1[100];
  char setting2[100];
};


struct device_settings s_settings = {"value1", "value2"};

static uint32_t usec = 40;
static uint8_t bPlay = 0;

void mongooseTask(void *data);


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch (event->event_id) {
	case SYSTEM_EVENT_AP_START:
		printf("Accesspoint connected\n");
		xTaskCreatePinnedToCore(&mongooseTask, "mongooseTask", 80000, NULL, 5, &xHandleMongoose, 0);
		break;

	case SYSTEM_EVENT_AP_STOP:
		printf("Accesspoint stopped\n");
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
		bStationConnected++;
		printf("Accesspoint Station connected\n");
		break;

	case SYSTEM_EVENT_AP_STADISCONNECTED:
		bStationConnected--;
		printf("Accesspoint Station disconnected\n");
		break;

	case SYSTEM_EVENT_AP_PROBEREQRECVED:
		printf("Accesspoint Probe Request received\n");
		break;

	default:
		break;
	}
	return ESP_OK;

}

//-------------------------------
static void initialize_wifi(void)
{
	tcpip_adapter_init();
	tcpip_adapter_ip_info_t info = { 0, };
	IP4_ADDR(&info.ip, 192, 168, 1, 3);
	IP4_ADDR(&info.gw, 192, 168, 1, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	wifi_config_t wifi_config;
	memset(&wifi_config, 0, sizeof(wifi_config));
	strcpy((char *)wifi_config.ap.ssid, "RynAP1004");
	wifi_config.ap.ssid_len = strlen((char *)wifi_config.ap.ssid);
	wifi_config.ap.max_connection = 4;

	wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
	strcpy((char*)wifi_config.ap.password, "1234567890");

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
}



//mg_serve_http_opts s_opts;
//extern struct mg_serve_http_opts s_opts;
//extern struct user_param_t params;

user_params_t params = { "0", "1", "90", "50", "90" };
struct mg_serve_http_opts s_opts;



// Mongoose event handler.
char *mongoose_eventToString(int ev);
char *mgStrToStr(struct mg_str mgStr);
void handle_save(struct mg_connection *nc, struct http_message *hm);
void handle_get_cpu_usage(struct mg_connection *nc);
void handle_ssi_call(struct mg_connection *nc, const char *param);
void mongoose_event_handler(struct mg_connection *nc, int ev, void *evData);


static void push_data_to_all_websocket_connections(struct mg_mgr *m) {
  struct mg_connection *c;
  int memory_usage = (double) rand() / RAND_MAX * 100.0;

  for (c = mg_next(m, NULL); c != NULL; c = mg_next(m, c)) {
    if (c->flags & MG_F_IS_WEBSOCKET) {
      mg_printf_websocket_frame(c, WEBSOCKET_OP_TEXT, "%d", memory_usage);
    }
  }
}




void mongooseTask(void *data) {

	struct mg_mgr mgr;
	mg_mgr_init(&mgr, NULL);
	struct mg_connection *nc = mg_bind(&mgr, ":80", mongoose_event_handler);
	if (nc == NULL) {
		printf("No connection from the mg_bind()\n");
		vTaskDelete(NULL);
		return;
	}

	mg_set_protocol_http_websocket(nc);

	memset(&s_opts, 0, sizeof(s_opts));

	s_opts.document_root = WEB_ROOT;
	s_opts.index_files = WEB_ROOT_INDEX_FILE;

	while (1) {

	    static time_t last_time;
	    time_t now = time(NULL);
	    mg_mgr_poll(&mgr, 1000);
#if 0
	    if (now - last_time > 0) {
	      push_data_to_all_websocket_connections(&mgr);
	      last_time = now;
	    }
#endif
		//mg_mgr_poll(&mgr, 200);
	}
	mg_mgr_free(&mgr);



} // mongooseTask


char *mongoose_eventToString(int ev) {
	static char temp[100];
	switch (ev) {
	case MG_EV_CONNECT:
		return "MG_EV_CONNECT";
	case MG_EV_ACCEPT:
		return "MG_EV_ACCEPT";
	case MG_EV_CLOSE:
		return "MG_EV_CLOSE";
	case MG_EV_SEND:
		return "MG_EV_SEND";
	case MG_EV_RECV:
		return "MG_EV_RECV";
	case MG_EV_HTTP_REQUEST:
		return "MG_EV_HTTP_REQUEST";
	case MG_EV_HTTP_REPLY:
		return "MG_EV_HTTP_REPLY";
	case MG_EV_MQTT_CONNACK:
		return "MG_EV_MQTT_CONNACK";
	case MG_EV_MQTT_CONNACK_ACCEPTED:
		return "MG_EV_MQTT_CONNACK";
	case MG_EV_MQTT_CONNECT:
		return "MG_EV_MQTT_CONNECT";
	case MG_EV_MQTT_DISCONNECT:
		return "MG_EV_MQTT_DISCONNECT";
	case MG_EV_MQTT_PINGREQ:
		return "MG_EV_MQTT_PINGREQ";
	case MG_EV_MQTT_PINGRESP:
		return "MG_EV_MQTT_PINGRESP";
	case MG_EV_MQTT_PUBACK:
		return "MG_EV_MQTT_PUBACK";
	case MG_EV_MQTT_PUBCOMP:
		return "MG_EV_MQTT_PUBCOMP";
	case MG_EV_MQTT_PUBLISH:
		return "MG_EV_MQTT_PUBLISH";
	case MG_EV_MQTT_PUBREC:
		return "MG_EV_MQTT_PUBREC";
	case MG_EV_MQTT_PUBREL:
		return "MG_EV_MQTT_PUBREL";
	case MG_EV_MQTT_SUBACK:
		return "MG_EV_MQTT_SUBACK";
	case MG_EV_MQTT_SUBSCRIBE:
		return "MG_EV_MQTT_SUBSCRIBE";
	case MG_EV_MQTT_UNSUBACK:
		return "MG_EV_MQTT_UNSUBACK";
	case MG_EV_MQTT_UNSUBSCRIBE:
		return "MG_EV_MQTT_UNSUBSCRIBE";
	case MG_EV_WEBSOCKET_HANDSHAKE_REQUEST:
		return "MG_EV_WEBSOCKET_HANDSHAKE_REQUEST";
	case MG_EV_WEBSOCKET_HANDSHAKE_DONE:
		return "MG_EV_WEBSOCKET_HANDSHAKE_DONE";
	case MG_EV_WEBSOCKET_FRAME:
		return "MG_EV_WEBSOCKET_FRAME";
	}
	sprintf(temp, "Unknown event: %d", ev);
	return temp;
} //eventToString



char *mgStrToStr(struct mg_str mgStr) {
	char *retStr = (char *) malloc(mgStr.len + 1);
	memcpy(retStr, mgStr.p, mgStr.len);
	retStr[mgStr.len] = 0;
	return retStr;
} // mgStrToStr

void handle_save(struct mg_connection *nc, struct http_message *hm) {
  // Get form variables and store settings values

  mg_get_http_var(&hm->body, "setting1", s_settings.setting1,
                  sizeof(s_settings.setting1));
  mg_get_http_var(&hm->body, "setting2", s_settings.setting2,
                  sizeof(s_settings.setting2));
  printf("s_settings.setting1=%s", s_settings.setting1);
  printf("s_settings.setting1=%s", s_settings.setting2);


  // Send response
  mg_http_send_redirect(nc, 302, mg_mk_str("/"), mg_mk_str(NULL));

}

//update_alladdress
void update_address(struct mg_connection *nc, struct http_message *hm) {
  // Get form variables and store settings values

  mg_get_http_var(&hm->body, "old_address", params.old_address ,
                  sizeof(params.old_address));
  mg_get_http_var(&hm->body, "new_address", params.new_address,
                  sizeof(params.new_address));

  char ref[32] = {0};
  mg_get_http_var(&hm->body, "ref", ref, sizeof(ref));


  printf("ref=%s\r\n",  ref);
  printf("old_address=%s\r\n",  params.old_address);
  printf("new_address=%s\r\n",  params.new_address);



  uint32_t ulNotifiedValue;

  if(strcmp(ref,"all") == 0)
  {

	  printf("xTaskNotifer handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotify(xHandleDmxFrame, PUSH_ADDRESS_UPDATE, eSetValueWithOverwrite);
	  printf("xTaskNotifyWait handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

	  mg_http_send_redirect(nc, 302, mg_mk_str("/update.shtml"), mg_mk_str(NULL));

  } else {


	  printf("xTaskNotifer handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotify(xHandleDmxFrame, PUSH_ADDRESS_TARGET_UPDATE, eSetValueWithOverwrite);
	  printf("xTaskNotifyWait handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

	  mg_http_send_redirect(nc, 302, mg_mk_str("/update_target.shtml"), mg_mk_str(NULL));
  }

}


void update_config(struct mg_connection *nc, struct http_message *hm) {
  // Get form variables and store settings values

  mg_get_http_var(&hm->body, "temperature", params.target_temp ,
                  sizeof(params.target_temp));
  mg_get_http_var(&hm->body, "brightness", params.brightness,
                  sizeof(params.brightness));

  mg_get_http_var(&hm->body, "adc", params.adc, sizeof(params.adc));

  char ref[32] = {0};
  mg_get_http_var(&hm->body, "ref", ref, sizeof(ref));


  printf("ref=%s\r\n",  ref);
  printf("adc=%s\r\n",  params.adc);
  printf("temperature=%s\r\n", params.target_temp);
  printf("brightness=%s\r\n",  params.brightness);

  uint32_t ulNotifiedValue;

  if(strcmp(ref,"temperature_set") == 0)
  {
	  printf("update_config xTaskNotifer handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotify(xHandleDmxFrame, PUSH_CONFIG_UPDATE, eSetValueWithOverwrite);
	  printf("update_config xTaskNotifyWait handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

	  mg_http_send_redirect(nc, 302, mg_mk_str("/config.shtml"), mg_mk_str(NULL));
  } else {


	  /*
	  printf("xTaskNotifer handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotify(xHandleDmxFrame, PUSH_ADDRESS_TARGET_UPDATE, eSetValueWithOverwrite);
	  printf("xTaskNotifyWait handle=%x\r\n", (int)xHandleMongoose);
	  xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

	  mg_http_send_redirect(nc, 302, mg_mk_str("/update_target.shtml"), mg_mk_str(NULL));
	  */
  }

}


void testdmx(struct mg_connection *nc, struct http_message *hm) {
  // Get form variables and store settings values

  uint32_t ulNotifiedValue;
  printf("testdmx xTaskNotifer handle=%x\r\n", (int)xHandleMongoose);
  xTaskNotify(xHandleDmxFrame, PUSH_TEST, eSetValueWithOverwrite);
  printf("testdmx xTaskNotifyWait handle=%x\r\n", (int)xHandleMongoose);
  xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);
  mg_http_send_redirect(nc, 302, mg_mk_str("/config.shtml"), mg_mk_str(NULL));

}


void handle_get_cpu_usage(struct mg_connection *nc) {
  printf("handle_get_cpu_usage\r\n");
  // Generate random value, as an example of changing CPU usage
  // Getting real CPU usage depends on the OS.
  int cpu_usage = (double) rand() / RAND_MAX * 100.0;

  // Use chunked encoding in order to avoid calculating Content-Length
  mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");

  // Output JSON object which holds CPU usage data
  mg_printf_http_chunk(nc, "{ \"result\": %d }", cpu_usage);

  // Send empty chunk, the end of response
  mg_send_http_chunk(nc, "", 0);
}

void handle_ssi_call(struct mg_connection *nc, const char *param) {

  if (strcmp(param, "old_address") == 0) {
    mg_printf_html_escape(nc, "%s", params.old_address);
  } else if (strcmp(param, "new_address") == 0) {
    mg_printf_html_escape(nc, "%s",  params.new_address);
  } else if (strcmp(param, "temperature") == 0) {
	    mg_printf_html_escape(nc, "%s",  params.target_temp);
  } else if (strcmp(param, "brightness") == 0) {
	    mg_printf_html_escape(nc, "%s",  params.brightness);
  }

}




// Mongoose event handler.
void mongoose_event_handler(struct mg_connection *nc, int ev, void *evData) {
	struct http_message *hm = (struct http_message *) evData;


	switch (ev) {
	case MG_EV_HTTP_REQUEST:
	  if (mg_vcmp(&hm->uri, "/save") == 0) {
		printf("mongoose_event_handler::handle_save\r\n");
		handle_save(nc, hm);
	  } else if (mg_vcmp(&hm->uri, "/update_address") == 0) {
		printf("mongoose_event_handler::update_address\r\n");
		update_address(nc, hm);

	  } else if (mg_vcmp(&hm->uri, "/update_config") == 0) {
		printf("mongoose_event_handler::update_config\r\n");
		update_config(nc, hm);

	  }else if (mg_vcmp(&hm->uri, "/get_cpu_usage") == 0) {
		handle_get_cpu_usage(nc);

	  } else if (mg_vcmp(&hm->uri, "/update.shtml") == 0) {
		printf("update.shtml\r\n");
		strcpy(params.old_address, "0");
		mg_serve_http(nc, hm, s_opts);

	  } else if (mg_vcmp(&hm->uri, "/update_target.shtml") == 0) {
		printf("update_target.shtml\r\n");
		strcpy(params.old_address, params.new_address);
		mg_serve_http(nc, hm, s_opts);

	  } else if (mg_vcmp(&hm->uri, "/testdmx") == 0) {
			printf("test\r\n");

			testdmx(nc, hm);
			//mg_serve_http(nc, hm, s_opts);
	  } else {
		printf("static contents : %s\r\n", mgStrToStr(hm->uri));
		mg_serve_http(nc, hm, s_opts);  // Serve static content

	  }
	  break;
	case MG_EV_SSI_CALL:
	  //printf("mongoose_event_handler::MG_EV_SSI_CALL\r\n");
	  handle_ssi_call(nc, evData);
	  break;
	default:
	  break;
	}
} // End of mongoose_event_handler



static void initialize_dmx()
{

	uart_config_t myUartConfig;
	myUartConfig.baud_rate = 250000;
	myUartConfig.data_bits = UART_DATA_8_BITS;
	myUartConfig.parity = UART_PARITY_DISABLE;
	myUartConfig.stop_bits = UART_STOP_BITS_2;
	myUartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
	myUartConfig.rx_flow_ctrl_thresh = 120;

	uart_param_config(UART_NUM_2, &myUartConfig);
	uart_driver_install(UART_NUM_2, 2048, 0, 10, NULL, 0);
}


uint8_t dmx_push_state_idx = PUSH_IDLE;

void dmx_break(void)
{
	gpio_config_t gpioConfig;
	gpioConfig.pin_bit_mask = (1 << GPIO_NUM_17);
	gpioConfig.mode = GPIO_MODE_OUTPUT;
	gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
	gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
	gpioConfig.intr_type = GPIO_INTR_DISABLE;
	gpio_config(&gpioConfig);
	gpio_set_level(GPIO_NUM_17, 0);
	vTaskDelay(1);

	uart_set_pin(UART_NUM_2, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void send_protocol_id(uint8_t id)
{
	for(int i = 0; i<PROTOCOL_KEY_SIZE; ++i)
	{
		dmx_break();
		uart_write_bytes(UART_NUM_2, (const char*)&const_protocols[id][i], 1);
		vTaskDelay(10);
	}
}

void DmxFrameTask(void *pv)
{
	uint32_t ulNotifiedValue;

	while (1) {


		printf("DmxFrameTask xTaskNotifyWait handle=%x\r\n", (int)xHandleDmxFrame);
		xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

		switch(ulNotifiedValue)
		{
			case PUSH_TEST:
			{
				while(1)
				{
				uint8_t dmx_val = 0xEE;
				uint8_t buf[513] = { 0, };
				buf[0] = 0;  // for start code is alwasy 0
				memset(&buf[1], dmx_val, sizeof(buf) - 1);
				dmx_break();
				uart_write_bytes_with_break(UART_NUM_2, (const char*)buf, sizeof(buf), 5);

				printf("test push\r\n");
				vTaskDelay(100);
				}

			}
			break;

			case PUSH_PLAYSCENE:
			{
				static uint8_t dmx_val = 0x00;

				dmx_break();

				uint8_t buf[513] = { 0, };
				buf[0] = 0;  // for start code is alwasy 0
				memset(&buf[1], dmx_val, sizeof(buf) - 1);

				uart_write_bytes_with_break(UART_NUM_2, (const char*)buf, sizeof(buf), 5);
				dmx_val++;
			}
			break;

			case PUSH_ADDRESS_UPDATE:
			{


				uint8_t buf[2];
				uint16_t newaddr = 0;

				newaddr = atoi(params.new_address);
				printf("oldaddr = %s, newaddr = %d, %s\r\n", params.old_address, newaddr, params.new_address );

				if(newaddr > 254) {
					buf[0] = newaddr - 254;
					buf[1] = 254;
				} else {
					buf[0] = 0;
					buf[1] = newaddr;
				}

				printf("buf = {%x, %x}\r\n", buf[0], buf[1]);
				send_protocol_id(PROTOCOL_ID_SETADDR);
				uart_write_bytes(UART_NUM_2, (const char*)buf, 2);

				//dac_output_enable(DAC_CHANNEL_1);
				//timer_start(TIMER_GROUP_0, TIMER_0);

				vTaskDelay(4500);

				printf("xTaskNotifier handle=%x\r\n", (int)xHandleDmxFrame);
				xTaskNotify(xHandleMongoose, 0x01, eSetValueWithOverwrite);

			}
			break;

			case PUSH_ADDRESS_TARGET_UPDATE:
			{


				uint8_t buf[4];
				uint16_t oldaddr = 0;
				uint16_t newaddr = 0;

				oldaddr = atoi(params.old_address);
				newaddr = atoi(params.new_address);

				printf("oldaddr = %s, newaddr = %d, %s\r\n", params.old_address, newaddr, params.new_address );

				if(oldaddr > 254) {
					buf[0] = oldaddr - 254;
					buf[1] = 254;
				} else {
					buf[0] = 0;
					buf[1] = oldaddr;
				}

				if(newaddr > 254) {
					buf[2] = newaddr - 254;
					buf[3] = 254;
				} else {
					buf[2] = 0;
					buf[3] = newaddr;
				}

				printf("buf = {%x, %x, %x, %x}\r\n", buf[0], buf[1], buf[2], buf[3]);
				send_protocol_id(PROTOCOL_ID_SETTARGET_ADDR);
				uart_write_bytes(UART_NUM_2, (const char*)buf, 4);

				//dac_output_enable(DAC_CHANNEL_1);
				//timer_start(TIMER_GROUP_0, TIMER_0);

				vTaskDelay(4500);

				printf("xTaskNotifier handle=%x\r\n", (int)xHandleDmxFrame);
				xTaskNotify(xHandleMongoose, 0x01, eSetValueWithOverwrite);
			}
			break;

			case PUSH_CONFIG_UPDATE:
			{

				uint8_t buf[3];
				uint16_t adc = 0;
				uint16_t brightness = 0;

				adc = atoi(params.adc);
				brightness = atoi(params.brightness);

				printf("adc = %s, brightness = %s\r\n", params.adc, params.brightness );

				if(adc > 254) {
					buf[0] = adc - 254;
					buf[1] = 254;
				} else {
					buf[0] = 0;
					buf[1] = adc;
				}

				buf[2] = (uint8_t)brightness;


				printf("buf = {%x, %x, %x}\r\n", buf[0], buf[1], buf[2]);
				send_protocol_id(PROTOCOL_ID_SETTEMP_THRES);
				uart_write_bytes(UART_NUM_2, (const char*)buf, 3);

				//dac_output_enable(DAC_CHANNEL_1);
				//timer_start(TIMER_GROUP_0, TIMER_0);

				vTaskDelay(4500);

				printf("xTaskNotifier handle=%x\r\n", (int)xHandleDmxFrame);
				xTaskNotify(xHandleMongoose, 0x01, eSetValueWithOverwrite);
			}
			break;

		}

		//printf("dmx value = %d updated\r\n", dmx_val);
	}
}



ssize_t nread;
FILE* fp;
uint8_t refill = 1;
uint8_t bdata_ready = 0;

extern portMUX_TYPE rtc_spinlock;

uint8_t bPlaySound = 0;

static void timer_isr(void* arg)
{
	//printf("timer_isr\r\n");
	static int i = 0;
	TIMERG0.int_clr_timers.t0 = 1;
	TIMERG0.hw_timer[0].config.alarm_en = 1;


	// for channel2
	// CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);

#if 1
	if(bPlaySound)
	{
		portENTER_CRITICAL(&rtc_spinlock);
		//Disable Tone
		CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);

		CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
		SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, data[i++], RTC_IO_PDAC1_DAC_S);   //dac_output

		portEXIT_CRITICAL(&rtc_spinlock);

		if(i >= sizeof(data))
		{
			i = 0;
			bPlaySound = 0;
			dac_output_disable(DAC_CHANNEL_1);

		}
	}
#else

	//dac_out_voltage(DAC_CHANNEL_1, );
	portENTER_CRITICAL(&rtc_spinlock);
	//Disable Tone
	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);

	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, sine_wave[i++], RTC_IO_PDAC1_DAC_S);   //dac_output

	portEXIT_CRITICAL(&rtc_spinlock);

	if(i>= sizeof(sine_wave))
	{
		i=0;


	}

	//TIMERG0.int_clr_timers.t0 = 1;
	//TIMERG0.hw_timer[0].config.alarm_en = 1;

#endif
}



//================
int app_main(void)
{
    



    printf("\r\n\n");
    ESP_LOGI(tag, "==== STARTING SPIFFS TEST ====\r\n");

    vfs_spiffs_register();
    printf("\r\n\n");


	initialize_wifi();
	initialize_dmx();

	ledc_timer_config_t ledc_timer = {0};
	ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
	ledc_timer.bit_num = LEDC_TIMER_10_BIT;
	ledc_timer.timer_num = LEDC_TIMER_0;
	ledc_timer.freq_hz = 200;
	ESP_ERROR_CHECK( ledc_timer_config(&ledc_timer) );


	ledc_channel_config_t lecstruct = {0};
	lecstruct.gpio_num = GPIO_NUM_18;
	lecstruct.speed_mode = LEDC_HIGH_SPEED_MODE;
	lecstruct.channel = LEDC_CHANNEL_0;
	lecstruct.intr_type = LEDC_INTR_DISABLE;
	lecstruct.timer_sel = LEDC_TIMER_0;
	lecstruct.duty = 0;

	ESP_ERROR_CHECK( ledc_channel_config(&lecstruct) );

	lecstruct.gpio_num = GPIO_NUM_5;
	lecstruct.speed_mode = LEDC_HIGH_SPEED_MODE;
	lecstruct.channel = LEDC_CHANNEL_1;
	lecstruct.intr_type = LEDC_INTR_DISABLE;
	lecstruct.timer_sel = LEDC_TIMER_0;


	ESP_ERROR_CHECK( ledc_channel_config(&lecstruct) );


	char szAppName[256] = {0};
	struct stat sb;

	sprintf(szAppName, "%s%s", SPIFFS_PATH, "/wtf.wav");
	stat(szAppName, &sb);


	printf("file size = %d\r\n\n", (int)sb.st_size);
    fp = fopen(szAppName, "rb");


    static intr_handle_t s_timer_handle;


    timer_config_t config = {
	.alarm_en = true,
	.counter_en = false,
	.intr_type = TIMER_INTR_LEVEL,
	.counter_dir = TIMER_COUNT_UP,
	.auto_reload = true,
	.divider = 80   /* 1uS nano per tick */
	};

	timer_init(TIMER_GROUP_0, TIMER_0, &config);
	timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);


	//timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, ((NOTE_C5*1000) / sizeof(sine_wave)) / 500);  // sampling rate 8K
	timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 46);  // sampling rate 8K
	timer_enable_intr(TIMER_GROUP_0, TIMER_0);
	timer_isr_register(TIMER_GROUP_0, TIMER_0, &timer_isr, NULL, 0, &s_timer_handle);



	xTaskCreate(&DmxFrameTask, "DmxFrameTask", 4096, NULL, 6, &xHandleDmxFrame);


	dac_output_enable(DAC_CHANNEL_1);
	bPlaySound = 1;
	timer_start(TIMER_GROUP_0, TIMER_0);



	    /*
	    do
	    {
	    	nread = fread(buf, 1, sizeof(buf), fp);
	    	printf("read = %d bytes\r\n\n", nread);

	    	if(nread != 0)
	    	{
	    		mg_send(nc, buf, nread);
	    		printf("mg_send = %d bytes\r\n\n", nread);
	    	}

	    } while(nread!=0);

	 w   fclose(fp);
	    */

	printf("while(1)\r\n");

    while (1) {
    	static int led_duty = 0;
    	static int dir = 1;

    	static int keyidx = 0;
    	static uint32_t tmperiod = 0;
    	uint16_t duration = 0;


    	if(bStationConnected!=0)
    	{
    	//ledc_channel_t channel, uint32_t duty);


    		if(dir == 1)
    			led_duty+=60;
    		else
    			led_duty-=60;

    		if( led_duty <= 0 || 1024 <= led_duty )
    			dir *= -1;

    		if( led_duty <= 0)
    			led_duty = 0;
    		if( led_duty >= 1024)
    			led_duty = 1024-1;

    		//printf("dir = %d, led_duty = %d\r\n", dir, led_duty);

    		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, led_duty );
    		ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);


    	} else {

    		ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
    		ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);

        }


#if 0
		if(melody[keyidx] != 0 )
		{
			timer_pause(TIMER_GROUP_0, TIMER_0);
			//dac_output_enable(DAC_CHANNEL_1);
			//tmperiod = (melody[keyidx] / sizeof(sine_wave));
			//tmperiod = ((melody[keyidx++]*1000) / sizeof(sine_wave)) / 500;
//			printf("%dhz, divider = %d us\r\n",  80000000 / tmperiod, tmperiod );

			tmperiod =  80000000 / (melody[keyidx] * sizeof(sine_wave));

			timer_set_divider(TIMER_GROUP_0, TIMER_0, tmperiod);
		    //TIMERG0.hw_timer[0].alarm_high  = (uint32_t) (tmperiod >> 32);
		    //TIMERG0.hw_timer[0].alarm_low  = (uint32_t) tmperiod;
			//timer_set_alarm_value(TIMER_GROUP_0, TIMER_0,tmperiod);  // sampling rate 8K
			//printf("Note: %dhz, sinewave: %dhz, divider = %d us\r\n", 80000000 / tmperiod *sizeof(sine_wave) , 80000000 / tmperiod, tmperiod );

		    timer_start(TIMER_GROUP_0, TIMER_0);
		} else {
			//dac_output_disable(DAC_CHANNEL_1);
			timer_pause(TIMER_GROUP_0, TIMER_0);
		}

		printf("Note: %dhz, sinewave: %dhz, divider = %d us\r\n", (80000000 / tmperiod)  / sizeof(sine_wave) , 80000000 / tmperiod , tmperiod );

		duration = 1500 / tempo[keyidx];


		vTaskDelay(duration / portTICK_RATE_MS);
		timer_pause(TIMER_GROUP_0, TIMER_0);
		keyidx++;

		printf("keyidx = %d, size = %d \r\n",  keyidx, sizeof(keys)/2 );
		if( keyidx == sizeof(melody)/4 )
		{
			keyidx = 0;
		}

#endif
		vTaskDelay(100 / portTICK_RATE_MS);
    }

   // fclose(fp);
    return 0;
}





