#include "tencentiotcsdk.h"

static DeviceInfo sg_devInfo;

static void _event_handler(void *pclient, void *handle_context, MQTTEventMsg *msg)
{
    (void)pclient;
    uintptr_t       packet_id = (uintptr_t)msg->msg;
    OTAContextData *ota_ctx   = (OTAContextData *)handle_context;

    switch (msg->event_type) {
        case MQTT_EVENT_UNDEF:
            Log_i("undefined event occur.");
            break;

        case MQTT_EVENT_DISCONNECT:
            Log_i("MQTT disconnect.");
            break;

        case MQTT_EVENT_RECONNECT:
            Log_i("MQTT reconnect.");
            break;

        case MQTT_EVENT_SUBCRIBE_SUCCESS:
            Log_i("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_TIMEOUT:
            Log_i("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_NACK:
            Log_i("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_PUBLISH_SUCCESS:
            Log_i("publish success, packet-id=%u", (unsigned int)packet_id);
            if (ota_ctx->report_packet_id == packet_id)
                ota_ctx->report_pub_ack = true;
            break;

        case MQTT_EVENT_PUBLISH_TIMEOUT:
            Log_i("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_PUBLISH_NACK:
            Log_i("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;
        default:
            Log_i("Should NOT arrive here.");
            break;
    }
}

static int _setup_connect_init_params(MQTTInitParams *initParams, void *ota_ctx, DeviceInfo *device_info)
{
    initParams->region      = device_info->region;
    initParams->product_id  = device_info->product_id;
    initParams->device_name = device_info->device_name;

#ifdef AUTH_MODE_CERT
    char  certs_dir[16] = "certs";
    char  current_path[128];
    char *cwd = getcwd(current_path, sizeof(current_path));

    if (cwd == NULL) {
        Log_e("getcwd return NULL");
        return QCLOUD_ERR_FAILURE;
    }

#ifdef WIN32
    HAL_Snprintf(initParams->cert_file, FILE_PATH_MAX_LEN, "%s\\%s\\%s", current_path, certs_dir,
                 device_info->dev_cert_file_name);
    HAL_Snprintf(initParams->key_file, FILE_PATH_MAX_LEN, "%s\\%s\\%s", current_path, certs_dir,
                 device_info->dev_key_file_name);
#else
    HAL_Snprintf(initParams->cert_file, FILE_PATH_MAX_LEN, "%s/%s/%s", current_path, certs_dir,
                 device_info->dev_cert_file_name);
    HAL_Snprintf(initParams->key_file, FILE_PATH_MAX_LEN, "%s/%s/%s", current_path, certs_dir,
                 device_info->dev_key_file_name);
#endif

#else
    initParams->device_secret = device_info->device_secret;
#endif

    initParams->command_timeout        = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    initParams->keep_alive_interval_ms = QCLOUD_IOT_MQTT_KEEP_ALIVE_INTERNAL;

    initParams->auto_connect_enable  = 1;
    initParams->event_handle.h_fp    = _event_handler;
    initParams->event_handle.context = ota_ctx;

    return QCLOUD_RET_SUCCESS;
}

static void _wait_for_pub_ack(OTAContextData *ota_ctx, int packet_id)
{
    int wait_cnt              = 10;
    ota_ctx->report_pub_ack   = false;
    ota_ctx->report_packet_id = packet_id;

    while (!ota_ctx->report_pub_ack) {
        HAL_SleepMs(500);
        IOT_MQTT_Yield(ota_ctx->mqtt_client, 500);
        if (wait_cnt-- == 0) {
            Log_e("wait report pub ack timeout!");
            break;
        }
    }
    ota_ctx->report_pub_ack = false;
    return;
}

/**********************************************************************************
 * OTA file operations START
 * these are platform-dependant functions
 * POSIX FILE is used in this sample code
 **********************************************************************************/
// calculate left MD5 for resuming download from break point
int TencentIotCSDK::_cal_exist_fw_md5(OTAContextData *ota_ctx)
{
    char   buff[OTA_BUF_LEN];
    size_t rlen, total_read = 0;
    int    ret = QCLOUD_RET_SUCCESS;

    ret = IOT_OTA_ResetClientMD5(ota_ctx->ota_handle);
    if (ret) {
        Log_e("reset MD5 failed: %d", ret);
        return QCLOUD_ERR_FAILURE;
    }

    void *fp = HAL_FileOpen(ota_ctx->fw_file_path, "ab+");
    if (NULL == fp) {
        Log_e("open file %s failed", ota_ctx->fw_file_path);
        return QCLOUD_ERR_FAILURE;
    }

    // rewind(fp);
    size_t size = ota_ctx->downloaded_size;

    while ((size > 0) && (!HAL_FileEof(fp))) {
        rlen = (size > OTA_BUF_LEN) ? OTA_BUF_LEN : size;
        if (rlen != HAL_FileRead(buff, 1, rlen, fp)) {
            Log_e("read data len not expected");
            ret = QCLOUD_ERR_FAILURE;
            break;
        }
        IOT_OTA_UpdateClientMd5(ota_ctx->ota_handle, buff, rlen);
        size -= rlen;
        total_read += rlen;
    }

    HAL_FileClose(fp);
    Log_d("total read: %d", total_read);
    return ret;
}

/* update local firmware info for resuming download from break point */
int TencentIotCSDK::_update_local_fw_info(OTAContextData *ota_ctx)
{
    void *fp;
    uint32_t   wlen;
    int   ret = QCLOUD_RET_SUCCESS;
    char  data_buf[FW_INFO_FILE_DATA_LEN];

    memset(data_buf, 0, sizeof(data_buf));
    HAL_Snprintf(data_buf, sizeof(data_buf), "{\"%s\":\"%s\", \"%s\":%d}", KEY_VER, ota_ctx->remote_version, KEY_SIZE,
                 ota_ctx->downloaded_size);

    fp = HAL_FileOpen(ota_ctx->fw_info_file_path, "w");
    if (NULL == fp) {
        Log_e("open file %s failed", ota_ctx->fw_info_file_path);
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }
    emit this->send_ota_debug_info(QString("%1").arg(data_buf));
    emit this->send_file_progress(ota_ctx->downloaded_size * 100 / ota_ctx->fw_file_size);
    wlen = HAL_FileWrite(data_buf, 1, strlen(data_buf), fp);
    if (wlen != strlen(data_buf)) {
        Log_e("save version to file err");
        ret = QCLOUD_ERR_FAILURE;
    }

exit:

    if (NULL != fp) {
        HAL_FileClose(fp);
    }

    return ret;
}

static int _get_local_fw_info(char *file_name, char *local_version)
{
    int  len;
    int  rlen;
    char json_doc[FW_INFO_FILE_DATA_LEN] = {0};

    FILE *fp = (FILE *)HAL_FileOpen(file_name, "r");
    //FILE *fp = fopen(file_name, "r");
    if (NULL == fp) {
        Log_e("open file %s failed", file_name);
        return 0;
    }

    HAL_FileSeek(fp, 0L, SEEK_END);
    len = ftell(fp);
    if (len > FW_INFO_FILE_DATA_LEN) {
        Log_e("%s is too big, pls check", file_name);
        HAL_FileClose(fp);
        return 0;
    }

    rewind(fp);
    rlen = HAL_FileRead(json_doc, 1, len, fp);
    if (len != rlen) {
        Log_e("read data len (%d) less than needed (%d), %s", rlen, len, json_doc);
        HAL_FileClose(fp);
        return 0;
    }

    char *version = LITE_json_value_of((char *)KEY_VER, json_doc);
    char *size    = LITE_json_value_of((char *)KEY_SIZE, json_doc);

    if ((NULL == version) || (NULL == size)) {
        if (version)
            HAL_Free(version);
        if (size)
            HAL_Free(size);
        HAL_FileClose(fp);
        return 0;
    }

    int local_size = atoi(size);
    HAL_Free(size);

    if (local_size <= 0) {
        Log_w("local info offset invalid: %d", local_size);
        local_size = 0;
    } else {
        strncpy(local_version, version, FW_VERSION_MAX_LEN);
    }
    HAL_Free(version);
    HAL_FileClose(fp);
    return local_size;
}

/* get local firmware offset for resuming download from break point */
int TencentIotCSDK::_update_fw_downloaded_size(OTAContextData *ota_ctx)
{
    int local_size = _get_local_fw_info(ota_ctx->fw_info_file_path, ota_ctx->local_version);
    if (local_size == 0) {
        ota_ctx->downloaded_size = 0;
        return 0;
    }

    if ((0 != strcmp(ota_ctx->local_version, ota_ctx->remote_version)) ||
        (ota_ctx->downloaded_size > ota_ctx->fw_file_size)) {
        ota_ctx->downloaded_size = 0;
        return 0;
    }

    ota_ctx->downloaded_size = local_size;
    Log_i("calc MD5 for resuming download from offset: %d", ota_ctx->downloaded_size);
    emit this->send_ota_debug_info(QString("%1%2").arg("calc MD5 for resuming download from offset:").arg(ota_ctx->downloaded_size));
    int ret = _cal_exist_fw_md5(ota_ctx);
    if (ret) {
        Log_e("regen OTA MD5 error: %d", ret);
        HAL_FileRemove(ota_ctx->fw_info_file_path);
        ota_ctx->downloaded_size = 0;
        return 0;
    }
    Log_d("local MD5 update done!");
    emit this->send_ota_debug_info("local MD5 update done!");
    return local_size;
}

static int _delete_fw_info_file(char *file_name)
{
    return HAL_FileRemove(file_name);
}

static int _save_fw_data_to_file(char *file_name, uint32_t offset, char *buf, int len)
{
    void *fp;
    if (offset > 0) {
        if (NULL == (fp = HAL_FileOpen(file_name, "ab+"))) {
            Log_e("open file failed");
            return QCLOUD_ERR_FAILURE;
        }
    } else {
        if (NULL == (fp = HAL_FileOpen(file_name, "wb+"))) {
            Log_e("open file failed");
            return QCLOUD_ERR_FAILURE;
        }
    }

    HAL_FileSeek(fp, offset, SEEK_SET);

    if (1 != HAL_FileWrite(buf, len, 1, fp)) {
        Log_e("write data to file failed");
        HAL_FileClose(fp);
        return QCLOUD_ERR_FAILURE;
    }
    HAL_FileFlush(fp);
    HAL_FileClose(fp);

    return 0;
}

char *TencentIotCSDK::_get_local_fw_running_version()
{
    // asuming the version is inside the code and binary
    // you can also get from a meta file
    Log_i("FW running version: %s", FW_RUNNING_VERSION);
    emit this->send_ota_debug_info(QString("%1%2").arg("FW running version:").arg(FW_RUNNING_VERSION));
    return (char *)FW_RUNNING_VERSION;
}
/**********************************************************************************
 * OTA file operations END
 **********************************************************************************/
// main OTA cycle
bool TencentIotCSDK::process_ota(OTAContextData *ota_ctx)
{
    bool  download_finished     = false;
    bool  upgrade_fetch_success = true;
    char  buf_ota[OTA_BUF_LEN]  = {0};
    int   rc                    = 0;
    void *h_ota                 = ota_ctx->ota_handle;
    int packet_id               = 0;
    uint32_t local_offset       = 0;
    int last_downloaded_size    = 0;

    /* Must report version first */
    if (0 > IOT_OTA_ReportVersion(h_ota, _get_local_fw_running_version())) {
        Log_e("report OTA version failed");
        upgrade_fetch_success = false;
        goto end_of_ota;
    }

    do {
        IOT_MQTT_Yield(ota_ctx->mqtt_client, 200);

        Log_i("wait for ota upgrade command...");
        emit this->send_ota_debug_info("wait for ota upgrade command...");
begin_of_ota:
        // recv the upgrade cmd
        if (IOT_OTA_IsFetching(h_ota)) {
            IOT_OTA_Ioctl(h_ota, IOT_OTAG_FILE_SIZE, &ota_ctx->fw_file_size, 4);
            IOT_OTA_Ioctl(h_ota, IOT_OTAG_VERSION, ota_ctx->remote_version, FW_VERSION_MAX_LEN);
            HAL_Snprintf(ota_ctx->fw_file_path, FW_FILE_PATH_MAX_LEN, "./FW_%s.bin", ota_ctx->remote_version);
            HAL_Snprintf(ota_ctx->fw_info_file_path, FW_FILE_PATH_MAX_LEN, "./FW_%s.json", ota_ctx->remote_version);
            /* check if pre-downloading finished or not */
            /* if local FW downloaded size (ota_ctx->downloaded_size) is not zero, it
             * will do resuming download */
            local_offset = _update_fw_downloaded_size(ota_ctx);
            if (ota_ctx->fw_file_size == local_offset) {  // have already downloaded
                upgrade_fetch_success = true;
                goto end_of_download;
            }

            /*set offset and start http connect*/
            rc = IOT_OTA_StartDownload(h_ota, ota_ctx->downloaded_size, ota_ctx->fw_file_size, OTA_HTTP_MAX_FETCHED_SIZE);
            if (QCLOUD_RET_SUCCESS != rc) {
                Log_e("OTA download start err,rc:%d", rc);
                upgrade_fetch_success = false;
                goto end_of_ota;
            }

            // download and save the fw
            do {
                int len = IOT_OTA_FetchYield(h_ota, buf_ota, OTA_BUF_LEN, 1);
                if (len > 0) {
                    rc = _save_fw_data_to_file(ota_ctx->fw_file_path, ota_ctx->downloaded_size, buf_ota, len);
                    if (rc) {
                        Log_e("write data to file failed rc=%d", rc);
                        upgrade_fetch_success = false;
                        ota_ctx->ota_fail_cnt = MAX_OTA_RETRY_CNT;
                        goto end_of_ota;
                    }
                } else if (len <= 0) {
                    Log_e("download fail rc=%d", len);
                    upgrade_fetch_success = false;
                    if (len == IOT_OTA_ERR_FETCH_AUTH_FAIL || len == IOT_OTA_ERR_FETCH_NOT_EXIST) {
                        ota_ctx->ota_fail_cnt = MAX_OTA_RETRY_CNT;
                    }
                    goto end_of_ota;
                }

                /* get OTA information and update local info */
                IOT_OTA_Ioctl(h_ota, IOT_OTAG_FETCHED_SIZE, &ota_ctx->downloaded_size, 4);
                rc = _update_local_fw_info(ota_ctx);
                if (QCLOUD_RET_SUCCESS != rc) {
                    Log_e("update local fw info err,rc:%d", rc);
                }

                // quit ota process as something wrong with mqtt
                rc = IOT_MQTT_Yield(ota_ctx->mqtt_client, 100);
                if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
                    Log_e("MQTT error: %d", rc);
                    upgrade_fetch_success = false;
                    goto end_of_ota;
                }

            } while (!IOT_OTA_IsFetchFinish(h_ota));

            /* Must check MD5 match or not */
            end_of_download:
            if (upgrade_fetch_success) {
                // download is finished, delete the fw info file
                _delete_fw_info_file(ota_ctx->fw_info_file_path);

                uint32_t firmware_valid;
                IOT_OTA_Ioctl(h_ota, IOT_OTAG_CHECK_FIRMWARE, &firmware_valid, 4);
                if (0 == firmware_valid) {
                    Log_e("The firmware is invalid");
                    emit this->send_ota_debug_info("The firmware is invalid");
                    upgrade_fetch_success = false;
                    goto end_of_ota;
                } else {
                    Log_i("The firmware is valid");
                    emit this->send_ota_debug_info("The firmware is valid");
                    upgrade_fetch_success = true;
                }
            }

            download_finished = true;
        }

        if (!download_finished)
            HAL_SleepMs(1000);

    } while (!download_finished);

    // do some post-download stuff for your need
    emit this->send_ota_success(); //OTA升级成功，向UI端推送升级成功的消息
end_of_ota:
    if (!upgrade_fetch_success) {
        // retry again
        if (IOT_MQTT_IsConnected(ota_ctx->mqtt_client)) {
            if ((ota_ctx->downloaded_size - last_downloaded_size) != OTA_HTTP_MAX_FETCHED_SIZE) {
                ota_ctx->ota_fail_cnt++;
            }
            if (ota_ctx->ota_fail_cnt <= MAX_OTA_RETRY_CNT) {
                upgrade_fetch_success = true;
                last_downloaded_size = ota_ctx->downloaded_size;
                Log_e("OTA failed, retry %drd time!", ota_ctx->ota_fail_cnt);
                HAL_SleepMs(1000);
                goto begin_of_ota;
            } else {
                ota_ctx->ota_fail_cnt = 0;
                Log_e("report download fail!");
                packet_id = IOT_OTA_ReportUpgradeFail(h_ota, NULL);
            }
        }
    } else if (upgrade_fetch_success) {
        packet_id = IOT_OTA_ReportUpgradeSuccess(h_ota, NULL);
        ota_ctx->ota_fail_cnt = 0;
    }

    _wait_for_pub_ack(ota_ctx, packet_id);

    return upgrade_fetch_success;
}

TencentIotCSDK::TencentIotCSDK(QObject *parent) : QThread(parent)
{

}

void TencentIotCSDK::run()
{
    int             rc;
    OTAContextData *ota_ctx     = NULL;
    void *          mqtt_client = NULL;
    void *          h_ota       = NULL;

    IOT_Log_Set_Level(eLOG_DEBUG);

    ota_ctx = (OTAContextData *)HAL_Malloc(sizeof(OTAContextData));
    if (ota_ctx == NULL) {
        Log_e("malloc failed");
        if (NULL != ota_ctx)
            HAL_Free(ota_ctx);

        if (NULL != h_ota)
            IOT_OTA_Destroy(h_ota);

        IOT_MQTT_Destroy(&mqtt_client);
    }
    memset(ota_ctx, 0, sizeof(OTAContextData));

    rc = HAL_GetDevInfo(&sg_devInfo);
    if (QCLOUD_RET_SUCCESS != rc) {
        Log_e("get device info failed: %d", rc);
        if (NULL != ota_ctx)
            HAL_Free(ota_ctx);

        if (NULL != h_ota)
            IOT_OTA_Destroy(h_ota);

        IOT_MQTT_Destroy(&mqtt_client);
    }

    // setup MQTT init params
    MQTTInitParams init_params = DEFAULT_MQTTINIT_PARAMS;
    rc                         = _setup_connect_init_params(&init_params, ota_ctx, &sg_devInfo);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("init params err,rc=%d", rc);
        //return rc;
        return;
    }

    // create MQTT mqtt_client and connect to server
    mqtt_client = IOT_MQTT_Construct(&init_params);
    if (mqtt_client != NULL) {
        Log_i("Cloud Device Construct Success");
        emit this->send_ota_debug_info("Cloud Device Construct Success");
    } else {
        Log_e("Cloud Device Construct Failed");
        emit this->send_ota_debug_info("Cloud Device Construct Failed");
        //return QCLOUD_ERR_FAILURE;
        return;
    }

    // init OTA handle
    h_ota = IOT_OTA_Init(sg_devInfo.product_id, sg_devInfo.device_name, mqtt_client);
    if (NULL == h_ota) {
        Log_e("initialize OTA failed");
        if (NULL != ota_ctx)
            HAL_Free(ota_ctx);

        if (NULL != h_ota)
            IOT_OTA_Destroy(h_ota);

        IOT_MQTT_Destroy(&mqtt_client);
    }

    ota_ctx->ota_handle  = h_ota;
    ota_ctx->mqtt_client = mqtt_client;

    bool ota_success;
    do {
        // mqtt should be ready first
        rc = IOT_MQTT_Yield(mqtt_client, 500);
        if (rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT) {
            HAL_SleepMs(1000);
            continue;
        } else if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
            if (rc == QCLOUD_ERR_MQTT_RECONNECT_TIMEOUT) {
                Log_e(
                    "exit. mqtt reconnect timeout! Please check the network connection, or try to increase "
                    "MAX_RECONNECT_WAIT_INTERVAL(%d)",
                    MAX_RECONNECT_WAIT_INTERVAL);
            } else {
                Log_e("exit with error: %d", rc);
            }
            break;
        }

        // OTA process
        ota_success = process_ota(ota_ctx);
        if (!ota_success) {
            Log_e("OTA failed! Do it again");
            HAL_SleepMs(2000);
        }
    } while (!ota_success);
}
