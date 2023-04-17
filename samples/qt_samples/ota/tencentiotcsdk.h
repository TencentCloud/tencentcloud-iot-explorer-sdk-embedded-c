#ifndef TENCENTIOTCSDK_H
#define TENCENTIOTCSDK_H

#include <QDebug>
#include <QThread>
#include <QObject>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lite-utils.h"
#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"
#include "utils_getopt.h"

#define FW_RUNNING_VERSION "1.0.0"

#define KEY_VER  "version"
#define KEY_SIZE "downloaded_size"

#define FW_VERSION_MAX_LEN    32
#define FW_FILE_PATH_MAX_LEN  128
#define OTA_BUF_LEN           5000
#define FW_INFO_FILE_DATA_LEN 128
#define OTA_HTTP_MAX_FETCHED_SIZE (50 * 1024)

#define MAX_OTA_RETRY_CNT 5

typedef struct OTAContextData {
    void *ota_handle;
    void *mqtt_client;
    char  fw_file_path[FW_FILE_PATH_MAX_LEN];
    char  fw_info_file_path[FW_FILE_PATH_MAX_LEN];

    // remote_version means version for the FW in the cloud and to be downloaded
    char     remote_version[FW_VERSION_MAX_LEN];
    uint32_t fw_file_size;

    // for resuming download
    /* local_version means downloading but not running */
    char local_version[FW_VERSION_MAX_LEN];
    uint32_t  downloaded_size;

    // to make sure report is acked
    bool report_pub_ack;
    uint64_t  report_packet_id;
    uint32_t ota_fail_cnt;
} OTAContextData;

class TencentIotCSDK : public QThread
{
    Q_OBJECT
public:
    explicit TencentIotCSDK(QObject *parent = nullptr);
    bool process_ota(OTAContextData *ota_ctx);
    char *_get_local_fw_running_version();
    int _update_local_fw_info(OTAContextData *ota_ctx);
    int _cal_exist_fw_md5(OTAContextData *ota_ctx);
    int _update_fw_downloaded_size(OTAContextData *ota_ctx);
signals:
    void send_ota_success();
    void send_ota_debug_info(QString);
    void send_file_progress(int percent);
private:
    void run();
};

#endif // TENCENTIOTCSDK_H
