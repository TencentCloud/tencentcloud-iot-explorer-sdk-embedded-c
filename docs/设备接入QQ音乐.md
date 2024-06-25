# 设备接入QQ音乐

## 概述

设备在接入腾讯云物联网开发平台后，可以选择开通增值服务-**QQ音乐**。当设备在连连小程序完成绑定授权后，就可以使用QQ音乐海量音乐资源。

## 交互流程

设备端和小程序以及物联网开发平台的交互流程如下：

![image](https://qcloudimg.tencent-cloud.cn/raw/1c4b8fc27c895cfdbe60aaf760c97e28.png)


## 交互协议

### 播控协议

通过腾讯云物联网开发平台的物模型协议来交互，具体的协议实现请参考[物模型协议](https://cloud.tencent.com/document/product/1081/34916)，当在控制台选择开通QQ音乐增值服务后，平台会默认给每个设备赋予QQ音乐的控制协议，客户无需关心。
![image.png](https://qcloudimg.tencent-cloud.cn/raw/cf0be950ef3cb8de13cd84729ffab41e.png)
 小程序可以通过物模型协议来控制设备的当前播放状态以及播放模式。设备端在收到control/action消息并执行后，会report最新的属性值给平台。
 

#### _sys_play_mode

用来控制设备的播放模式，设备根据此模式来决定歌单列表的播放顺序。

#### _sys_play_position

1. 设备用来上报当前歌曲播放进度
2. 小程序可以改变该属性值来实现快进/快退操作

#### _sys_recommend_quality

推荐播放质量，设备收到该值后会优先获取该质量下的歌曲资源。

#### _sys_volume

1. 小程序会下发改变设备音量
2. 设备会上报当前播放音量

#### _sys_pause_play

小程序控制当前播放/暂停。

#### _sys_song_id

1. 小程序切歌时会下发歌曲ID给设备，完成切歌。
2. 设备会更新该值来上报当前正在播放的歌曲ID

#### _sys_song_list

设备会维护该列表，当歌单发生变化时，会更新该列表给小程序，用来指示设备当前播放歌单。

#### _sys_pre_next

小程序用来播放上一曲/下一曲/从头播放当前歌曲，切歌完成后会收到设备的切歌结果
 

### 更新歌单


#### MQTT Topic

通过订阅和发布此topic，用于更新设备播放歌单。

```shell
$thing/up/service/{product_id}/{device_name}   // 发布

$thing/down/service/{product_id}/{device_name} // 订阅
```


#### 协议

#### 下发歌曲列表

小程序需要播放某个歌单时，会主动下发歌单到设备。

| 字段         | 类型              | 说明                                                                        |
|------------|-----------------|---------------------------------------------------------------------------|
| type       | int             | 歌曲列表下发类型，与qq音乐开发者平台文档索引相同  5:歌单 6:歌曲 7:电台 8:榜单 9:专辑 10:歌手 12:推荐  14:免登录专区 |
| id         | uint64_t             | 歌曲列表Id                                                                    |
| quality    | int             | 歌曲音质 1：流畅 2.标准 4. 高品质 8. 超品质 16. Hi-Res 32.杜比 64.母带 128.臻品全景声             |
| name       | string          | 歌曲列表名称                                                                    |
| songList   | array of object | 歌曲列表                                                                      |
| total      | int             | 歌曲列表中包含的歌曲总数                                                              |
| page       | int             | 歌曲列表页码数                                                                   |
| pageSize   | int             | 歌曲列表页码大小                                                                  |
| playSongId | int             | 需要播放的歌曲ID                                                                 |

| 字段           | 类型     | 说明                                |
|--------------|--------|-----------------------------------|
| userOwnRule  | int    | 表示用户拥有接口的权限。0：只浏览；1：可播放           |
| songId       | uint64_t    | 歌曲id                              |
| songPlayTime | int    | 播放总时间                             |
| songURL      | string | 流畅品质流媒体url。若user_own_rule=0，则取值为空 |
| token        | string | 免登录专区的歌曲，会同时下发该Token              |
| songName     | string | 歌曲名称                              |
| singerName   | string | 歌手名称                              |

```json
{
  "method": "qqmusic_sync_song_list",
  "clientToken": "clientToken-0YA7Qu#0_",
  "timestamp": 1704941322,
  "params": {
    "type": 5,
    "id": "song_list_id",
    "quality": 1,
    "name": "歌单1",
    "total": 10,
    "page": 1,
    "pageSize": 10,
    "playSongId": 1002,
    "songList": [
      {
        "userOwnRule": 1,
        "songId": 1002,
        "songPlayTime": 300,
        "songURL": "https://xx.abc?q=",
        "songName": "免费歌曲1",
        "singerName": "歌手1"
      },
      {
        "userOwnRule": 0,
        "songId": 1003,
        "songPlayTime": 210,
        "songURL": "",
        "songName": "付费歌曲2",
        "singerName": "歌手2"
      }
    ]
  }
}
```

##### 上行
设备成功解析歌单后，返回解析结果给平台。
```json
{
  "method": "qqmusic_sync_song_list_reply",
  "clientToken": "clientToken-0YA7Qu#0_",
  "timestamp": 1704941322,
  "params": {
    "code": 0,
    "msg": "ok"
  }
}
```

#### 获取歌曲列表
如果歌单中的歌曲太多，则分页下发。由设备根据total和pageSize字段来决定是否请求歌单中的剩余歌曲。
##### 上行

```json
{
  "method": "qqmusic_query_song_list",
  "clientToken": "clientToken-0YA7Qu#0_",
  "timestamp": 1704941322,
  "params": {
    "type": 5,
    "quality": 1,
    "id": "song_list_id",
    "page": 2,
    "pageSize": 10
  }
}
```

##### 下行
平台返回歌单中的剩余歌曲，由设备决定是否继续请求全部歌单歌曲。
```json
{
  "method": "qqmusic_query_song_list_reply",
  "clientToken": "clientToken-0YA7Qu#0_",
  "timestamp": 1704941322,
  "params": {
    "code": 0,
    "msg": "ok",
    "type": 5,
    "id": "song_list_id",
    "quality": 1,
    "name": "歌单1",
    "total": 10,
    "page": 2,
    "pageSize": 10,
    "songList": [
      {
        "userOwnRule": 1,
        "songId": 1002,
        "songPlayTime": 300,
        "songUrl": "https://xx.abc?q=",
        "songName": "免费歌曲1",
        "singerName": "歌手1"
      },
      {
        "userOwnRule": 0,
        "songId": 1003,
        "songPlayTime": 210,
        "songURL": "",
        "songName": "付费歌曲2",
        "singerName": "歌手2"
      }
    ]
  }
}
```

#### 获取歌曲信息

> 获取音乐信息，当云端同步给设备端侧的歌单中url为空时，需要设备侧主动查询音乐信息，免登录歌曲需要同时传递token来获取歌曲信息

##### 上行

```json
{
  "method": "qqmusic_query_song",
  "clientToken": "clientToken-0YA7Qu#0_",
  "timestamp": 1704941322,
  "params": {
    "songId": 1002,
    "token": "xxx"
    // 免登录歌曲需要同时传递token来获取歌曲信息
  }
}
```

##### 下行

```json
{
  "method": "qqmusic_query_song_reply",
  "clientToken": "clientToken-0YA7Qu#0_",
  "timestamp": 1704941322,
  "params": {
    "code": 0,
    "msg": "ok",
    "userOwnRule": 1,
    "songId": 1002,
    "songPlayTime": 300,
    "songURL": "https://xx.abc?q=",
    "songName": "免费歌曲1",
    "songTitle": "歌曲标题",
    "singerName": "歌手1",
    "singerPic": "歌手图片",
    "albumName": "专辑名称",
    "albumPic": "专辑图片"
  }
}
```

## sample使用说明

> 注意： 只提供Linux平台sample
### 设备端操作
1. 拿到SDK后，需确认根目录下的CMakeLists.txt里的如下选项存在（以密钥认证设备为例）
```shell
set(BUILD_TYPE                   "release")
set(COMPILE_TOOLS                "gcc") 
set(PLATFORM 	                 "linux")
set(FEATURE_MQTT_COMM_ENABLED ON)
set(FEATURE_AUTH_MODE "KEY")
set(FEATURE_AUTH_WITH_NOTLS OFF)
set(FEATURE_DEBUG_DEV_INFO_USED  OFF)
set(FEATURE_DEBUG_DEV_INFO_USED  OFF)
set(FEATURE_QQMUSIC_ENABLED  ON)
```

2. 执行编译脚本`$ ./cmake_build.sh`，输出sample位于`./output/release/bin`文件夹中。
3. 填写设备信息 将上面在腾讯云物联网开发平台 IoT Explorer 创建的设备的设备信息(以密钥认证设备为例)填写到 device_info.json 中 `c { "auth_mode":"KEY", "productId":"XXX", "deviceName":"xxx", "key_deviceinfo":{     "deviceSecret":"xxxxxxxx" } } `
4. ubuntu linux 环境安装 mplayer `sudo apt install mplayer`，保证`mplaery music_url`能正常播放音频
5. 执行`./output/release/bin/qqmusic_data_template_sample `来运行示例程序。

### 小程序操作
小程序端拿到QQMusic插件并完成开发后，设备上线后，需跳转到QQ音乐授权登录页面完成授权绑定，绑定完成后，就可以同步QQ音乐资源，直接点击歌单即可完成歌单资源的下发，播放等。

### sample运行日志分析
如下是设备端收到的歌单更新日志：
```json
{
    "method": "qqmusic_sync_song_list",
    "clientToken": "abf7qGHuq",
    "timestamp": 1707035495,
    "params": {
        "id": 4167081898,
        "name": "我喜欢",
        "page": 0,
        "pageSize": 10,
        "playSongId": 247347346,
        "quality": 4,
        "songList": [
            {
                "userOwnRule": 1,
                "songId": 451468714,
                "songPlayTime": 166,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C600002hjf5d37WuNj.m4a",
                "songName": "我期待的不是雪 (而是有你的冬天)",
                "singerName": "张妙格"
            },
            {
                "userOwnRule": 1,
                "songId": 247347346,
                "songPlayTime": 235,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C600001QJyJ32zybEe.m4a",
                "songName": "句号",
                "singerName": "G.E.M. 邓紫棋"
            },
            {
                "userOwnRule": 1,
                "songId": 422061155,
                "songPlayTime": 284,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C6000006oypR46bcLX.m4a",
                "songName": "瞬",
                "singerName": "郑润泽"
            },
            {
                "userOwnRule": 1,
                "songId": 447260,
                "songPlayTime": 247,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C600002XfMqZ1A2VNQ.m4a",
                "songName": "我还想她",
                "singerName": "林俊杰"
            },
            {
                "userOwnRule": 1,
                "songId": 105519196,
                "songPlayTime": 292,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C600002vmR7d2ldNWA.m4a",
                "songName": "疑心病",
                "singerName": "任然"
            },
            {
                "userOwnRule": 1,
                "songId": 464314235,
                "songPlayTime": 192,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C600002PpXDy2oCtx8.m4a",
                "songName": "栀子花与雪花",
                "singerName": "白允y"
            },
            {
                "userOwnRule": 1,
                "songId": 224827783,
                "songPlayTime": 285,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C600000NHikR1jV7Zz.m4a",
                "songName": "像鱼",
                "singerName": "王贰浪"
            },
            {
                "userOwnRule": 1,
                "songId": 1424979,
                "songPlayTime": 266,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C6000013BRSP1eUbya.m4a",
                "songName": "Family Affair",
                "singerName": "Mary J. Blige"
            },
            {
                "userOwnRule": 1,
                "songId": 232276537,
                "songPlayTime": 261,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C6000030o6bl4arAjH.m4a",
                "songName": "Halo",
                "singerName": "Beyoncé"
            },
            {
                "userOwnRule": 1,
                "songId": 234341116,
                "songPlayTime": 181,
                "songURL": "http://isure6.stream.qqmusic.qq.com/C600004ZSc7B3RlVg4.m4a",
                "songName": "Senorita",
                "singerName": "Adam Christopher"
            }
        ],
        "total": 18,
        "type": 5
    }
}
```
设备端收到该消息后，会做如下操作：
1. 播放歌曲`247347346`
2. 计算还需要拉取的歌曲数量，发送请求剩余歌曲

```shell
DBG|2024-02-04 00:31:35|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/service/GGBE5NCDZ1/music1|payload={"method":"qqmusic_query_song_list","clientToken":"qqmusic_query_song_list-1933479329","params":{"type":5,"quality":4,"id":4167081898,"page":1,"pageSize":8}}

```
3. 更新属性值_sys_song_list,并report

```shell
DBG|2024-02-04 00:31:36|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/GGBE5NCDZ1/music1|payload={"method":"report", "clientToken":"GGBE5NCDZ1-2", "params":{"_sys_song_list":"{\"songListId\":4167081898,\"songListType\":5,\"songList\":[451468714,247347346,422061155,447260,105519196,464314235,224827783,1424979,232276537,234341116]}"}}
```
4. 收到剩余歌曲的回复

> 说明：如果这里计算歌单中还有剩余歌曲，则继续发`qqmusic_query_song_list`来请求剩余歌曲。


```shell
{"method":"qqmusic_query_song_list_reply","clientToken":"qqmusic_query_song_list-1933479329","code":0,"status":"","params":{// 歌单中的剩余歌曲}}
```

设备收到切歌指令：
```
DBG|2024-02-04 00:39:40|data_template_action.c|_on_action_handle_callback(160): recv:{"method":"action","clientToken":"v2179940304OBlyZ::VTYM9aR-6x","actionId":"_sys_pre_next","timestamp":1707035980,"params":{"pre_or_next":2}}

DBG|2024-02-04 00:39:40|qqmusic_client.c|IOT_QQMusic_GetPlaySongInfo(450): song_id : 0 type : 2
DBG|2024-02-04 00:39:40|qqmusic_client.c|_qqmusic_get_play_song_info_from_type(435): music->song_list.playIndex : 3
DBG|2024-02-04 00:39:40|qqmusic_data_template.c|OnActionCallback(52): song name : 我还想她
DBG|2024-02-04 00:39:40|qqmusic_data_template.c|OnActionCallback(53): signer name : 林俊杰
DBG|2024-02-04 00:39:40|qqmusic_data_template.c|OnActionCallback(54): song id : 447260
DBG|2024-02-04 00:39:40|HAL_Music_Linux.c|_player_play(201): play song http://isure6.stream.qqmusic.qq.com/C600002XfMqZ1A2VNQ.m4a-247
mkdir: cannot create directory ‘/tmp/qqmusic/’: File exists
DBG|2024-02-04 00:39:40|HAL_Music_Linux.c|_player_play(210): mkfifo ret: 0
do_connect: could not connect to socket
connect: No such file or directory
Failed to open LIRC support. You will not be able to use your remote control.

Couldn't resolve name for AF_INET6: isure6.stream.qqmusic.qq.com
XDG_RUNTIME_DIR (/run/user/1000) is not owned by us (uid 0), but by uid 1000! (This could e.g. happen if you try to connect to a non-root PulseAudio as a root user, over the native protocol. Don't do that.)
AO: [pulse] Init failed: Connection refused
Failed to initialize audio driver 'pulse'
DBG|2024-02-04 00:39:42|mqtt_client_publish.c|qcloud_iot_mqtt_publish(339): publish topic seq=30732|topicName=$thing/up/action/GGBE5NCDZ1/music1|payload={"method":"action_reply", "clientToken":"v2179940304OBlyZ::VTYM9aR-6x", "code":0, "status":"action execute success!", "response":{"code":0}}

```

设备上报当前歌曲播放进度：
```
DBG|2024-02-04 00:38:42|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/GGBE5NCDZ1/music1|payload={"method":"report", "clientToken":"GGBE5NCDZ1-45", "params":{"_sys_play_position":181,"_sys_song_id":422061155}}
INF|2024-02-04 00:38:42|qqmusic_data_template.c|main(565): data template reporte success
DBG|2024-02-04 00:38:42|data_template_client_manager.c|_on_template_downstream_topic_handler(505): recv:{"method":"report_reply","clientToken":"GGBE5NCDZ1-45","code":0,"status":"success"}
INF|2024-02-04 00:38:42|qqmusic_data_template.c|OnReportReplyCallback(265): recv report reply response, reply ack: 0
DBG|2024-02-04 00:38:52|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/GGBE5NCDZ1/music1|payload={"method":"report", "clientToken":"GGBE5NCDZ1-46", "params":{"_sys_play_position":191,"_sys_song_id":422061155}}
INF|2024-02-04 00:38:52|qqmusic_data_template.c|main(565): data template reporte success
DBG|2024-02-04 00:38:52|data_template_client_manager.c|_on_template_downstream_topic_handler(505): recv:{"method":"report_reply","clientToken":"GGBE5NCDZ1-46","code":0,"status":"success"}
INF|2024-02-04 00:38:52|qqmusic_data_template.c|OnReportReplyCallback(265): recv report reply response, reply ack: 0
DBG|2024-02-04 00:39:02|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/GGBE5NCDZ1/music1|payload={"method":"report", "clientToken":"GGBE5NCDZ1-47", "params":{"_sys_play_position":201,"_sys_song_id":422061155}}
INF|2024-02-04 00:39:02|qqmusic_data_template.c|main(565): data template reporte success
DBG|2024-02-04 00:39:02|data_template_client_manager.c|_on_template_downstream_topic_handler(505): recv:{"method":"report_reply","clientToken":"GGBE5NCDZ1-47","code":0,"status":"success"}
INF|2024-02-04 00:39:02|qqmusic_data_template.c|OnReportReplyCallback(265): recv report reply response, reply ack: 0
DBG|2024-02-04 00:39:12|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/GGBE5NCDZ1/music1|payload={"method":"report", "clientToken":"GGBE5NCDZ1-48", "params":{"_sys_play_position":211,"_sys_song_id":422061155}}
INF|2024-02-04 00:39:12|qqmusic_data_template.c|main(565): data template reporte success
DBG|2024-02-04 00:39:12|data_template_client_manager.c|_on_template_downstream_topic_handler(505): recv:{"method":"report_reply","clientToken":"GGBE5NCDZ1-48","code":0,"status":"success"}
INF|2024-02-04 00:39:12|qqmusic_data_template.c|OnReportReplyCallback(265): recv report reply response, reply ack: 0
DBG|2024-02-04 00:39:17|mqtt_client_yield.c|_mqtt_keep_alive(200): PING request 1 has been sent...
DBG|2024-02-04 00:39:22|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/GGBE5NCDZ1/music1|payload={"method":"report", "clientToken":"GGBE5NCDZ1-49", "params":{"_sys_play_position":221,"_sys_song_id":422061155}}
INF|2024-02-04 00:39:22|qqmusic_data_template.c|main(565): data template reporte success
DBG|2024-02-04 00:39:22|data_template_client_manager.c|_on_template_downstream_topic_handler(505): recv:{"method":"report_reply","clientToken":"GGBE5NCDZ1-49","code":0,"status":"success"}
INF|2024-02-04 00:39:22|qqmusic_data_template.c|OnReportReplyCallback(265): recv report reply response, reply ack: 0
DBG|2024-02-04 00:39:32|mqtt_client_publish.c|qcloud_iot_mqtt_publish(346): publish packetID=0|topicName=$thing/up/property/GGBE5NCDZ1/music1|payload={"method":"report", "clientToken":"GGBE5NCDZ1-50", "params":{"_sys_play_position":231,"_sys_song_id":422061155}}
INF|2024-02-04 00:39:32|qqmusic_data_template.c|main(565): data template reporte success
DBG|2024-02-04 00:39:32|data_template_client_manager.c|_on_template_downstream_topic_handler(505): recv:{"method":"report_reply","clientToken":"GGBE5NCDZ1-50","code":0,"status":"success"}

```

## 适配说明

提供一个`HAL_Music_Linux.c`的适配文件供参考。
需要适配如下函数：
```c
/**
 * @brief 初始化音乐播放器并播放音乐
 *
 * @param[out] player 音乐播放器句柄
 * @param url 待播放音乐的HTTP URL
 * @param duration 音频时长
 * @return 0成功 其他失败
 */
int HAL_Music_Play(void **player, const char *url, int duration);

/**
 * @brief 停止音乐播放
 *
 * @param player
 * @return 0 when success
 */
int HAL_Music_Stop(void **player);

/**
 * @brief 控制播放or暂停
 *
 * @param player music player
 * @param playPause 0:pause, 1:play
 * @return true for play
 * @return false for pause
 */
bool HAL_Music_PlayPause(void *player, uint8_t playPause);

/**
 * @brief 设置音量
 *
 * @param player music player
 * @param volume
 * @return 0 when success
 */
int HAL_Music_SetVolume(void *player, int volume);

/**
 * @brief 获取音量
 *
 * @param player music player
 * @return current volume
 */
int HAL_Music_GetVolume(void *player);

/**
 * @brief 检测是否播完
 *
 * @param player music player
 * @return true for play end
 * @return false for playing
 */
bool HAL_Music_PlayEndCheck(void *player);

/**
 * @brief 获取播放进度
 *
 * @param player music player
 * @return current play position
 */
int HAL_Music_GetPlayPosition(void *player);

/**
 * @brief 设置播放进度
 *
 * @param player music player
 * @param position play position
 * @return 0 when success
 */
int HAL_Music_SetPlayPosition(void *player, int position);
```


























