
set(INC_DIR  ${PROJECT_SOURCE_DIR}/output/${BUILD_TYPE}/include)
set(LINK_DIR ${PROJECT_SOURCE_DIR}/output/${BUILD_TYPE}/lib/)
include_directories(${INC_DIR})
link_directories(${LINK_DIR})

if(FEATURE_AUTH_WITH_NOTLS)
set(link_lib
	iot_sdk
	iot_platform
)
else()
set(link_lib
	iot_sdk
	iot_platform
	mbedtls
	mbedx509
	mbedcrypto
)
endif()

if(CMAKE_COMPILER_IS_GNUCC)
	set(lib -Wl,--start-group ${link_lib} -Wl,--end-group)
if(FEATURE_WIFI_CONFIG_ENABLED STREQUAL "ON")
	set(lib -Wl,--start-group ${link_lib} -lcjson -Wl,--end-group)
endif()
else()
	set(lib ${link_lib})
if(FEATURE_WIFI_CONFIG_ENABLED STREQUAL "ON")
	set(lib ${link_lib} cjson)
endif()
endif()

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/output/${BUILD_TYPE}/bin)


# data_template
file(GLOB src_data_template_sample 			${PROJECT_SOURCE_DIR}/samples/data_template/data_template_sample.c)
add_executable(data_template_sample 		${src_data_template_sample})
target_link_libraries(data_template_sample 	 	 ${lib})

file(GLOB src_light_data_template_sample	${PROJECT_SOURCE_DIR}/samples/scenarized/light_data_template_sample.c)
add_executable(light_data_template_sample 	${src_light_data_template_sample})
target_link_libraries(light_data_template_sample ${lib})
	
# MQTT
file(GLOB src_mqtt_sample 					${PROJECT_SOURCE_DIR}/samples/mqtt/mqtt_sample.c)
add_executable(mqtt_sample 					${src_mqtt_sample})
target_link_libraries(mqtt_sample 		    ${lib})

# Raw data
file(GLOB src_raw_data 							${PROJECT_SOURCE_DIR}/samples/raw_data/raw_data_sample.c)
add_executable(raw_data_mqtt_sample 			${src_raw_data})
target_link_libraries(raw_data_mqtt_sample 		${lib})

# GATEWAY
if(${FEATURE_GATEWAY_ENABLED} STREQUAL "ON")
	file(GLOB src_gateway_sample  				${PROJECT_SOURCE_DIR}/samples/gateway/*.c)
	add_executable(gateway_sample 				${src_gateway_sample})
	target_link_libraries(gateway_sample 			 ${lib})
endif()

# OTA MQTT
if(${FEATURE_OTA_COMM_ENABLED} STREQUAL "ON")
	file(GLOB src_ota_mqtt_sample 				${PROJECT_SOURCE_DIR}/samples/ota/ota_mqtt_sample.c)
	add_executable(ota_mqtt_sample				${src_ota_mqtt_sample})
	target_link_libraries(ota_mqtt_sample			 ${lib})
endif()

# Resource MQTT
if(${FEATURE_RESOURCE_UPDATE_ENABLED} STREQUAL "ON")
	file(GLOB src_resource_mqtt_sample 				${PROJECT_SOURCE_DIR}/samples/resource/resource_mqtt_sample.c)
	add_executable(resource_mqtt_sample				${src_resource_mqtt_sample})
	target_link_libraries(resource_mqtt_sample			 ${lib})
endif()

# ASR
if(${FEATURE_ASR_ENABLED} STREQUAL "ON")
	file(GLOB src_asr_sample 				${PROJECT_SOURCE_DIR}/samples/asr/asr_data_template_sample.c)
	add_executable(asr_data_template_sample				${src_asr_sample})
	target_link_libraries(asr_data_template_sample			 ${lib})
endif()

# DYN_REG
if(${FEATURE_DEV_DYN_REG_ENABLED} STREQUAL "ON")
	file(GLOB src_dynreg_dev_sample		${PROJECT_SOURCE_DIR}/samples/dynreg_dev/dynreg_dev_sample.c)
	add_executable(dynreg_dev_sample 		${src_dynreg_dev_sample})
	target_link_libraries(dynreg_dev_sample 	 ${lib})
endif()

# WIFI_CONFIG
if(${FEATURE_WIFI_CONFIG_ENABLED} STREQUAL "ON")
	file(GLOB src_wifi_config_sample		${PROJECT_SOURCE_DIR}/samples/wifi_config/wifi_config_sample.c)
	add_executable(wifi_config_sample 		${src_wifi_config_sample})
	target_link_libraries(wifi_config_sample 	 ${lib})
endif()

