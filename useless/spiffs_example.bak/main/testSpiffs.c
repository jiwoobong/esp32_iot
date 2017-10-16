// how to set eclipse with gdb.
// https://esp32.com/viewtopic.php?f=13&t=336&hilit=gdb&start=10

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define CONFIG_EXAMPLE_USE_WIFI 1

#ifdef CONFIG_EXAMPLE_USE_WIFI

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

#endif

#include <errno.h>
#include <sys/fcntl.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "spiffs_vfs.h"

#include "test1_html.h"
#include "mongoose.h"
#include "driver/uart.h"
#include "driver/gpio.h"



static const char tag[] = "[SPIFFS example]";

#ifdef CONFIG_EXAMPLE_USE_WIFI



static esp_err_t event_handler(void *ctx, system_event_t *event)
{
//	vfs_spiffs_register
	return ESP_OK;
}


//-------------------------------
static void initialize_wifi(void)
{
	tcpip_adapter_init();
	tcpip_adapter_ip_info_t info = { 0, };
	IP4_ADDR(&info.ip, 192, 168, 2, 1);
	IP4_ADDR(&info.gw, 192, 168, 2, 1);
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


#endif


// ============================================================================
#include <ctype.h>

// fnmatch defines
#define	FNM_NOMATCH	1	// Match failed.
#define	FNM_NOESCAPE	0x01	// Disable backslash escaping.
#define	FNM_PATHNAME	0x02	// Slash must be matched by slash.
#define	FNM_PERIOD		0x04	// Period must be matched by period.
#define	FNM_LEADING_DIR	0x08	// Ignore /<tail> after Imatch.
#define	FNM_CASEFOLD	0x10	// Case insensitive search.
#define FNM_PREFIX_DIRS	0x20	// Directory prefixes of pattern match too.
#define	EOS	        '\0'

//-----------------------------------------------------------------------
static const char * rangematch(const char *pattern, char test, int flags)
{
  int negate, ok;
  char c, c2;

  /*
   * A bracket expression starting with an unquoted circumflex
   * character produces unspecified results (IEEE 1003.2-1992,
   * 3.13.2).  This implementation treats it like '!', for
   * consistency with the regular expression syntax.
   * J.T. Conklin (conklin@ngai.kaleida.com)
   */
  if ( (negate = (*pattern == '!' || *pattern == '^')) ) ++pattern;

  if (flags & FNM_CASEFOLD) test = tolower((unsigned char)test);

  for (ok = 0; (c = *pattern++) != ']';) {
    if (c == '\\' && !(flags & FNM_NOESCAPE)) c = *pattern++;
    if (c == EOS) return (NULL);

    if (flags & FNM_CASEFOLD) c = tolower((unsigned char)c);

    if (*pattern == '-' && (c2 = *(pattern+1)) != EOS && c2 != ']') {
      pattern += 2;
      if (c2 == '\\' && !(flags & FNM_NOESCAPE)) c2 = *pattern++;
      if (c2 == EOS) return (NULL);

      if (flags & FNM_CASEFOLD) c2 = tolower((unsigned char)c2);

      if ((unsigned char)c <= (unsigned char)test &&
          (unsigned char)test <= (unsigned char)c2) ok = 1;
    }
    else if (c == test) ok = 1;
  }
  return (ok == negate ? NULL : pattern);
}

//--------------------------------------------------------------------
static int fnmatch(const char *pattern, const char *string, int flags)
{
  const char *stringstart;
  char c, test;

  for (stringstart = string;;)
    switch (c = *pattern++) {
    case EOS:
      if ((flags & FNM_LEADING_DIR) && *string == '/') return (0);
      return (*string == EOS ? 0 : FNM_NOMATCH);
    case '?':
      if (*string == EOS) return (FNM_NOMATCH);
      if (*string == '/' && (flags & FNM_PATHNAME)) return (FNM_NOMATCH);
      if (*string == '.' && (flags & FNM_PERIOD) &&
          (string == stringstart ||
          ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
              return (FNM_NOMATCH);
      ++string;
      break;
    case '*':
      c = *pattern;
      // Collapse multiple stars.
      while (c == '*') c = *++pattern;

      if (*string == '.' && (flags & FNM_PERIOD) &&
          (string == stringstart ||
          ((flags & FNM_PATHNAME) && *(string - 1) == '/')))
              return (FNM_NOMATCH);

      // Optimize for pattern with * at end or before /.
      if (c == EOS)
        if (flags & FNM_PATHNAME)
          return ((flags & FNM_LEADING_DIR) ||
                    strchr(string, '/') == NULL ?
                    0 : FNM_NOMATCH);
        else return (0);
      else if ((c == '/') && (flags & FNM_PATHNAME)) {
        if ((string = strchr(string, '/')) == NULL) return (FNM_NOMATCH);
        break;
      }

      // General case, use recursion.
      while ((test = *string) != EOS) {
        if (!fnmatch(pattern, string, flags & ~FNM_PERIOD)) return (0);
        if ((test == '/') && (flags & FNM_PATHNAME)) break;
        ++string;
      }
      return (FNM_NOMATCH);
    case '[':
      if (*string == EOS) return (FNM_NOMATCH);
      if ((*string == '/') && (flags & FNM_PATHNAME)) return (FNM_NOMATCH);
      if ((pattern = rangematch(pattern, *string, flags)) == NULL) return (FNM_NOMATCH);
      ++string;
      break;
    case '\\':
      if (!(flags & FNM_NOESCAPE)) {
        if ((c = *pattern++) == EOS) {
          c = '\\';
          --pattern;
        }
      }
      break;
      // FALLTHROUGH
    default:
      if (c == *string) {
      }
      else if ((flags & FNM_CASEFOLD) && (tolower((unsigned char)c) == tolower((unsigned char)*string))) {
      }
      else if ((flags & FNM_PREFIX_DIRS) && *string == EOS && ((c == '/' && string != stringstart) ||
    		  (string == stringstart+1 && *stringstart == '/')))
              return (0);
      else return (FNM_NOMATCH);
      string++;
      break;
    }
  // NOTREACHED
  return 0;
}

// ============================================================================

//-----------------------------------------
static void list(char *path, char *match) {

    DIR *dir = NULL;
    struct dirent *ent;
    char type;
    char size[9];
    char tpath[255];
    char tbuffer[80];
    struct stat sb;
    struct tm *tm_info;
    char *lpath = NULL;
    int statok;

    printf("LIST of DIR [%s]\r\n", path);
    // Open directory
    dir = opendir(path);
    if (!dir) {
        printf("Error opening directory\r\n");
        return;
    }

    // Read directory entries
    uint64_t total = 0;
    int nfiles = 0;
    printf("T  Size      Date/Time         Name\r\n");
    printf("-----------------------------------\r\n");
    while ((ent = readdir(dir)) != NULL) {
    	sprintf(tpath, path);
        if (path[strlen(path)-1] != '/') strcat(tpath,"/");
        strcat(tpath,ent->d_name);
        tbuffer[0] = '\0';

        if ((match == NULL) || (fnmatch(match, tpath, (FNM_PERIOD)) == 0)) {
			// Get file stat
			statok = stat(tpath, &sb);

			if (statok == 0) {
				tm_info = localtime(&sb.st_mtime);
				strftime(tbuffer, 80, "%d/%m/%Y %R", tm_info);
			}
			else sprintf(tbuffer, "                ");

			if (ent->d_type == DT_REG) {
				type = 'f';
				nfiles++;
				if (statok) strcpy(size, "       ?");
				else {
					total += sb.st_size;
					if (sb.st_size < (1024*1024)) sprintf(size,"%8d", (int)sb.st_size);
					else if ((sb.st_size/1024) < (1024*1024)) sprintf(size,"%6dKB", (int)(sb.st_size / 1024));
					else sprintf(size,"%6dMB", (int)(sb.st_size / (1024 * 1024)));
				}
			}
			else {
				type = 'd';
				strcpy(size, "       -");
			}

			printf("%c  %s  %s  %s\r\n",
				type,
				size,
				tbuffer,
				ent->d_name
			);
        }
    }
    if (total) {
        printf("-----------------------------------\r\n");
    	if (total < (1024*1024)) printf("   %8d", (int)total);
    	else if ((total/1024) < (1024*1024)) printf("   %6dKB", (int)(total / 1024));
    	else printf("   %6dMB", (int)(total / (1024 * 1024)));
    	printf(" in %d file(s)\r\n", nfiles);
    }
    printf("-----------------------------------\r\n");

    closedir(dir);

    free(lpath);

	uint32_t tot, used;
	spiffs_fs_stat(&tot, &used);
	printf("SPIFFS: free %d KB of %d KB\r\n", (tot-used) / 1024, tot / 1024);
}

//----------------------------------------------------
static int file_copy(const char *to, const char *from)
{
    FILE *fd_to;
    FILE *fd_from;
    char buf[1024];
    ssize_t nread;
    int saved_errno;

    fd_from = fopen(from, "rb");
    //fd_from = open(from, O_RDONLY);
    if (fd_from == NULL) return -1;

    fd_to = fopen(to, "wb");
    if (fd_to == NULL) goto out_error;

    while (nread = fread(buf, 1, sizeof(buf), fd_from), nread > 0) {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = fwrite(out_ptr, 1, nread, fd_to);

            if (nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR) goto out_error;
        } while (nread > 0);
    }

    if (nread == 0) {
        if (fclose(fd_to) < 0) {
            fd_to = NULL;
            goto out_error;
        }
        fclose(fd_from);

        // Success!
        return 0;
    }

  out_error:
    saved_errno = errno;

    fclose(fd_from);
    if (fd_to) fclose(fd_to);

    errno = saved_errno;
    return -1;
}

//--------------------------------
static void writeTest(char *fname)
{
	printf("==== Write to file \"%s\" ====\r\n", fname);

	int n, res, tot, len;
	char buf[40];

	FILE *fd = fopen(fname, "wb");
    if (fd == NULL) {
    	printf("     Error opening file\r\n");
    	return;
    }
    tot = 0;
    for (n = 1; n < 11; n++) {
    	sprintf(buf, "ESP32 spiffs write to file, line %d\r\n", n);
    	len = strlen(buf);
		res = fwrite(buf, 1, len, fd);
		if (res != len) {
	    	printf("     Error writing to file(%d <> %d\r\n", res, len);
	    	break;
		}
		tot += res;
    }
	printf("     %d bytes written\r\n", tot);
	res = fclose(fd);
	if (res) {
    	printf("     Error closing file\r\n");
	}
    printf("\r\n");
}

//-------------------------------
static void readTest(char *fname)
{
	printf("==== Reading from file \"%s\" ====\r\n", fname);

	int res;
	char *buf;
	buf = calloc(1024, 1);
	if (buf == NULL) {
    	printf("     Error allocating read buffer\"\r\n");
    	return;
	}

	FILE *fd = fopen(fname, "rb");
    if (fd == NULL) {
    	printf("     Error opening file\r\n");
    	free(buf);
    	return;
    }
    res = 999;
    res = fread(buf, 1, 1023, fd);
    if (res <= 0) {
    	printf("     Error reading from file\r\n");
    }
    else {
    	printf("     %d bytes read [\r\n", res);
        buf[res] = '\0';
        printf("%s\r\n]\r\n", buf);
    }
	free(buf);

	res = fclose(fd);
	if (res) {
    	printf("     Error closing file\r\n");
	}
    printf("\r\n");
}

//----------------------------------
static void mkdirTest(char *dirname)
{
	printf("==== Make new directory \"%s\" ====\r\n", dirname);

	int res;
	struct stat st = {0};
	char nname[80];

	if (stat(dirname, &st) == -1) {
	    res = mkdir(dirname, 0777);
	    if (res != 0) {
	    	printf("     Error creating directory (%d)\r\n", res);
	        printf("\r\n");
	        return;
	    }
    	printf("     Directory created\r\n\r\n");
		list("/spiffs/", NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);

    	printf("     Copy file from root to new directory...\r\n");
    	sprintf(nname, "%s/test.txt.copy", dirname);
    	res = file_copy(nname, "/spiffs/test.txt");
	    if (res != 0) {
	    	printf("     Error copying file (%d)\r\n", res);
	    }
    	printf("\r\n");
    	list(dirname, NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);

    	printf("     Removing file from new directory...\r\n");
	    res = remove(nname);
	    if (res != 0) {
	    	printf("     Error removing directory (%d)\r\n", res);
	    }
    	printf("\r\n");
    	list(dirname, NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);

    	printf("     Removing directory...\r\n");
	    res = remove(dirname);
	    if (res != 0) {
	    	printf("     Error removing directory (%d)\r\n", res);
	    }
    	printf("\r\n");
		list("/spiffs/", NULL);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	else {
		printf("     Directory already exists, removing\r\n");
	    res = remove(dirname);
	    if (res != 0) {
	    	printf("     Error removing directory (%d)\r\n", res);
	    }
	}

    printf("\r\n");
}

//------------------------------------------------------
static int writeFile(char *fname, char *mode, char *buf)
{
	FILE *fd = fopen(fname, mode);
    if (fd == NULL) {
        ESP_LOGE("[write]", "fopen failed");
    	return -1;
    }
    int len = strlen(buf);
	int res = fwrite(buf, 1, len, fd);
	if (res != len) {
        ESP_LOGE("[write]", "fwrite failed: %d <> %d ", res, len);
        res = fclose(fd);
        if (res) {
            ESP_LOGE("[write]", "fclose failed: %d", res);
            return -2;
        }
        return -3;
    }
	res = fclose(fd);
	if (res) {
        ESP_LOGE("[write]", "fclose failed: %d", res);
    	return -4;
	}
    return 0;
}

//------------------------------
static int readFile(char *fname)
{
    uint8_t buf[16];
	FILE *fd = fopen(fname, "rb");
    if (fd == NULL) {
        ESP_LOGE("[read]", "fopen failed");
        return -1;
    }
    int res = fread(buf, 1, 8, fd);
    if (res <= 0) {
        ESP_LOGE("[read]", "fread failed: %d", res);
        res = fclose(fd);
        if (res) {
            ESP_LOGE("[read]", "fclose failed: %d", res);
            return -2;
        }
        return -3;
    }
	res = fclose(fd);
	if (res) {
        ESP_LOGE("[read]", "fclose failed: %d", res);
    	return -4;
	}
    return 0;
}

//================================
static void File_task_1(void* arg)
{
    int res = 0;
    int n = 0;
    
    ESP_LOGI("[TASK_1]", "Started.");
    res = writeFile("/spiffs/testfil1.txt", "wb", "1");
    if (res == 0) {
        while (n < 1000) {
            n++;
            res = readFile("/spiffs/testfil1.txt");
            if (res != 0) {
                ESP_LOGE("[TASK_1]", "Error reading from file (%d), pass %d", res, n);
                break;
            }
            res = writeFile("/spiffs/testfil1.txt", "a", "1");
            if (res != 0) {
                ESP_LOGE("[TASK_1]", "Error writing to file (%d), pass %d", res, n);
                break;
            }
            vTaskDelay(2);
            if ((n % 100) == 0) {
                ESP_LOGI("[TASK_1]", "%d reads/writes", n+1);
            }
        }
        if (n == 1000) ESP_LOGI("[TASK_1]", "Finished.");
    }
    else {
        ESP_LOGE("[TASK_1]", "Error creating file (%d)", res);
    }

    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

//================================
static void File_task_2(void* arg)
{
    int res = 0;
    int n = 0;
    
    ESP_LOGI("[TASK_2]", "Started.");
    res = writeFile("/spiffs/testfil2.txt", "wb", "2");
    if (res == 0) {
        while (n < 1000) {
            n++;
            res = readFile("/spiffs/testfil2.txt");
            if (res != 0) {
                ESP_LOGE("[TASK_2]", "Error reading from file (%d), pass %d", res, n);
                break;
            }
            res = writeFile("/spiffs/testfil2.txt", "a", "2");
            if (res != 0) {
                ESP_LOGE("[TASK_2]", "Error writing to file (%d), pass %d", res, n);
                break;
            }
            vTaskDelay(2);
            if ((n % 100) == 0) {
                ESP_LOGI("[TASK_2]", "%d reads/writes", n+1);
            }
        }
        if (n == 1000) ESP_LOGI("[TASK_2]", "Finished.");
    }
    else {
        ESP_LOGE("[TASK_2]", "Error creating file (%d)", res);
    }

    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

//================================
static void File_task_3(void* arg)
{
    int res = 0;
    int n = 0;

    ESP_LOGI("[TASK_3]", "Started.");
    res = writeFile("/spiffs/testfil3.txt", "wb", "3");
    if (res == 0) {
        while (n < 1000) {
            n++;
            res = readFile("/spiffs/testfil3.txt");
            if (res != 0) {
                ESP_LOGE("[TASK_3]", "Error reading from file (%d), pass %d", res, n);
                break;
            }
            res = writeFile("/spiffs/testfil3.txt", "a", "3");
            if (res != 0) {
                ESP_LOGE("[TASK_3]", "Error writing to file (%d), pass %d", res, n);
                break;
            }
            vTaskDelay(2);
            if ((n % 100) == 0) {
                ESP_LOGI("[TASK_3]", "%d reads/writes", n+1);
            }
        }
        if (n == 1000) ESP_LOGI("[TASK_3]", "Finished.");
    }
    else {
        ESP_LOGE("[TASK_3]", "Error creating file (%d)", res);
    }

    vTaskDelay(1000 / portTICK_RATE_MS);
    printf("\r\n");
	list("/spiffs/", NULL);
    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
}


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


// Mongoose event handler.
void mongoose_event_handler(struct mg_connection *nc, int ev, void *evData) {
	switch (ev) {
	case MG_EV_HTTP_REQUEST: {
			struct http_message *message = (struct http_message *) evData;

			char *uri = mgStrToStr(message->uri);

			if (strcmp(uri, "/time") == 0) {
				char payload[256];
				struct timeval tv;
				gettimeofday(&tv, NULL);
				sprintf(payload, "Time since start: %d.%d secs", (int)tv.tv_sec, (int)tv.tv_usec);
				int length = strlen(payload);
				mg_send_head(nc, 200, length, "Content-Type: text/plain");
				mg_printf(nc, "%s", payload);
			} else if (strcmp(uri, "/") == 0) {
				test1_html_len = sizeof(test1_html);
				mg_send_head(nc, 200, test1_html_len, "Content-Type: text/html");
				mg_send(nc, test1_html, test1_html_len);
			} else {
				mg_send_head(nc, 404, 0, "Content-Type: text/plain");
			}

			nc->flags |= MG_F_SEND_AND_CLOSE;
			free(uri);
			break;
		}
	} // End of switch
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



void DmxFrameTask(void *pv)
{
	while (1) {
		static uint8_t dmx_val = 0x00;

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

		uint8_t buf[513] = { 0, };
		buf[0] = 0;  // for start code is alwasy 0
		memset(&buf[1], dmx_val, sizeof(buf) - 1);

		uart_write_bytes_with_break(UART_NUM_2, (const char*)buf, sizeof(buf), 5);
		dmx_val++;
	}
}



void mongooseTask(void *data) {
	ESP_LOGD(tag, "Mongoose task starting");
	struct mg_mgr mgr;
	ESP_LOGD(tag, "Mongoose: Starting setup");
	mg_mgr_init(&mgr, NULL);
	ESP_LOGD(tag, "Mongoose: Succesfully inited");
	struct mg_connection *c = mg_bind(&mgr, ":80", mongoose_event_handler);
	ESP_LOGD(tag, "Mongoose Successfully bound");
	if (c == NULL) {
		ESP_LOGE(tag, "No connection from the mg_bind()");
		vTaskDelete(NULL);
		return;
	}
	mg_set_protocol_http_websocket(c);

	while (1) {
		mg_mgr_poll(&mgr, 1000);
	}
} // mongooseTask


//================
int app_main(void)
{
    
#if 0
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(tag, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
       ///obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
#endif

    printf("\r\n\n");
    ESP_LOGI(tag, "==== STARTING SPIFFS TEST ====\r\n");

    vfs_spiffs_register();
    printf("\r\n\n");

   	if (spiffs_is_mounted) {
		vTaskDelay(2000 / portTICK_RATE_MS);

		writeTest("/spiffs/test.txt");
		readTest("/spiffs/test.txt");
		readTest("/spiffs/spiffs.info");

		list("/spiffs/", NULL);
	    printf("\r\n");

		mkdirTest("/spiffs/newdir");

		printf("==== List content of the directory \"images\" ====\r\n\r\n");
        list("/spiffs/images", NULL);
	    printf("\r\n");
    }

//	nvs_flash_init();

	initialize_wifi();

	initialize_dmx();


	xTaskCreatePinnedToCore(&mongooseTask, "mongooseTask", 80000, NULL, 5, NULL, 0);
	xTaskCreate(&DmxFrameTask, "DmxFrameTask", 4096, NULL, 6, NULL);

    while (1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
    }
    return 0;
}
