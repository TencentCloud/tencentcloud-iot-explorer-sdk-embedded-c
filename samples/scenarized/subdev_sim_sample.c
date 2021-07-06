#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#include "qcloud_iot_export.h"
#include "utils_getopt.h"
#include "lite-utils.h"

#define LOCAL_SERVER_PORT (20000)

static char sg_pid[MAX_SIZE_OF_PRODUCT_ID + 1]    = "SUB_PID";
static char sg_dname[MAX_SIZE_OF_DEVICE_NAME + 1] = "SUB_DEVICENAME";

static int parse_arguments(int argc, char **argv)
{
    int c;
    while ((c = utils_getopt(argc, argv, "p:d:")) != EOF) switch (c) {
            case 'p':
                Log_d("Get product id %s", utils_optarg);
                strncpy(sg_pid, utils_optarg, MAX_SIZE_OF_PRODUCT_ID + 1);
                break;

            case 'd':
                Log_d("Get device name %s", utils_optarg);
                strncpy(sg_dname, utils_optarg, MAX_SIZE_OF_DEVICE_NAME + 1);
                break;

            default:
                HAL_Printf(
                    "usage: %s [options]\n"
                    "  [-p <product id>] \n"
                    "  [-d <device name>] \n",
                    argv[0]);
                return -1;
        }
    return 0;
}

#define STATUS_MSG_FMT    "{\"subdev_type\":\"%s\",\"product_id\":\"%s\",\"device_name\":\"%s\"}"
#define ANSI_COLOR_RED    "\x1b[31m"
#define ANSI_COLOR_GREEN  "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE   "\x1b[34m"
#define ANSI_COLOR_RESET  "\x1b[0m"

static int  sg_pwr_switch = 0;
static int  sg_color      = 0;
static int  sg_brightness = 0;
static char sg_client_token[64];

static void _deal_with_msg(char *msg, int fd)
{
    int   i;
    char *s_pwr_switch   = LITE_json_value_of("params.power_switch", msg);
    char *s_color        = LITE_json_value_of("params.color", msg);
    char *s_brightness   = LITE_json_value_of("params.brightness", msg);
    char *s_client_token = LITE_json_value_of("clientToken", msg);
    char *method         = LITE_json_value_of("method", msg);

    if (strcmp(method, "control")) {
        Log_d("only control message supported");
        goto exit;
    }

    if (!s_client_token) {
        Log_e("Failed to get clientToken");
        goto exit;
    }
    strncpy(sg_client_token, s_client_token, 64);
    sg_client_token[63] = 0;
    if (s_pwr_switch)
        LITE_get_int32(&sg_pwr_switch, s_pwr_switch);
    if (s_color)
        LITE_get_int32(&sg_color, s_color);
    if (s_brightness)
        LITE_get_int32(&sg_brightness, s_brightness);
    const char *ansi_color         = NULL;
    const char *ansi_color_name    = NULL;
    char        brightness_bar[]   = "||||||||||||||||||||";
    int         brightness_bar_len = strlen(brightness_bar);
    /* light color */
    switch (sg_color) {
        case 0:
            ansi_color      = ANSI_COLOR_RED;
            ansi_color_name = " RED ";
            break;
        case 1:
            ansi_color      = ANSI_COLOR_GREEN;
            ansi_color_name = "GREEN";
            break;
        case 2:
            ansi_color      = ANSI_COLOR_BLUE;
            ansi_color_name = " BLUE";
            break;
        default:
            ansi_color      = ANSI_COLOR_YELLOW;
            ansi_color_name = "UNKNOWN";
            break;
    }
    /* light brightness bar */
    brightness_bar_len =
        (sg_brightness >= 100) ? brightness_bar_len : (int)((sg_brightness * brightness_bar_len) / 100);
    for (i = brightness_bar_len; i < strlen(brightness_bar); i++) {
        brightness_bar[i] = '-';
    }

    if (sg_pwr_switch) {
        /* light is on , show with the properties*/
        HAL_Printf("%s[  lighting  ]|[color:%s]|[brightness:%s]\n" ANSI_COLOR_RESET, ansi_color, ansi_color_name,
                   brightness_bar);
    } else {
        /* light is off */
        HAL_Printf(ANSI_COLOR_YELLOW "[  light is off ]|[color:%s]|[brightness:%s]\n" ANSI_COLOR_RESET, ansi_color_name,
                   brightness_bar);
    }

    // reply control message
    char reply_control[256];
    HAL_Snprintf(reply_control, 256, "{\"method\":\"control_reply\",\"clientToken\":\"%s\",\"code\":%d}",
                 sg_client_token, 0);
    send(fd, reply_control, strlen(reply_control) + 1, 0);

exit:
    HAL_Free(method);
    HAL_Free(s_client_token);
    HAL_Free(s_pwr_switch);
    HAL_Free(s_color);
    HAL_Free(s_brightness);
}

