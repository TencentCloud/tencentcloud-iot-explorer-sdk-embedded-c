
# UTILS
file(GLOB src_utils	${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c)
set(src_sdk  ${src_utils})

# data_template
file(GLOB src_mqtt ${CMAKE_CURRENT_SOURCE_DIR}/protocol/mqtt/*.c)
list(APPEND src_sdk ${src_mqtt})

file(GLOB src_data_template ${CMAKE_CURRENT_SOURCE_DIR}/services/data_template/*.c)
list(APPEND src_sdk ${src_data_template})

# service mqtt
file(GLOB src_service_mqtt ${CMAKE_CURRENT_SOURCE_DIR}/services/service_com/*.c)
list(APPEND src_sdk ${src_service_mqtt})

# gateway
if(${FEATURE_GATEWAY_ENABLED} STREQUAL "ON")
	file(GLOB src_gateway ${CMAKE_CURRENT_SOURCE_DIR}/services/gateway/*.c)
	list(APPEND src_sdk ${src_gateway})
endif()

# ota mqtt
if(${FEATURE_OTA_COMM_ENABLED} STREQUAL "ON")
	file(GLOB src_mqtt_ota ${CMAKE_CURRENT_SOURCE_DIR}/services/ota/*.c)
	list(APPEND src_sdk ${src_mqtt_ota})
endif()

# resource
if(${FEATURE_RESOURCE_UPDATE_ENABLED} STREQUAL "ON")
	file(GLOB src_mqtt_resource ${CMAKE_CURRENT_SOURCE_DIR}/services/resource/*.c)
	list(APPEND src_sdk ${src_mqtt_resource})
endif()

# file manage 
if(${FEATURE_FILE_MANAGE_ENABLE} STREQUAL "ON")
	file(GLOB src_mqtt_file_manage ${CMAKE_CURRENT_SOURCE_DIR}/services/file_manage/*.c)
	list(APPEND src_sdk ${src_mqtt_file_manage})
endif()

# ASR
if(${FEATURE_ASR_ENABLED} STREQUAL "ON")
	file(GLOB src_asr ${CMAKE_CURRENT_SOURCE_DIR}/services/asr/*.c)
	list(APPEND src_sdk ${src_asr})
endif()

# HTTP 
if(${FEATURE_OTA_COMM_ENABLED} STREQUAL "ON" OR ${FEATURE_DEV_DYN_REG_ENABLED} STREQUAL "ON" OR ${FEATURE_LOG_UPLOAD_ENABLED} STREQUAL "ON")
	# http
	file(GLOB src_http 	${CMAKE_CURRENT_SOURCE_DIR}/protocol/http/*.c)
	list(APPEND src_sdk ${src_http})
endif()

# DYN REG 	
if(${FEATURE_DEV_DYN_REG_ENABLED} STREQUAL "ON")
	file(GLOB src_dyn_reg ${CMAKE_CURRENT_SOURCE_DIR}/services/dynreg/*.c)
	list(APPEND src_sdk ${src_dyn_reg})
endif()

# LOG
if(${FEATURE_LOG_UPLOAD_ENABLED} STREQUAL "ON")
	file(GLOB src_log_upload ${CMAKE_CURRENT_SOURCE_DIR}/services/log/*.c)
	list(APPEND src_sdk ${src_log_upload})
endif()

#SYSTEM RESOURCE
if(${FEATURE_SYSTEM_COMM_ENABLED} STREQUAL "ON")
	file(GLOB src_system ${CMAKE_CURRENT_SOURCE_DIR}/services/system/*.c)
	list(APPEND src_sdk ${src_system})
endif()

# AT OR TCP
if(${FEATURE_AT_TCP_ENABLED} STREQUAL "ON")
	# at
	file(GLOB src_at_socket ${CMAKE_CURRENT_SOURCE_DIR}/network/at_socket/*.c)
	list(APPEND src_sdk ${src_at_socket})
endif()

# TLS
if(${FEATURE_AUTH_WITH_NOTLS} STREQUAL "OFF")
	file(GLOB src_tls ${CMAKE_CURRENT_SOURCE_DIR}/network/tls/*.c)
	list(APPEND src_sdk ${src_tls})
endif()

# NETWORK
# tcp
file(GLOB src_socket ${CMAKE_CURRENT_SOURCE_DIR}/network/socket/*.c)
list(APPEND src_sdk ${src_socket})
# interface
file(GLOB src_network ${CMAKE_CURRENT_SOURCE_DIR}/network/*.c)
list(APPEND src_sdk ${src_network})

# WIFI Config
if(FEATURE_WIFI_CONFIG_ENABLED STREQUAL "ON")
file(GLOB src_wifi_config ${CMAKE_CURRENT_SOURCE_DIR}/wifi_config/*.c)
list(APPEND src_sdk ${src_wifi_config})
endif()

# code output
if(${EXTRACT_SRC} STREQUAL  "ON")
	file(COPY ${src_sdk} DESTINATION ${PROJECT_SOURCE_DIR}/output/qcloud_iot_c_sdk/sdk_src/)
endif()
set(service_target "iot_sdk")
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output/${BUILD_TYPE}/lib/)  
add_library(${service_target} STATIC ${src_sdk})
