- [设备端相关文档](#设备端相关文档)
- [编译问题](#编译问题)
  - [如何将`C SDK`编译到其他目标平台？](#如何将c-sdk编译到其他目标平台)
  - [编译报错 `error Only C99 allowed`](#编译报错-error-only-c99-allowed)
  - [编译后不能在目标设备上运行](#编译后不能在目标设备上运行)
- [设备接入问题](#设备接入问题)
  - [使用`MQTT.fx`连接物联网开发平台失败？](#使用mqttfx连接物联网开发平台失败)
  - [设备连接物联网开发平台失败？](#设备连接物联网开发平台失败)
  - [动态注册失败？](#动态注册失败)
  - [`Topic`订阅失败是什么原因？](#topic订阅失败是什么原因)
  - [设备日志中的错误码都表示什么含义？](#设备日志中的错误码都表示什么含义)
  - [`C SDK`是否支持自定义透传数据？](#c-sdk是否支持自定义透传数据)
  - [如何在控制台查看设备上报信息？](#如何在控制台查看设备上报信息)
  - [数据模版解析失败？](#数据模版解析失败)
  - [控制台如何判断设备在线？](#控制台如何判断设备在线)
  - [如何修改设备端接入域名？](#如何修改设备端接入域名)
  - [如何从物联网开发平台获取`UTC`时间？](#如何从物联网开发平台获取utc时间)
  - [如何实现设备断开连接后的持续重连?](#如何实现设备断开连接后的持续重连)
- [配网问题](#配网问题)
  - [设备端支持哪些配网方式？](#设备端支持哪些配网方式)
  - [蓝牙配网时,设备已经联网成功,但是小程序显示配网失败？](#蓝牙配网时设备已经联网成功但是小程序显示配网失败)
  - [Smartconfig配网时,一直提示失败？](#smartconfig配网时一直提示失败)
  - [Soft AP配网时,手机无法连接设备创建热点？](#soft-ap配网时手机无法连接设备创建热点)
  - [向后台发送token的时候,收到的`bind_device`消息作用是什么？](#向后台发送token的时候收到的bind_device消息作用是什么)
  - [设备端配网示例程序](#设备端配网示例程序)
- [AT模组问题](#at模组问题)
  - [使用`AT`模组,设备无法连接物联网开发平台？](#使用at模组设备无法连接物联网开发平台)
  - [如何升级`AT`模组？](#如何升级at模组)
  - [AT模组配网时,如何使用动态注册功能？](#at模组配网时如何使用动态注册功能)
- [其他问题](#其他问题)
  - [使用ESP8266 + C SDK开发时,会出现挂死现象?](#使用esp8266--c-sdk开发时会出现挂死现象)


## 设备端相关文档
1.  [SDK快速入门](https://cloud.tencent.com/document/product/1081/34732) : 物联网开发平台的用户，可以通过快速入门的 Demo 示例加快对开发平台的了解。
2. [设备开发指南](https://cloud.tencent.com/document/product/1081/48329) : 包括[SDK使用参考](https://cloud.tencent.com/document/product/1081/48357)，[直连设备接入指引](https://cloud.tencent.com/document/product/1081/48358)、[蓝牙设备接入指引](https://cloud.tencent.com/document/product/1081/48359)、[网关设备接入指引](https://cloud.tencent.com/document/product/1081/48360)，[设备配网指引](https://cloud.tencent.com/document/product/1081/48361)等文档。
3. [设备身份认证](https://cloud.tencent.com/document/product/1081/39321) : 物联网开发平台提供了证书认证和密钥认证两种认证方式，密钥认证又可以分为[产品级密钥认证](https://cloud.tencent.com/document/product/1081/47494)和[设备级密钥认证](https://cloud.tencent.com/document/product/1081/47497)。请参考[设备信息存储](https://cloud.tencent.com/document/product/1081/48376)持久化存储设备认证信息。
4. [产品开发流程](https://cloud.tencent.com/document/product/1081/34738) : 包括[产品定义](https://cloud.tencent.com/document/product/1081/34739)、[数据模版](https://cloud.tencent.com/document/product/1081/44921)、[设备开发](https://cloud.tencent.com/document/product/1081/34740)、[交互开发](https://cloud.tencent.com/document/product/1081/40457)、[设备调试](https://cloud.tencent.com/document/product/1081/34741)、[批量投产](https://cloud.tencent.com/document/product/1081/34742)等产品开发过程文档。
5. [数据模版资料](https://cloud.tencent.com/document/product/1081/44921) : 可以参考[数据模版代码生成](https://cloud.tencent.com/document/product/1081/48374)和[数据模版应用开发](https://cloud.tencent.com/document/product/1081/48375)进行数据模版开发。
6. [设备协议文档](https://cloud.tencent.com/document/product/1081/47335) : 包括[物模型协议](https://cloud.tencent.com/document/product/1081/34916)、[固件升级协议](https://cloud.tencent.com/document/product/1081/39359)、[资源管理协议](https://cloud.tencent.com/document/product/1081/60187)、[文件管理协议](https://cloud.tencent.com/document/product/1081/60186)等协议文档。
## 编译问题
### 如何将`C SDK`编译到其他目标平台？
  `C SDK`支持代码抽取，可以将相关代码抽取到一个单独的文件夹，方便用户拷贝集成到自己的开发环境。 请参见[C SDK编译说明](https://github.com/tencentyun/qcloud-iot-explorer-sdk-embedded-c/blob/master/docs/C-SDK_Build%E7%BC%96%E8%AF%91%E7%8E%AF%E5%A2%83%E5%8F%8A%E9%85%8D%E7%BD%AE%E9%80%89%E9%A1%B9%E8%AF%B4%E6%98%8E.md)。
### 编译报错 `error Only C99 allowed`
  * 如果使用`Makefile`编译，请在`Makefile`中添加`CFLAGS += -std=gnu99`编译参数。
  * 如果使用`CMake`编译，请在`CMakeLists.txt`中添加`set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gun99`语句。
### 编译后不能在目标设备上运行
  * 请检查交叉编译工具链是否配置正确。
  
## 设备接入问题
###  使用`MQTT.fx`连接物联网开发平台失败？
  请核对接入信息是否填写正确。参考[MQTT.fx快速接入指引](https://cloud.tencent.com/document/product/1081/46507)，该文档内提供了`User Name`和`Password`的生成小工具。
### 设备连接物联网开发平台失败？
  * 当错误码是`-111` ，表示连接超时，可适当增加连接超时时间：`#define QCLOUD_IOT_MQTT_COMMAND_TIMEOUT (5 * 1000)`。
  * 当错误码是`0x0052`时，请检查设备网络是否正常，设备上的 DNS 域名解析是否正常。
  * 当错误信息是`getaddrinfo xxx error`时，请检查设备网络是否正常，设备上的 DNS 域名解析是否正常。
  * 请检查[设备身份信息](https://cloud.tencent.com/document/product/1081/48376#.E8.AE.BE.E5.A4.87.E8.BA.AB.E4.BB.BD.E4.BF.A1.E6.81.AF)是否正确，建议直接点击物联网开发平台的复制按钮粘贴设备三元组。
  * 若未使用官方`C SDK`, 需要注意`MQTT`的`Password`是由设备密钥经过一系列转换得到的，具体实现请参考`C SDK`源码。
  * 当前平台限制是**5秒响应一次连接请求**。请注意重试时间间隔
### 动态注册失败？
  * 动态注册签名验证失败时，请检查产品密钥是否填写正确。**注意不要填写到设备密钥中**。
  * 动态注册签名验证失败时，请检查时间戳是否是`UTC`时间或者`0`。
  * 使用同一设备名重复动态注册失败，请在控制台打开`自动创建设备`选项。
  * 动态注册成功后需要设备端存储物联网开发平台返回的设备密钥。详细指引请参见[动态注册使用说明](https://github.com/tencentyun/qcloud-iot-explorer-sdk-embedded-c/blob/master/docs/%E5%8A%A8%E6%80%81%E6%B3%A8%E5%86%8C%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E.md)。
### `Topic`订阅失败是什么原因？
  * 请检查设备订阅`Topic`数量是否超出限制：`#define MAX_MESSAGE_HANDLERS (10)`。
  * `Topic`订阅没有权限。请在控制台查看设备是否有订阅权限：`产品开发->设备名->设备开发->Topic列表`。
### 设备日志中的错误码都表示什么含义？
  * `qcloud_iot_exporet_error.h`中定义了设备端错误码，相应错误码有注释说明。
  * `TLS`相关错误码请查看文件`external_libs/mbedtls/include/mbedtls/ssl.h`。
### `C SDK`是否支持自定义透传数据？
  * 创建设备时可以选择通信协议为`数据模版`或`自定义透传`。请参考[自定义透传设备上云](https://cloud.tencent.com/document/product/1081/52964#.E6.9F.A5.E7.9C.8B.E8.AE.BE.E5.A4.87.E4.B8.8A.E6.8A.A5.E6.95.B0.E6.8D.AE)。
  * 自定义透传数据解析请参考[LoRaWAN 设备数据解析](https://cloud.tencent.com/document/product/1081/41190)。
### 如何在控制台查看设备上报信息？
  * `设备调试->在线调试->通信日志`中可以看到设备和云端的交互信息，勾选`打开响应报文`可以看到设备的回复信息。
  *  在`设备调试->设备云端日志`和`设备调试->设备本地日志`可以看到设备的相关日志，`设备本地日志`需要设备端使能日志上报功能。
### 数据模版解析失败？
  * 请确认设备端数据模版和控制台数据模版内容一致。
  * 请检查数据模版使用和数据模版定义类型相同，数据没有超出范围。
### 控制台如何判断设备在线？
  * 设备端定期会发送`PING`包到物联网开发平台，默认周期为`240`秒 : `#define QCLOUD_IOT_MQTT_KEEP_ALIVE_INTERNAL (240 * 1000)`。
  * 平台会在`1.5`倍`KEEP_ALIVE`时间内没有收到设备报文时标记设备为离线状态。
### 如何修改设备端接入域名？
  * `C SDK`域名相关定义在`sdk_src/internal_inc/qcloud_iot_common.h` 文件中。
  * 各地域服务器地址请参见[设备接入地域说明](https://cloud.tencent.com/document/product/634/61228)。
### 如何从物联网开发平台获取`UTC`时间？
  * 请参见[NTP服务](https://cloud.tencent.com/document/product/634/56139)。
### 如何实现设备断开连接后的持续重连?
  * 示例代码如下：
  ```c
	while(1)
	{
		IOT_MQTT_Construct();
		while (1) {
			int ret = IOT_MQTT_Yield();
			if (rc != QCLOUD_RET_SUCCESS &amp;&amp; rc != QCLOUD_RET_MQTT_RECONNECTED) {
				Log_e("exit with error: %d", rc);
				break;
			}
		}
		IOT_MQTT_Destroy();
		HAL_SleepMs(10000);
	}
  ```
## 配网问题
### 设备端支持哪些配网方式？
  * 设备端支持[softAP](https://cloud.tencent.com/document/product/1081/48404)、[SmartConfig](https://cloud.tencent.com/document/product/1081/48405)、[AirKiss](https://cloud.tencent.com/document/product/1081/48406)、[simpleConfig](https://cloud.tencent.com/document/product/1081/48407)、[LLSync蓝牙配网](https://cloud.tencent.com/document/product/1081/63493)、[blufi蓝牙配网](https://cloud.tencent.com/document/product/1081/48408)等多种配网方式。
### 蓝牙配网时,设备已经联网成功,但是小程序显示配网失败？
  * 通常是设备端最后一个蓝牙包没有发送结束设备就关闭了蓝牙，请等待最后一个蓝牙包发送成功后再关闭蓝牙。
### Smartconfig配网时,一直提示失败？
  * 请分析设备端日志，检查是否收到完整的目标路由器的`SSID`和`Password`。`Smartconfig`、`Airkiss`等配网方式的基本原理都是路由器持续转发手机的`UDP`包，如果当前WiFi网络环境复杂，很有可能会收不完整的`SSID`和`Password`，可以尝试更换位置或路由器后重试。
### Soft AP配网时,手机无法连接设备创建热点？
  * 为了防止腾讯连连小程序连接到其他设备开启的热点导致配网失败。腾讯连连小程序和控制台对设备热点名称做了一些规则限制，设备端必须按照这些规则创建热点，腾讯连连小程序才可以正常连接。请在`交互开发->配网引导->首选配网方式->设备热点连接引导页`进行设置。
### 向后台发送token的时候,收到的`bind_device`消息作用是什么？
![bind_device](https://qcloudimg.tencent-cloud.cn/raw/0cd5063145e13ef1c6910aaaf801d607.png)
  * 腾讯连连小程序和设备绑定成功后，物联网开发平台会向设备发送`app_bind_token_reply`消息告知用户设备绑定成功。
  * 为兼容旧版本，物联网开发平台会追加发送一条`bind device`消息，该消息现在可以忽略。
### 设备端配网示例程序
  * [ESP32](https://github.com/espressif/esp-qcloud/tree/master/src/provisioning)和[ESP8266](https://github.com/tencentyun/qcloud-iot-esp-wifi/tree/master/qcloud-iot-esp8266-demo)上均有实现配网功能。
 
## AT模组问题
### 使用`AT`模组,设备无法连接物联网开发平台？
  * 检查设备信息是否设置正确，请参考[腾讯云AT指令集](https://docs.espressif.com/projects/esp-at/zh_CN/latest/Customized_AT_Commands_and_Firmware/Tencent_Cloud_IoT_AT/Tencent_Cloud_IoT_AT_Command_Set.html)使用`AT`指令。
### 如何升级`AT`模组？
  * 物联网开发平台支持`MCU`和`AT`模组升级，可以通过平台下发消息中的`fw_type`字段进行区分。`mcu`表示`MCU`固件升级，`moudule`表示模组固件升级。
### AT模组配网时,如何使用动态注册功能？
* 使用AT指令配网时，也可以使用动态注册功能。使用流程是先通过`AT+TCPRDINFOSET=<tls_mode>,<product_ID>,<product_secret>,<devic<br>e_name>`,设置产品ID、产品密钥和设备名称，然后开启配网指令。在ESP8266上的使用示例如下：
![](https://qcloudimg.tencent-cloud.cn/raw/9485fea2b68ebc0fcf4d52fc53e71282.jpg)
## 其他问题
###  使用ESP8266 + C SDK开发时,会出现挂死现象?
`ESP8266`中`bool`类型占用一个字节，但是`true`和`false`占用4个字节，导致踩内存错误。请将`qcloud_iot_export_data_template.h`中的`bool`类型定义为`int`。
```c
typedef int TYPE_DEF_TEMPLATE_BOOL;
```