int main(int argc, char **argv)
{
    int                ret = 0;
    struct sockaddr_in srv_addr;
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_addr.s_addr = INADDR_ANY;
    srv_addr.sin_family      = AF_INET;
    srv_addr.sin_port        = htons(LOCAL_SERVER_PORT);
    int fd                   = socket(AF_INET, SOCK_STREAM, 0);
    int i;

    ret = parse_arguments(argc, argv);
    if (ret != QCLOUD_RET_SUCCESS) {
        Log_e("parse arguments error, rc = %d", ret);
        return ret;
    }

    IOT_Log_Set_Level(eLOG_DEBUG);
    if (connect(fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
        Log_e("Failed to connect to gateway");
        goto exit;
    }

    char msg[256];
    // set online
    {
        Log_d("connect gateway OK, subdev is going to be online");
        ret = HAL_Snprintf(msg, 256, STATUS_MSG_FMT, "online", sg_pid, sg_dname);
        ret = send(fd, msg, ret + 1, 0);
        if (ret < 0) {
            Log_e("Failed to write");
            goto exit;
        }
    }

    fd_set         rd_set;
    struct timeval tv;
    srand(time(NULL));
    // loop
    for (i = 0; i < 100; ++i) {
        FD_ZERO(&rd_set);
        FD_SET(fd, &rd_set);
        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        ret        = select(fd + 1, &rd_set, NULL, NULL, &tv);
        if (ret < 0) {
            Log_e("Failed to select");
            break;
        }
        if (ret > 0 && FD_ISSET(fd, &rd_set)) {
            ret = recv(fd, msg, 256, 0);
            if (ret <= 0)
                continue;
            msg[ret] = 0;
            Log_d("recv %s", msg);
            _deal_with_msg(msg, fd);
            continue;
        }
        if (i & 0xf)
            continue;
#define REPLY_MSG_FMT                                                                                        \
    "{\"subdev_type\": \"report\",\"product_id\": \"%s\",\"device_name\": \"%s\",\"msg\": "                  \
    "\"{\"method\":\"%s\",\"clientToken\":\"subdev-%d\",\"timestamp\":%lu,\"power_switch\":%d,\"color\":%d," \
    "\"brightness\":%d}}"
        char report_msg[256];
        ret = HAL_Snprintf(report_msg, 256, REPLY_MSG_FMT, sg_pid, sg_dname, "report", rand(), time(NULL),
                           sg_pwr_switch, sg_color, sg_brightness);
        Log_d("Report property %s", report_msg);
        send(fd, report_msg, ret + 1, 0);
    }

    // set offline
    {
        Log_d("set subdev offline");
        ret = HAL_Snprintf(msg, 256, STATUS_MSG_FMT, "offline", sg_pid, sg_dname);
        ret = send(fd, msg, ret + 1, 0);
        if (ret < 0) {
            Log_e("Failed to write %d", ret);
            goto exit;
        }
        HAL_SleepMs(5000);
    }
exit:
    if (fd > 0)
        close(fd);

    return ret;
}