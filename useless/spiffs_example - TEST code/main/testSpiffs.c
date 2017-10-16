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

#define MAX_SUPPORTED_TYPE 6

const char* szSupportedExt[] = {"js", "html", "shtml", "png", "gif", "css"};
const char* szMIMETypes[]    = {"application/javascript", "text/html", "text/html", "image/png", "image/gif", "text/css" };


static const char tag[] = "[SPIFFS example]";
static struct mg_serve_http_opts s_opts;
void mongooseTask(void *data);


#define CONFIG_EXAMPLE_USE_WIFI 1
#ifdef CONFIG_EXAMPLE_USE_WIFI

struct device_settings {
  char setting1[100];
  char setting2[100];
};


static struct device_settings s_settings = {"value1", "value2"};

static uint32_t usec = 40;
static uint8_t bPlay = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
	switch (event->event_id) {
	case SYSTEM_EVENT_AP_START:
		printf("Accesspoint connected\n");
		xTaskCreatePinnedToCore(&mongooseTask, "mongooseTask", 80000, NULL, 5, NULL, 0);
		break;

	case SYSTEM_EVENT_AP_STOP:
		printf("Accesspoint stopped\n");
		break;

	case SYSTEM_EVENT_AP_STACONNECTED:
		printf("Accesspoint Station connected\n");
		break;

	case SYSTEM_EVENT_AP_STADISCONNECTED:
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

static void handle_save(struct mg_connection *nc, struct http_message *hm) {
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

static void handle_get_cpu_usage(struct mg_connection *nc) {
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

static void handle_ssi_call(struct mg_connection *nc, const char *param) {
  printf("handle_ssi_call\r\n");
  if (strcmp(param, "setting1") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.setting1);
  } else if (strcmp(param, "setting2") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.setting2);
  }
}




// Mongoose event handler.
void mongoose_event_handler(struct mg_connection *nc, int ev, void *evData) {
#if 0
	const int bufsize = 1024*4;
	struct stat sb;
	char buf[bufsize];

	switch (ev) {
	case MG_EV_HTTP_REQUEST: {
			struct http_message *message = (struct http_message *) evData;

			char *uri = mgStrToStr(message->uri);
			printf("Message %s\r\n\n", uri);

			if (strcmp(uri, "/time") == 0) {
				char payload[256];
				struct timeval tv;
				gettimeofday(&tv, NULL);
				sprintf(payload, "Time since start: %d.%d secs", (int)tv.tv_sec, (int)tv.tv_usec);
				int length = strlen(payload);
				mg_send_head(nc, 200, length, "Content-Type: text/plain");
				mg_printf(nc, "%s", payload);

			} else if (strcmp(uri, "/get_cpu_usage") == 0) {
				handle_get_cpu_usage(nc);

			} else if (strcmp(uri, "/") == 0) {


				char szAppName[256] = {0};
				sprintf(szAppName, "%s%s", SPIFFS_PATH, "/index.shtml");

				ssize_t nread;
				FILE* fp;
				stat(szAppName, &sb);
			    mg_send_head(nc, 200, (int64_t)sb.st_size, "Content-Type: text/html");

				printf("file size = %d\r\n\n", (int)sb.st_size);
			    fp = fopen(szAppName, "rb");

			    do
			    {
			    	nread = fread(buf, 1, sizeof(buf), fp);
			    	printf("read = %d bytes\r\n\n", nread);

			    	if(nread != 0)
			    	{
			    		mg_send(nc, buf, nread);
			    		printf("mg_send = %d bytes\r\n\n", nread);
			    	}

			    }while(nread!=0);

			    fclose(fp);

			} else {

				const char* ldot = strrchr(uri, '.');

				int idx = -1;
				if(ldot != NULL)
				{
					for(int i=0;i<MAX_SUPPORTED_TYPE;++i)
					{
						if(strcmp(ldot+1, szSupportedExt[i]) == 0)
						{
							idx = i;
							break;
						}
					}
				}

				char szAppName[256] = {0};
				char szContentType[128] = {0};

				if(idx != -1)
				{
					sprintf(szAppName, "%s%s", SPIFFS_PATH, uri);
					if(stat(szAppName, &sb) == 0)
					{
						// file exist time to read.
						ssize_t nread;
						FILE* fp;

						sprintf(szContentType, "Content-Type: %s", szMIMETypes[idx]);
						mg_send_head(nc, 200, (int64_t)sb.st_size, szContentType);

						printf("file size = %d\r\n\n", (int)sb.st_size);
						fp = fopen(szAppName, "rb");

						do
						{
							nread = fread(buf, 1, sizeof(buf), fp);
							printf("read = %d bytes\r\n\n", nread);

							if(nread != 0)
							{
								mg_send(nc, buf, nread);
								printf("mg_send = %d bytes\r\n\n", nread);
							}

						}while(nread!=0);

						fclose(fp);
					} else {
						mg_send_head(nc, 404, 0, "Content-Type: text/plain");
					}


				} else {
					mg_send_head(nc, 404, 0, "Content-Type: text/plain");
				}
			}

			nc->flags |= MG_F_SEND_AND_CLOSE;
			free(uri);
			break;
		}
	} // End of switch

#else
	//static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
	  struct http_message *hm = (struct http_message *) evData;

	  switch (ev) {
	    case MG_EV_HTTP_REQUEST:
	      if (mg_vcmp(&hm->uri, "/save") == 0) {
	    	 printf("mongoose_event_handler::handle_save\r\n");
	        handle_save(nc, hm);
	      } else if (mg_vcmp(&hm->uri, "/get_cpu_usage") == 0) {
	        handle_get_cpu_usage(nc);
	      } else {
	        mg_serve_http(nc, hm, s_opts);  // Serve static content
	      }
	      break;
	    case MG_EV_SSI_CALL:
	      handle_ssi_call(nc, evData);
	      break;
	    default:
	      break;
	  }
	//}
#endif

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

typedef enum {
	PUSH_IDLE = 0,
    PUSH_PLAYSCENE,
    PUSH_ADDRESS_UPDATE,
	PUSH_ADDRESS_TARGET_UPDATE,

    DMX_PUSH_MODE_MAX,
} dmx_push_state_t;

uint8_t dmx_push_state_idx = PUSH_ADDRESS_TARGET_UPDATE;

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
	while (1) {

		switch(dmx_push_state_idx)
		{
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
				send_protocol_id(PROTOCOL_ID_SETADDR);

				uint8_t buf[2] = { 111, 0 };
				uart_write_bytes(UART_NUM_2, (const char*)buf, 2);

				vTaskDelay(4500);
			}
			break;

			case PUSH_ADDRESS_TARGET_UPDATE:
			{
				static uint8_t toggle = 0;

				send_protocol_id(PROTOCOL_ID_SETTARGET_ADDR);

				uint8_t buf[2][4] = { {113,0,111,0},{ 111, 0, 113, 0 } };

				if(toggle == 0)
				{
					uart_write_bytes(UART_NUM_2, (const char*)buf[0], 4);
					toggle = 1;
				} else {
					toggle = 0;
					uart_write_bytes(UART_NUM_2, (const char*)buf[1], 4);
				}



				vTaskDelay(8000);

			}
			break;
		}

		//printf("dmx value = %d updated\r\n", dmx_val);
	}
}


