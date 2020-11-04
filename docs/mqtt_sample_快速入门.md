## 快速开始
本文档将讲述如何在腾讯云物联网开发平台IoT Explorer控制台申请设备，并结合`mqtt_sample`示例快速体验设备端连接到腾讯云`IoT Explorer
`使用数据模版进行消息通信。本示例提供了使用`MQTT`协议简单实现数据模版功能的一种方法，SDK对数据模版功能已经做了封装，用户可以直接使用，请参考`data_template_sample`例程。

### 一 控制台创建设备
1. 注册/登陆腾讯云账号  
访问[腾讯云登录页面](https://cloud.tencent.com/login?s_url=https%3A%2F%2Fcloud.tencent.com%2F), 点击[立即注册](https://cloud.tencent.com/register?s_url=https%3A%2F%2Fcloud.tencent.com%2F), 免费获取腾讯云账号，若您已有账号，可直接登录。
2. 访问物联网开发平台  
登录后点击右上角控制台，进入控制台后，鼠标悬停在云产品上，弹出层叠菜单，点击物联网开发平台
![](https://main.qcloudimg.com/raw/c50c7f33b2cdcb17d8f6ddbc9b7bea08.png)  
或者直接访问[物联网开发平台](https://console.cloud.tencent.com/iotexplorer)
3. 创建产品和设备  
点击**新建项目**  
![](https://main.qcloudimg.com/raw/104afefe53c8e5b25c3bf185da9f81b0.png)  
进入项目后，点击**新建产品**  
![](https://main.qcloudimg.com/raw/10834921cca8db1ff4f62b00a80a5c8e.png)
进入新产品后，步骤4是**新建设备**    
![](https://main.qcloudimg.com/raw/e0570841878d9d770c57847d4d6700ed.png)  
***数据模版资料请参考[数据模版](https://cloud.tencent.com/document/product/1081/34916)***   

### 二 编译运行示例程序
1. 编译SDK  
修改`CMakeLists.txt`确保以下选项存在（以密钥认证设备为例）
    ```c
    set(BUILD_TYPE                   "release")
    set(COMPILE_TOOLS                "gcc") 
    set(PLATFORM 	                 "linux")
    set(FEATURE_MQTT_COMM_ENABLED ON)
    set(FEATURE_AUTH_MODE "KEY")
    set(FEATURE_AUTH_WITH_NOTLS OFF)
    set(FEATURE_DEBUG_DEV_INFO_USED  OFF)
    ```
   执行编译脚本
    ```c
    ./cmake_build.sh
    ```
    示例输出位于`output/release/bin`文件夹中。
2. 填写设备信息
将上面在腾讯云物联网开发平台IoT Explorer创建的设备的设备信息(以密钥认证设备为例)填写到device_info.json中
    ```c
    {
        "auth_mode":"KEY",
        "productId":"89FZMW5XT9",
        "deviceName":"led_001",
        "key_deviceinfo":{    
            "deviceSecret":"n0NjeTJA5xPeG4bhJTeZAA=="
        }
    }
    ```
3. 执行mqtt_sample示例程序  
    ```c
    tencent-cloud-iot@:output/release/bin$ ./mqtt_sample -c ../../../device_info.json -l 1000
    DBG|2020-09-14 10:59:37|HAL_TLS_mbedtls.c|HAL_TLS_Connect(223): Setting up the SSL/TLS structure...
    DBG|2020-09-14 10:59:37|HAL_TLS_mbedtls.c|HAL_TLS_Connect(265): Performing the SSL/TLS handshake...
    DBG|2020-09-14 10:59:37|HAL_TLS_mbedtls.c|HAL_TLS_Connect(266): Connecting to /89FZMW5XT9.iotcloud.tencentdevices.com/8883...
    INF|2020-09-14 10:59:38|HAL_TLS_mbedtls.c|HAL_TLS_Connect(288): connected with /89FZMW5XT9.iotcloud.tencentdevices.com/8883...
    INF|2020-09-14 10:59:38|mqtt_client.c|IOT_MQTT_Construct(125): mqtt connect with id: 7e37S success
    INF|2020-09-14 10:59:38|mqtt_sample.c|main(466): Cloud Device Construct Success
    DBG|2020-09-14 10:59:38|mqtt_client_subscribe.c|qcloud_iot_mqtt_subscribe(147): topicName=$thing/down/property/89FZMW5XT9/led_001|packet_id=58677
    INF|2020-09-14 10:59:38|mqtt_sample.c|_mqtt_event_handler(84): subscribe success, packet-id=58677
    DBG|2020-09-14 10:59:39|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"get_status", "clientToken":"89FZMW5XT9-1"}
    INF|2020-09-14 10:59:39|mqtt_sample.c|property_message_callback(309): Receive Message With topicName:$thing/down/property/89FZMW5XT9/led_001, payload:{"method":"get_status_reply","clientToken":"89FZMW5XT9-1","code":0,"status":"success","data":{"reported":{"power_switch":0,"color":0,"brightness":90,"name":"led_001"}}}
    INF|2020-09-14 10:59:39|mqtt_sample.c|property_get_status_reply_handle(272): 	power_switch     = 0         
    INF|2020-09-14 10:59:39|mqtt_sample.c|property_get_status_reply_handle(272): 	brightness       = 90        
    INF|2020-09-14 10:59:39|mqtt_sample.c|property_get_status_reply_handle(272): 	color            = 0         
    INF|2020-09-14 10:59:39|mqtt_sample.c|property_get_status_reply_handle(272): 	name             = led_001   
    DBG|2020-09-14 10:59:40|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"report", "clientToken":"89FZMW5XT9-1", "params":{"power_switch":0, "color":0, "brightness":90, "name":"led_001"}}
    INF|2020-09-14 10:59:40|mqtt_sample.c|property_message_callback(309): Receive Message With topicName:$thing/down/property/89FZMW5XT9/led_001, payload:{"method":"report_reply","clientToken":"89FZMW5XT9-1","code":0,"status":"success"}
    INF|2020-09-14 10:59:40|mqtt_sample.c|property_report_reply_handle(293): report reply status: success
    DBG|2020-09-14 10:59:42|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"report", "clientToken":"89FZMW5XT9-2", "params":{"power_switch":0, "color":0, "brightness":91, "name":"led_001"}}
    INF|2020-09-14 10:59:42|mqtt_sample.c|property_message_callback(309): Receive Message With topicName:$thing/down/property/89FZMW5XT9/led_001, payload:{"method":"report_reply","clientToken":"89FZMW5XT9-2","code":0,"status":"success"}
    INF|2020-09-14 10:59:42|mqtt_sample.c|property_report_reply_handle(293): report reply status: success
    DBG|2020-09-14 10:59:43|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"report", "clientToken":"89FZMW5XT9-3", "params":{"power_switch":0, "color":0, "brightness":92, "name":"led_001"}}
    INF|2020-09-14 10:59:43|mqtt_sample.c|property_message_callback(309): Receive Message With topicName:$thing/down/property/89FZMW5XT9/led_001, payload:{"method":"control","clientToken":"clientToken-31dcede2-04d4-4ed5-af24-098916a2d413","params":{"name":"","testfloat":1,"color":0,"power_switch":1,"brightness":88}}
    INF|2020-09-14 10:59:43|mqtt_sample.c|property_control_handle(236): 	power_switch     = 1         
    INF|2020-09-14 10:59:43|mqtt_sample.c|property_control_handle(236): 	brightness       = 88        
    INF|2020-09-14 10:59:43|mqtt_sample.c|property_control_handle(236): 	color            = 0         
    DBG|2020-09-14 10:59:43|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"control_reply", "code":0, "clientToken":"clientToken-31dcede2-04d4-4ed5-af24-098916a2d413"}
    ```
4. 观察数据模版`get_status`方法
设备开机后发送`get_status`到腾讯云物联网开发平台IoT Explorer获取设备最新状态
    ```c
    DBG|2020-09-14 10:59:39|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"get_status", "clientToken":"89FZMW5XT9-1"}
    ```
    腾讯云物联网通信平台IoT Explorer回复设备最新状态
    ```c
    INF|2020-09-14 10:59:39|mqtt_sample.c|property_message_callback(309): Receive Message With topicName:$thing/down/property/89FZMW5XT9/led_001, payload:{"method":"get_status_reply","clientToken":"89FZMW5XT9-1","code":0,"status":"success","data":{"reported":{"power_switch":0,"color":0,"brightness":90,"name":"led_001"}}}
    ```
5. 观察数据模版`report`方法
    设备属性修改后发送`report`到腾讯云物联网开发平台IoT Explorer，上报设备最新信息
    ```c
    DBG|2020-09-14 10:59:40|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"report", "clientToken":"89FZMW5XT9-1", "params":{"power_switch":0, "color":0, "brightness":90, "name":"led_001"}}
    ```
    腾讯云物联网通信平台IoT Explorer回复确认
    ```c
    INF|2020-09-14 10:59:40|mqtt_sample.c|property_message_callback(309): Receive Message With topicName:$thing/down/property/89FZMW5XT9/led_001, payload:{"method":"report_reply","clientToken":"89FZMW5XT9-1","code":0,"status":"success"}
    ```
6. 观察数据模版`control`方法
腾讯云物联网通信平台IoT Explorer通过云API修改设备属性
![](https://main.qcloudimg.com/raw/306893b2b8653b397f033b61660906f7.png)  
    在后台修改设备属性后，设备端收到消息
    ```c
    INF|2020-09-14 10:59:43|mqtt_sample.c|property_message_callback(309): Receive Message With topicName:$thing/down/property/89FZMW5XT9/led_001, payload:{"method":"control","clientToken":"clientToken-31dcede2-04d4-4ed5-af24-098916a2d413","params":{"name":"","testfloat":1,"color":0,"power_switch":1,"brightness":88}}
    ```  
    设备收到消息后作出回复确认  
    ```c
    DBG|2020-09-14 10:59:43|mqtt_client_publish.c|qcloud_iot_mqtt_publish(345): publish packetID=0|topicName=$thing/up/property/89FZMW5XT9/led_001|payload={"method":"control_reply", "code":0, "clientToken":"clientToken-31dcede2-04d4-4ed5-af24-098916a2d413"}
    ```
