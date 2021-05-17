## 1 简介
小程序 WIFI 配网方式目前有 softap 、微信 airkiss、乐鑫 smartconfig、瑞昱 simpleconfig、蓝牙 wifi combo 配网。
![](https://cloud.tencent.com/document/product/1081/48403)

### 1. softap 配网
softap 配网方式设备的 WIFI 需要支持切换为 AP 模式。
[原理介绍](https://cloud.tencent.com/document/product/1081/48404)

### 2. airkiss 配网
airkiss 配网属于微信硬件平台的 WIFI 配网方式。
[原理介绍](https://cloud.tencent.com/document/product/1081/48406)

### 3. 乐鑫 smartconfig 配网
乐鑫 smartconfig 是使用实现了乐鑫 esptouch 协议的配网方式。
[原理介绍](https://cloud.tencent.com/document/product/1081/48405)

### 4. 瑞昱 simpleconfig 配网
瑞昱 simpleconfig 是使用实现了瑞昱 simpleconfig 协议的配网方式。
[原理介绍](https://cloud.tencent.com/document/product/1081/48407)

### 5. 蓝牙 WIFI Combo 配网
蓝牙 WIFI Combo 配网需要设备既支持 WIFI 功能还需要支持蓝牙功能，设备端与小程序通过蓝牙通信配置目标 ap 的 ssid password 和 token。
[原理介绍](https://cloud.tencent.com/document/product/1081/48408)

## 2 配网步骤

### 1. 设备开发
在设备上集成 IoT Explorer CSDK。
[设备开发](https://cloud.tencent.com/document/product/1081/48354)

CSDK 实现了接收小程序发送的 token 并向物联网开发发起绑定的流程。

适配设备所需的配网方式，在 CSDK 的 platform/wifi_config/adapter 目录下修改对应配网方式的 C 文件，参考 wifi_config_sample 例程使能选择的配网方式。

### 2. 产品开发
在平台上创建产品和设备。
[产品开发](https://cloud.tencent.com/document/product/1081/34739)

### 3. 设备身份认证
设备要接入物联网开发平台需要进行的身份认证方法。
[设备身份认证](https://cloud.tencent.com/document/product/1081/39322),
[动态注册1](https://cloud.tencent.com/document/product/1081/48376),[动态注册2](https://cloud.tencent.com/document/product/1081/47612)。


动态注册对于不支持文件系统的设备请适配设备信息的存储和获取 `HAL_SetDevInfo  HAL_GetDevInfo`。

### 4. 平台配网引导
配网引导入口

![](https://main.qcloudimg.com/raw/2a411d6d2a6113cc4956d3f1dc0bed7f.png)

配网芯片方案选择

![](https://main.qcloudimg.com/raw/f5a57d38135573b4c3b63671b7a50af7.png)

配网方式选择

![](https://main.qcloudimg.com/raw/9a14c5e0f3b90cf276c1f682d03e37c5.png)

微信或腾讯连连小程序扫描下图最底部的二维码。

![](https://main.qcloudimg.com/raw/3162d66a4833c608e8310bfd9cc1c91b.png)

## 3. 设备配网适配

根据硬件支持选择适配。

```
相关头文件： qcloud_wifi_config.h  qcloud_wifi_config_internal.h
```
### 1. softap 适配
```
适配文件：platform/wifi_config/adapter/HAL_Soft_ap.c
函数：start_device_softAP 用于将 wifi 切换为 ap 模式，需要设置默认 IP 地址为 192.168.4.1 掩码 255.255.255.0，开启 DHCP Server 功能；函数不能阻塞。
函数：stop_device_softAP 用于将 wifi 的 ap 模式停止，如切换 wifi 为 station 模式等；函数不能阻塞。
函数：_soft_ap_config_task 用于监控 softap 的配网结果和目标 ap 的连接状态， wifi_config_event_cb 函数指针将发起设备向平台的绑定流程。

适配文件：platform/wifi_config/adapter/HAL_WIFI_api.c
函数：HAL_Wifi_StaConnect 用于设备切换到 station 模式并连接到目标 ap，保存目标 ap ssid password 信息，函数返回 0 SDK 将认为设备连接目标 ap 成功；函数需要阻塞获取 wifi 连接结果并返回成功/失败/超时。
根据文件中 TO-DO 注释去调用状态/结果设置的函数。
```

### 2. airkiss 适配
```
适配文件：platform/wifi_config/adapter/HAL_Airkiss.c
函数：start_device_airkiss 用于启动 wifi 的 airkiss 配网模式。
函数：stop_device_airkiss 用于关闭 wifi 的 airkiss 配网模式。
函数：airkiss_task 用于监控 airkiss 的配网结果和目标 ap 的连接状态， 调用 wifi_config_event_cb 函数指针发起设备向平台的绑定流程。
注：设备通过 airkiss 方式获取到 SSID PASSWORD 后应自行发起 WIFI 连接过程监控连接结果；在 airkiss 获取 SSID PASSWORD 过程中根据成功或失败调用 set_airkiss_config_state 函数设置 airkiss 执行的结果。
根据文件中 TO-DO 注释去调用状态/结果设置的函数。
```

### 3. smartconfig 适配
```
适配文件：platform/wifi_config/adapter/HAL_Smart_config.c
函数：start_device_smartconfig 用于启动 wifi 的 smartconfig 配网模式。
函数：stop_device_smartconfig 用于关闭 wifi 的 smartconfig 配网模式。
函数：smart_config_task 用于监控 smartconfig 的配网结果和目标 ap 的连接状态， 调用 wifi_config_event_cb 函数指针发起设备向平台的绑定流程。
注：设备通过 smartconfig 方式获取到 SSID PASSWORD 后应自行发起 WIFI 连接过程监控连接结果；在 smartconfig 获取 SSID PASSWORD 过程中根据成功或失败调用 set_smart_config_state 函数设置 smartconfig 执行的结果。
根据文件中 TO-DO 注释去调用状态/结果设置的函数。
```

### 4. simpleconfig 适配
```
适配文件：platform/wifi_config/adapter/HAL_Simple_config.c
函数：start_device_simpleconfig 用于启动 wifi 的 simpleconfig 配网模式。
函数：stop_device_simpleconfig 用于关闭 wifi 的 simpleconfig 配网模式。
函数：simple_config_task 用于监控 simpleconfig 的配网结果和目标 ap 的连接状态， 调用 wifi_config_event_cb 函数指针发起设备向平台的绑定流程。
注：设备通过 simpleconfig 方式获取到 SSID PASSWORD 后应自行发起 WIFI 连接过程监控连接结果；在 simpleconfig 获取 SSID PASSWORD 过程中根据成功或失败调用 set_simple_config_state 函数设置 simpleconfig 执行的结果。
根据文件中 TO-DO 注释去调用状态/结果设置的函数。
```

### 5. 蓝牙 combo 适配
LLSync协议配网请[参考](https://github.com/tencentyun/qcloud-iot-explorer-BLE-sdk-embedded/blob/master/docs/LLSync%20SDK%E8%BE%85%E5%8A%A9%E9%85%8D%E7%BD%91%E5%8A%9F%E8%83%BD%E6%8E%A5%E5%85%A5%E6%8C%87%E5%BC%95.md)  
Bluefi协议配网指引如下：
```
适配文件：platform/wifi_config/adapter/HAL_BTCombo_config.c
函数：start_device_btcomboconfig 用于启动 wifi 的蓝牙 combo 配网模式。
函数：stop_device_btcomboconfig 用于关闭 wifi 的蓝牙 combo 配网模式。
函数：bt_combo_config_task 用于监控蓝牙 combo 的配网结果和目标 ap 的连接状态， 调用 wifi_config_event_cb 函数指针发起设备向平台的绑定流程。调用 bt_combo_report_bind_result 函数向小程序发送设备 token 绑定结果。
函数：bt_combo_send_message 用于设备通过蓝牙向小程序发送消息。
注：设备通过 BLE 方式获取到 SSID PASSWORD 后应自行发起 WIFI 连接过程监控连接结果；在 BLE 获取 SSID PASSWORD 过程中根据成功或失败调用 set_bt_combo_config_state 函数设置 BLE 获取 SSID PASSWORD的结果；BLE 获取到 token 时调用 qiot_device_bind_set_token 函数设置 token。
注：蓝牙 combo 配网过程中设备与小程序的蓝牙交互必须要按照 docs/腾讯连连蓝牙辅助配网协议V1.1.pdf 文档开发。
根据文件中 TO-DO 注释去调用状态/结果设置的函数。
```

### 6. Log 适配
```
Log 是 WIFI 配网阶段产生的日志数据。
适配文件：platform/wifi_config/adapter/HAL_Soft_ap.c
函数：start_device_softAP 用于将 wifi 切换为 ap 模式，需要设置默认 IP 地址为 192.168.4.1 掩码 255.255.255.0，开启 DHCP Server 功能。
函数：stop_device_softAP 用于将 wifi 的 ap 模式停止，如切换 wifi 为 station 模式等。
以上函数适配后，当配网失败时，调用函数 qiot_wifi_config_send_log 开启日志上报，小程序可以连接到设备 SosftAP 上，获取配网日志到小程序。

文件：platform/wifi_config/adapter/HAL_WIFI_api.c
函数：HAL_Wifi_save_err_log 用于保存设备配网期间的出错日志。
函数：HAL_Wifi_check_err_log 用于获取保存的设备在配网期间出错的日志数量。
函数：HAL_Wifi_load_err_log 用于读取保存的设备在配网期间出错的日志。
函数：HAL_Wifi_clear_err_log 用于清除保存的设备在配网期间的出错日志。
```

### 7. 配网必须适配
```
适配文件：platform/wifi_config/adapter/HAL_WIFI_api.c
函数：HAL_Wifi_IsConnected 用于配网流程中判断设备是否连接 WIFI 成功并获取到了 IP 地址，从而进行设备绑定过程。
```

### 8. UDP 通信适配
```
适配文件：platform/os/xxx/HAL_UDP_xxx.c
函数：HAL_UDP_CreateBind 用于创建本地 UDP socket，绑定本地端口号 8266
函数：HAL_UDP_ReadTimeoutPeerInfo 用于超时读取数据并返回 UDP 对端 IP Port 信息，以供回复使用
函数：HAL_UDP_WriteTo 用于发送数据给入参的 host 和 port
函数：HAL_UDP_Close 用于关闭本地 UDP socket
函数：HAL_UDP_GetErrno 获取最近一次 socket 通信的 errno 值
函数：HAL_UDP_GetErrnoStr 获取最近一次的 socket 通信的 errno 对应的字符串。
以上函数 SDK 已适配了 platform/os/linux/HAL_UDP_linux.c platform/os/windows/HAL_UDP_win.c platform/os/freertos/HAL_UDP_lwip.c，使用 lwip tcp/ip 协议栈的可使用 platform/os/freertos/HAL_UDP_lwip.c 该文件；其他的 tcp/ip 协议栈请自行参考适配。
```

### 9. 配网时序图
SoftAP 配网绑定时序图
![softp 时序图](https://main.qcloudimg.com/raw/9a53ecc101db5d11827fd15e92fdace7.png)

Airkiss/Smartconfig/Simpleconfig 配网绑定时序图
![airkiss 时序图](https://main.qcloudimg.com/raw/590d10b07942df6fc4c69c1620369cee.png)

BLE Combo 配网绑定时序图
![BLE Combo 时序图](https://main.qcloudimg.com/raw/c38dd46f85d0a09869ecf10bd7f0e535.png)

## 4. 编译 SDK 运行示例程序

### 1. 编译 SDK

修改CMakeLists.txt确保以下选项存在(以密钥认证设备为例)
```
set(BUILD_TYPE                   "release")
set(COMPILE_TOOLS                "gcc")
set(PLATFORM 	                "linux")
set(FEATURE_MQTT_COMM_ENABLED ON)
set(FEATURE_AUTH_MODE "KEY")
set(FEATURE_AUTH_WITH_NOTLS OFF)
set(FEATURE_DEBUG_DEV_INFO_USED  OFF)
set(FEATURE_WIFI_CONFIG_ENABLED  ON)
#开启 WIFI 配网功能
set(FEATURE_WIFI_CONFIG_ENABLED  ON)
#根据需要开启动态注册功能
set(FEATURE_DEV_DYN_REG_ENABLED  ON)
```
执行脚本编译
```
./cmake_build.sh 
```
示例程序输出位于`output/release/bin`文件夹中

#### 2. 填写设备信息
产品开发阶段在腾讯云物联网开发平台创建的产品的设备信息(以**密钥认证产品动态注册**为例)填写到 device_info.json 中
```
{
	"auth_mode":"KEY",
	"productId":"L4WG9V9M2M",
	"deviceName":"TEST2",
	"region":"china",
	"productSecret":"XoezQjzzMedF4lAr2CqCkVYK",
	"key_deviceinfo":{
		"deviceSecret":"YOUR_IOT_PSK"
	}
}
```

#### 3. 执行 wifi_config_sample 示例程序
示例程序 wifi_config_sample
示例程序未适配任何硬件平台，仅做流程演示。
带有 WiFi 功能或插了无线网卡和蓝牙模块的 windows / linux 可自行适配 softap / airkiss / 蓝牙 combo 配网方式。
例程日志是在 ubuntu 上准备了有线网卡访问外网，无线网卡通过 hostpad 切换为 ap 进行的测试。
```
./output/release/bin/wifi_config_sample 
INF|2021-01-22 15:19:04|HAL_Wifi_api.c|HAL_Wifi_read_err_log(47): HAL_Wifi_check_err_log
INF|2021-01-22 15:19:04|qcloud_wifi_config_error_handle.c|check_err_log(89): invalid magic code: 0x8
DBG|2021-01-22 15:19:04|HAL_Soft_ap.c|HAL_SoftApProvision_Start(115): start softAP success!
DBG|2021-01-22 15:19:04|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:04|HAL_UDP_linux.c|HAL_UDP_CreateBind(183): establish tcp connection with server(host=0.0.0.0 port=8266)
DBG|2021-01-22 15:19:04|HAL_UDP_linux.c|HAL_UDP_CreateBind(197): establish tcp connection with server(host=0 port=18976)
DBG|2021-01-22 15:19:04|HAL_UDP_linux.c|HAL_UDP_CreateBind(212): success to establish udp, fd=3
INF|2021-01-22 15:19:04|HAL_UDP_linux.c|HAL_UDP_CreateBind(217): HAL_UDP_CreateBind fd:3
INF|2021-01-22 15:19:04|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(265): UDP server socket listening...
INF|2021-01-22 15:19:04|HAL_UDP_linux.c|HAL_UDP_ReadTimeoutPeerInfo(235): HAL_UDP_ReadTimeoutPeerInfo fd:3
DBG|2021-01-22 15:19:05|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:06|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:06|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
DBG|2021-01-22 15:19:07|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:07|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(293): wait for read...
INF|2021-01-22 15:19:07|HAL_UDP_linux.c|HAL_UDP_ReadTimeoutPeerInfo(235): HAL_UDP_ReadTimeoutPeerInfo fd:3
DBG|2021-01-22 15:19:08|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:08|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
DBG|2021-01-22 15:19:09|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:10|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:10|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(293): wait for read...
INF|2021-01-22 15:19:10|HAL_UDP_linux.c|HAL_UDP_ReadTimeoutPeerInfo(235): HAL_UDP_ReadTimeoutPeerInfo fd:3
DBG|2021-01-22 15:19:10|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
DBG|2021-01-22 15:19:11|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:12|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:12|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
DBG|2021-01-22 15:19:13|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:13|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(293): wait for read...
INF|2021-01-22 15:19:13|HAL_UDP_linux.c|HAL_UDP_ReadTimeoutPeerInfo(235): HAL_UDP_ReadTimeoutPeerInfo fd:3
DBG|2021-01-22 15:19:14|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:14|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
DBG|2021-01-22 15:19:15|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:16|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:16|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(293): wait for read...
INF|2021-01-22 15:19:16|HAL_UDP_linux.c|HAL_UDP_ReadTimeoutPeerInfo(235): HAL_UDP_ReadTimeoutPeerInfo fd:3
DBG|2021-01-22 15:19:16|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
DBG|2021-01-22 15:19:17|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:18|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:18|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
INF|2021-01-22 15:19:18|HAL_UDP_linux.c|HAL_UDP_ReadTimeoutPeerInfo(264): HAL_UDP_ReadTimeoutPeerInfo SELECT SUCCSS fd:3
INF|2021-01-22 15:19:18|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(278): Received 100 bytes from <192.168.4.66:47482> msg: {"cmdType":1,"ssid":"xxxxxxxx","password":"xxxxxxxx","token":"69f978dfbb48ec40cadb6d9af470d0f6"}
Report dev info(93): {
	"cmdType":	2,
	"productId":	"L4WG9V9M2M",
	"deviceName":	"TEST2",
	"protoVersion":	"2.0"
}INF|2021-01-22 15:19:18|HAL_UDP_linux.c|HAL_UDP_WriteTo(285): HAL_UDP_WriteTo host:192.168.4.66, port:47482, fd:3
DBG|2021-01-22 15:19:18|HAL_UDP_linux.c|HAL_UDP_WriteTo(303): HAL_UDP_WriteTo(host=1107601600 port=31417)
DBG|2021-01-22 15:19:19|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
INF|2021-01-22 15:19:19|HAL_UDP_linux.c|HAL_UDP_WriteTo(285): HAL_UDP_WriteTo host:192.168.4.66, port:47482, fd:3
DBG|2021-01-22 15:19:19|HAL_UDP_linux.c|HAL_UDP_WriteTo(303): HAL_UDP_WriteTo(host=1107601600 port=31417)
DBG|2021-01-22 15:19:20|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:20|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
INF|2021-01-22 15:19:20|HAL_UDP_linux.c|HAL_UDP_WriteTo(285): HAL_UDP_WriteTo host:192.168.4.66, port:47482, fd:3
DBG|2021-01-22 15:19:20|HAL_UDP_linux.c|HAL_UDP_WriteTo(303): HAL_UDP_WriteTo(host=1107601600 port=31417)
Report dev info: {
	"cmdType":	2,
	"productId":	"L4WG9V9M2M",
	"deviceName":	"TEST2",
	"protoVersion":	"2.0"
}DBG|2021-01-22 15:19:21|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:22|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:22|HAL_Soft_ap.c|_soft_ap_config_task(57): soft_ap_task running!
DBG|2021-01-22 15:19:23|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
INF|2021-01-22 15:19:23|qcloud_wifi_config_comm_service.c|_app_handle_recv_data(194): STA to connect SSID:xxxxxxxx PASSWORD:xxxxxxxx
INF|2021-01-22 15:19:23|HAL_Wifi_api.c|HAL_Wifi_StaConnect(33): STA to connect SSID:xxxxxxxx PASSWORD:xxxxxxxx CHANNEL:0
DBG|2021-01-22 15:19:23|qcloud_wifi_config_comm_service.c|_app_handle_recv_data(208): wifi_sta_connect success
WRN|2021-01-22 15:19:23|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(285): Finish app cmd handling.
WRN|2021-01-22 15:19:23|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(312): Shutting down UDP server socket
INF|2021-01-22 15:19:23|qcloud_wifi_config_comm_service.c|_qiot_comm_service_task(317): UDP server task quit
DBG|2021-01-22 15:19:24|qcloud_wifi_config_device_bind.c|qiot_device_bind(392): device bind start
DBG|2021-01-22 15:19:24|qcloud_wifi_config_device_bind.c|_get_reg_dev_info(288): dev psk not exist, do dyn reg!
DBG|2021-01-22 15:19:24|dynreg.c|IOT_DynReg_Device(469): sign:YWM2ZjM1MDZlYzE4N2M1NGNiYjIwZTcwY2E1YTY5NzIyMWM0Mjg2NA==
DBG|2021-01-22 15:19:24|dynreg.c|IOT_DynReg_Device(485): request:{"deviceName":"TEST2","nonce":1005258070,"productId":"L4WG9V9M2M","timestamp":1611299964,"signature":"YWM2ZjM1MDZlYzE4N2M1NGNiYjIwZTcwY2E1YTY5NzIyMWM0Mjg2NA=="}
DBG|2021-01-22 15:19:24|HAL_TLS_mbedtls.c|_mbedtls_client_init(156): psk/pskid is empty!
DBG|2021-01-22 15:19:24|HAL_TLS_mbedtls.c|HAL_TLS_Connect(224): Setting up the SSL/TLS structure...
DBG|2021-01-22 15:19:24|HAL_TLS_mbedtls.c|HAL_TLS_Connect(266): Performing the SSL/TLS handshake...
DBG|2021-01-22 15:19:24|HAL_TLS_mbedtls.c|HAL_TLS_Connect(267): Connecting to /gateway.tencentdevices.com/443...
DBG|2021-01-22 15:19:24|HAL_TLS_mbedtls.c|HAL_TLS_Connect(289): connected with /gateway.tencentdevices.com/443...
DBG|2021-01-22 15:19:24|utils_httpc.c|qcloud_http_client_connect(745): http client connect success
DBG|2021-01-22 15:19:24|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:25|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:26|dynreg.c|_parse_devinfo(239): recv: {"code":0,"message":"success","len":53,"payload":"sf1gANtJCT/PRC4ZHOypfkLnO53otVR/oBAnD3vXhS2m8XkbQ08bTCD+I1Tz5Jjo7vhAnC6QE3RgWWZGg6pWGg==","requestId":"27d8557c-8a38-4f7b-affb-7fe5bbbf75b1"}
DBG|2021-01-22 15:19:26|dynreg.c|_parse_devinfo(253): payload:sf1gANtJCT/PRC4ZHOypfkLnO53otVR/oBAnD3vXhS2m8XkbQ08bTCD+I1Tz5Jjo7vhAnC6QE3RgWWZGg6pWGg==
DBG|2021-01-22 15:19:26|dynreg.c|IOT_DynReg_Device(489): request dev info success
DBG|2021-01-22 15:19:26|HAL_Device_linux.c|iot_save_devinfo_to_json_file(471): JsonDoc(195):{
"auth_mode":"KEY",
"productId":"L4WG9V9M2M",
"deviceName":"TEST2",
"region":"china",
"productSecret":"XoezQjzzMedF4lAr2CqCkVYK",
"key_deviceinfo":{
"deviceSecret":"MLvMHde65nKPk0Lx+ZThaA=="
}
}
DBG|2021-01-22 15:19:26|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:26|HAL_TLS_mbedtls.c|HAL_TLS_Connect(224): Setting up the SSL/TLS structure...
DBG|2021-01-22 15:19:26|HAL_TLS_mbedtls.c|HAL_TLS_Connect(266): Performing the SSL/TLS handshake...
DBG|2021-01-22 15:19:26|HAL_TLS_mbedtls.c|HAL_TLS_Connect(267): Connecting to /L4WG9V9M2M.iotcloud.tencentdevices.com/8883...
DBG|2021-01-22 15:19:27|HAL_TLS_mbedtls.c|HAL_TLS_Connect(289): connected with /L4WG9V9M2M.iotcloud.tencentdevices.com/8883...
INF|2021-01-22 15:19:27|mqtt_client.c|IOT_MQTT_Construct(125): mqtt connect with id: Kex7h success
INF|2021-01-22 15:19:27|qcloud_wifi_config_device_bind.c|_setup_mqtt_connect(332): Cloud Device Construct Success
DBG|2021-01-22 15:19:27|mqtt_client_subscribe.c|qcloud_iot_mqtt_subscribe(147): topicName=$thing/down/service/L4WG9V9M2M/TEST2|packet_id=63932
DBG|2021-01-22 15:19:27|qcloud_wifi_config_device_bind.c|_mqtt_event_handler(53): mqtt topic subscribe success
DBG|2021-01-22 15:19:27|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:28|mqtt_client_publish.c|qcloud_iot_mqtt_publish(339): publish topic seq=63933|topicName=$thing/up/service/L4WG9V9M2M/TEST2|payload={"method":"app_bind_token","clientToken":"TEST2-687232230","params": {"token":"69f978dfbb48ec40cadb6d9af470d0f6"}}
INF|2021-01-22 15:19:28|qcloud_wifi_config_device_bind.c|_mqtt_event_handler(68): publish success, packet-id=63933
DBG|2021-01-22 15:19:28|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
INF|2021-01-22 15:19:28|qcloud_wifi_config_device_bind.c|_on_message_callback(101): recv msg topic: $thing/down/service/L4WG9V9M2M/TEST2
INF|2021-01-22 15:19:28|qcloud_wifi_config_device_bind.c|_on_message_callback(112): msg payload: {"method":"app_bind_token_reply","clientToken":"TEST2-687232230","code":0,"status":"success"}
DBG|2021-01-22 15:19:28|qcloud_wifi_config_device_bind.c|_on_message_callback(123): token reply code = 0
INF|2021-01-22 15:19:29|qcloud_wifi_config_device_bind.c|_send_token_wait_reply(249): wait for token sending result...
DBG|2021-01-22 15:19:29|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
INF|2021-01-22 15:19:30|mqtt_client_connect.c|qcloud_iot_mqtt_disconnect(468): mqtt disconnect!
INF|2021-01-22 15:19:30|mqtt_client.c|IOT_MQTT_Destroy(189): mqtt release!
DBG|2021-01-22 15:19:30|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:31|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:32|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:33|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
DBG|2021-01-22 15:19:34|wifi_config_sample.c|qcloud_wifi_config_proc(486): wait wifi config result...
INF|2021-01-22 15:19:35|qcloud_wifi_config_device_bind.c|qiot_device_bind(398): WIFI_MQTT_CONNECT_SUCCESS
DBG|2021-01-22 15:19:35|qcloud_wifi_config.c|_qiot_wifi_config_event_cb(53): Device bind success!
DBG|2021-01-22 15:19:35|wifi_config_sample.c|_wifi_config_result_cb(454): entry...
INF|2021-01-22 15:19:35|wifi_config_sample.c|_wifi_config_result_cb(458): WiFi is ready, to do Qcloud IoT demo
DBG|2021-01-22 15:19:35|wifi_config_sample.c|_wifi_config_result_cb(459): timestamp now:1611299975
DBG|2021-01-22 15:19:35|HAL_TLS_mbedtls.c|HAL_TLS_Connect(224): Setting up the SSL/TLS structure...
DBG|2021-01-22 15:19:35|HAL_TLS_mbedtls.c|HAL_TLS_Connect(266): Performing the SSL/TLS handshake...
DBG|2021-01-22 15:19:35|HAL_TLS_mbedtls.c|HAL_TLS_Connect(267): Connecting to /L4WG9V9M2M.iotcloud.tencentdevices.com/8883...
DBG|2021-01-22 15:19:35|HAL_TLS_mbedtls.c|HAL_TLS_Connect(289): connected with /L4WG9V9M2M.iotcloud.tencentdevices.com/8883...
INF|2021-01-22 15:19:35|mqtt_client.c|IOT_MQTT_Construct(125): mqtt connect with id: P1N89 success
INF|2021-01-22 15:19:35|wifi_config_sample.c|main(586): Cloud Device Construct Success
DBG|2021-01-22 15:19:35|mqtt_client_subscribe.c|qcloud_iot_mqtt_subscribe(147): topicName=$thing/down/property/L4WG9V9M2M/TEST2|packet_id=60373
INF|2021-01-22 15:19:35|wifi_config_sample.c|_mqtt_event_handler(94): subscribe success, packet-id=60373
DBG|2021-01-22 15:19:36|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/L4WG9V9M2M/TEST2|payload={"method":"get_status", "clientToken":"L4WG9V9M2M-1"}
INF|2021-01-22 15:19:36|wifi_config_sample.c|property_message_callback(322): Receive Message With topicName:$thing/down/property/L4WG9V9M2M/TEST2, payload:{"method":"get_status_reply","clientToken":"L4WG9V9M2M-1","code":0,"status":"success","data":{}}
ERR|2021-01-22 15:19:36|wifi_config_sample.c|property_get_status_reply_handle(278): Fail to parse report
DBG|2021-01-22 15:19:37|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/L4WG9V9M2M/TEST2|payload={"method":"report", "clientToken":"L4WG9V9M2M-1", "params":{"power_switch":0, "color":0, "brightness":0, "name":"TEST2"}}
INF|2021-01-22 15:19:37|mqtt_client_connect.c|qcloud_iot_mqtt_disconnect(468): mqtt disconnect!
INF|2021-01-22 15:19:37|mqtt_client.c|IOT_MQTT_Destroy(189): mqtt release!

```