void LedTask(void *pv)
{
	int i = 0;
	uint32_t t0, t1;


	t0 = t1 = 0;

	while (1) {

		 if(bPlay)
		 {
			 if(t0 == 0){
				 t0 = system_get_time();
			 }

			 t1 = system_get_time() - t0;

			 if(t1 > usec)
			 {

				  //dac_out_voltage(DAC_CHANNEL_1, data[i++]);
				  t0 = system_get_time();

				//  if(i>NUM_ELEMENTS-1)
				  {
					  i = 0;
					  bPlay = 0;
				  }

			 }
		 }

#if 0
		   uint32_t max_duty = (1 << LEDC_TIMER_10_BIT) - 1;
		   //uint32_t duty = lroundf((float)data[i++]  (float)max_duty);
		   uint32_t duty = lroundf((float)data[i++] * 4.01f);

		   //printf("---- Data[%d]=%d, duty=%d/%d\r\n", i, data[i], duty, max_duty);



		   ESP_ERROR_CHECK( ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty) );
		   ESP_ERROR_CHECK( ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0) );
#endif

	}
}

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
#if 0
	ESP_LOGV(tag, "Mongoose task starting");
	struct mg_mgr mgr;
	ESP_LOGV(tag, "Mongoose: Starting setup");
	mg_mgr_init(&mgr, NULL);
	ESP_LOGV(tag, "Mongoose: Succesfully inited");
	struct mg_connection *c = mg_bind(&mgr, ":80", mongoose_event_handler);
	ESP_LOGV(tag, "Mongoose Successfully bound");
	if (c == NULL) {
		ESP_LOGE(tag, "No connection from the mg_bind()");
		vTaskDelete(NULL);
		return;
	}
	mg_set_protocol_http_websocket(c);

	while (1) {

	    static time_t last_time;
	    time_t now = time(NULL);
	    mg_mgr_poll(&mgr, 1000);
	    if (now - last_time > 0) {
	      push_data_to_all_websocket_connections(&mgr);
	      last_time = now;
	    }

		//mg_mgr_poll(&mgr, 1000);
	}
#else

	struct mg_mgr mgr;
	mg_mgr_init(&mgr, NULL);
	struct mg_connection *nc = mg_bind(&mgr, ":80", mongoose_event_handler);
	if (nc == NULL) {
		printf("No connection from the mg_bind()\n");
		vTaskDelete(NULL);
		return;
	}

//	static struct mg_serve_http_opts s_opts;

	mg_set_protocol_http_websocket(nc);
	//s_opts.

	memset(&s_opts, 0, sizeof(s_opts));
	//s_opts.
	s_opts.document_root = "/spiffs/images";
	s_opts.index_files = "index.shtml";
	while (1) {

	    static time_t last_time;
	    time_t now = time(NULL);
	    mg_mgr_poll(&mgr, 1000);
	    if (now - last_time > 0) {
	      push_data_to_all_websocket_connections(&mgr);
	      last_time = now;
	    }
		//mg_mgr_poll(&mgr, 200);
	}
	mg_mgr_free(&mgr);
#endif


} // mongooseTask

ssize_t nread;
FILE* fp;
uint8_t refill = 1;
uint8_t bdata_ready = 0;

extern portMUX_TYPE rtc_spinlock;

static void timer_isr(void* arg)
{
	//printf("timer_isr\r\n");
	static int i = 0;
	TIMERG0.int_clr_timers.t0 = 1;
	TIMERG0.hw_timer[0].config.alarm_en = 1;


	// for channel2
	// CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);

#if 1
	portENTER_CRITICAL(&rtc_spinlock);
	//Disable Tone
	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL1_REG, SENS_SW_TONE_EN);

	CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
	SET_PERI_REG_BITS(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC, data[i++], RTC_IO_PDAC1_DAC_S);   //dac_output

	portEXIT_CRITICAL(&rtc_spinlock);

	if(i >= sizeof(data))
	{
		i = 0;
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
    
	///DIR *dir = NULL;
//	struct dirent *ent;

	//char buf[1024];
//	ssize_t nread;

//		const char* szPath = "/spiffs/";



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



#if 0
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
#endif


	initialize_wifi();
	initialize_dmx();


#if 0
		dir = opendir(szPath);
		ent = readdir(dir);

		ent->


		FILE* fp = fopen("/spiffs/index.shtml", "rb");

		//fd_from = open(from, O_RDONLY);
		if (fp == NULL);

		do
		{
		nread = fread(buf, 1, sizeof(buf), fp);


		} while (nread > 0);

#endif



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
	lecstruct.duty = 1024-1;

	ESP_ERROR_CHECK( ledc_channel_config(&lecstruct) );

	lecstruct.gpio_num = GPIO_NUM_5;
	lecstruct.speed_mode = LEDC_HIGH_SPEED_MODE;
	lecstruct.channel = LEDC_CHANNEL_1;
	lecstruct.intr_type = LEDC_INTR_DISABLE;
	lecstruct.timer_sel = LEDC_TIMER_0;


	ESP_ERROR_CHECK( ledc_channel_config(&lecstruct) );


#if 0

	typedef struct {
	    bool alarm_en; /*!< Timer alarm enable */
	    bool counter_en; /*!< Counter enable */
	    timer_intr_mode_t intr_type; /*!< Interrupt mode */
	    timer_count_dir_t counter_dir; /*!< Counter direction  */
	    bool auto_reload; /*!< Timer auto-reload */
	    uint16_t divider; /*!< Counter clock divider*/
	} timer_config_t;




	esp_err_t timer_init(timer_group_t group_num, timer_idx_t timer_num, const timer_config_t* config);
	timer_init()
#endif

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

	//tmperiod = ;

	//timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, ((NOTE_C5*1000) / sizeof(sine_wave)) / 500);  // sampling rate 8K
	timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 46);  // sampling rate 8K
	timer_enable_intr(TIMER_GROUP_0, TIMER_0);
	timer_isr_register(TIMER_GROUP_0, TIMER_0, &timer_isr, NULL, 0, &s_timer_handle);


	//dac_output_enable(DAC_CHANNEL_1);
	//timer_start(TIMER_GROUP_0, TIMER_0);

	//xTaskCreate(&DmxFrameTask, "DmxFrameTask", 4096, NULL, 6, NULL);
	//xTaskCreate(&LedTask, "LedTask", 4096, NULL, 6, NULL);
	//void LedTask(void *pv)



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
    	static int keyidx = 0;
    	static uint32_t tmperiod = 0;
    	uint16_t duration = 0;


    	//ledc_channel_t channel, uint32_t duty);
    	ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, led_duty );
    	led_duty+=50;
    	ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
    	if(led_duty >= 1024)
    		led_duty = 0;





#if 0
    	tmperiod = ((keys[keyidx++]*1000) / sizeof(sine_wave)) / 500;

		printf("time period = %dus\r\n",  tmperiod );
		timer_set_alarm_value(TIMER_GROUP_0, TIMER_0,tmperiod);  // sampling rate 8K

		printf("keyidx = %d, size = %d \r\n",  keyidx, sizeof(keys)/2 );
		if( keyidx == sizeof(keys)/2 )
		{
			keyidx = 20;
		}
#endif

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





