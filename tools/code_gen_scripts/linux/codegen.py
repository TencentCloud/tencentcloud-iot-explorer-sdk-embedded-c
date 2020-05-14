"""
Generate data_template_config.c and data_template_config.h base on json
"""
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright @ 2020 Tencent.com

import argparse
import json
import os

KEY_PROPERTIES = 'properties'
KEY_EVENTS = 'events'
KEY_ACTIONS = 'actions'
KEY_ACTION_INPUT = 'input'
KEY_ACTION_OUTPUT = 'output'

KEY_PARAMS = 'params'
KEY_ID = 'id'
KEY_DEFINE = 'define'
KEY_TYPE = 'type'
KEY_DEFAULT_VALUE = 'start'
KEY_MAX = 'max'

CODE_PROPERTY_TOTAL_COUNT = '#To Generate Property Count'
CODE_PROPERTY_ID = '#To Generate Properties ID'
CODE_PROPERTY_PARAMETER = '#To Generate Properties Parameters'
CODE_PROPERTY_INIT = '#To Generate Init Properties'
CODE_PROPERTY_GEN_PARAMETER = '#To generate Get Product Parameter'
#CODE_PROPERTY_FREE = '#To Generate Free Properties'

CODE_EVENT_TOTAL_COUNT = '#To Generate Event Count'
CODE_EVENT_PARAMETER_NUM = '#To Generate Events Parameter Num'
CODE_EVENT_ID = '#To Generate Events ID'
CODE_EVENT_PARAMETER = '#To Generate Events Parameters'
CODE_EVENT_NUM_SUM = '#To Generate Events Parameter Sum'
CODE_EVENT_INIT = '#To Generate Init Events'

CODE_ACTION_TOTAL_COUNT = '#To Generate Action Count'
CODE_ACTION_PARAMETER_NUM = '#To Generate Actions Parameter Num'
CODE_ACTION_ID = '#To Generate Actions ID'
CODE_ACTION_PARAMETER = '#To Generate Actions Parameters'
CODE_ACTION_NUM_SUM = '#To Generate Actions Parameter Sum'
CODE_ACTION_INIT = '#To Generate Init Actions'

PARAMETER_TYPE = {
    "int":'TYPE_DEF_TEMPLATE_INT',
    "enum":'TYPE_DEF_TEMPLATE_ENUM',
    "float":'TYPE_DEF_TEMPLATE_FLOAT',
    "bool":'TYPE_DEF_TEMPLATE_BOOL',
    "string":'TYPE_DEF_TEMPLATE_STRING',
    "timestamp":'TYPE_DEF_TEMPLATE_TIME'
}

DATA_PROPERTY_TYPE = {
    "int":'TYPE_TEMPLATE_INT',
    "enum":'TYPE_TEMPLATE_ENUM',
    "float":'TYPE_TEMPLATE_FLOAT',
    "bool":'TYPE_TEMPLATE_BOOL',
    "string":'TYPE_TEMPLATE_STRING',
    "timestamp":'TYPE_TEMPLATE_TIME'
}

class DataTemplate():
    """Parse DataTemplate with json file.
    Parse key and initial value in json file
    """

    def __init__(self, json_file_path):
        """Inits DataTemplate with json file."""
        with open(json_file_path, 'r', encoding='UTF-8') as data_template:
            self.data_template = json.loads(data_template.read())

    def __get_properties(self):
        """Get DataTemplate properties."""
        return self.data_template[KEY_PROPERTIES]

    def __get_events(self):
        """Get DataTemplate events."""
        return self.data_template[KEY_EVENTS]

    def __get_actions(self):
        """Get DataTemplate actions."""
        return self.data_template[KEY_ACTIONS]

    def generate_properties_code(self):
        """Generate property code."""
        properties = self.__get_properties()
        property_count = 0
        property_id = []
        property_defines = {}
        for items in properties:
            property_id.append(items[KEY_ID])
            property_defines[items[KEY_ID]] = items[KEY_DEFINE]
            property_count = property_count + 1

        property_code = {}

        # CODE_PROPERTY_TOTAL_COUNT = '#To Generate Property Count'
        property_count_code = '#define TOTAL_PROPERTY_COUNTS {count}'.format(count=property_count)
        property_code[CODE_PROPERTY_TOTAL_COUNT] = property_count_code

        # CODE_PROPERTY_ID = '#To Generate Properties ID'
        property_id_code = ''
        for key in property_id:
            property_id_code += 'ePROPERTY_{ID},\n'.format(ID=key.upper())
        property_code[CODE_PROPERTY_ID] = property_id_code[:-1].replace('\n', '\n    ')

        # CODE_PROPERTY_PARAMETER = '#To Generate Properties Parameter'
        property_parameter_code = ''
        for key in property_id:
            if property_defines[key][KEY_TYPE] == 'string':
                property_parameter_code += '{parameter_type} m_{name}[{max_len} + 1];\n'.format(
                    parameter_type=PARAMETER_TYPE[property_defines[key][KEY_TYPE]],
                    name=key,
                    max_len=property_defines[key][KEY_MAX]
                )
            else:
                property_parameter_code += '{parameter_type} m_{name};\n'.format(
                    parameter_type=PARAMETER_TYPE[property_defines[key][KEY_TYPE]],
                    name=key
                )
        property_code[CODE_PROPERTY_PARAMETER] = property_parameter_code[:-1].replace('\n', '\n    ')

        # CODE_PROPERTY_INIT = '#To Generate Init Properties'
        property_init_code = ''
        for key in property_id:
            if property_defines[key][KEY_TYPE] == 'string':
               
                property_init_code += \
                    'pData->m_{name}[0] = \'\\0\';\n' \
                    'ADD_PROPERTY_PARA_STR({name}, TYPE_TEMPLATE_STRING, &pDataTemplate->property[{property_id}].data_property, pData->m_{name}, '\
                    'sizeof(pData->m_{name})/sizeof(pData->m_{name}[0]));\n'.format(
                        name=key,
                        property_id='ePROPERTY_{ID}'.format(ID=key.upper())
                    )
                """old style
                property_init_code += \
                    'pData->m_{name}[0] = \'0\';\n' \
                    'pDataTemplate->property[{property_id}].data_property.key  = "{name}";\n' \
                    'pDataTemplate->property[{property_id}].data_property.data = pData->m_{name};\n' \
                    'pDataTemplate->property[{property_id}].data_property.data_buff_len = sizeof(pData->m_{name})/sizeof(pData->m_{name}[0]);\n' \
                    'pDataTemplate->property[{property_id}].data_property.type = TYPE_TEMPLATE_STRING;\n\n'.format(
                        name=key,
                        property_id='ePROPERTY_{ID}'.format(ID=key.upper())
                    )
                """
            elif property_defines[key][KEY_TYPE] == 'int' or \
                 property_defines[key][KEY_TYPE] == 'float':
                property_init_code += \
                    'pData->m_{name} = {default_value};\n' \
                    'ADD_PROPERTY_PARA({name}, {parameter_type}, &pDataTemplate->property[{property_id}].data_property, &pData->m_{name});\n'.format(
                        name=key,
                        default_value=property_defines[key][KEY_DEFAULT_VALUE],
                        property_id='ePROPERTY_{ID}'.format(ID=key.upper()),
                        parameter_type=DATA_PROPERTY_TYPE[property_defines[key][KEY_TYPE]]
                    )
                """old style
                property_init_code += \
                    'pData->m_{name} = {default_value};\n' \
                    'pDataTemplate->property[{property_id}].data_property.key  = "{name}";\n' \
                    'pDataTemplate->property[{property_id}].data_property.data = &pData->m_{name};\n' \
                    'pDataTemplate->property[{property_id}].data_property.type = {parameter_type};\n\n'.format(
                        name=key,
                        default_value=property_defines[key][KEY_DEFAULT_VALUE],
                        property_id='ePROPERTY_{ID}'.format(ID=key.upper()),
                        parameter_type=DATA_PROPERTY_TYPE[property_defines[key][KEY_TYPE]]
                    )
                """
            else:
                property_init_code += \
                    'pData->m_{name} = 0;\n' \
                    'ADD_PROPERTY_PARA({name}, {parameter_type}, &pDataTemplate->property[{property_id}].data_property, &pData->m_{name});\n'.format(
                        name=key,
                        property_id='ePROPERTY_{ID}'.format(ID=key.upper()),
                        parameter_type=DATA_PROPERTY_TYPE[property_defines[key][KEY_TYPE]]
                    )
                """old style
                property_init_code += \
                    'pData->m_{name} = 0;\n' \
                    'pDataTemplate->property[{property_id}].data_property.key  = "{name}";\n' \
                    'pDataTemplate->property[{property_id}].data_property.data = &pData->m_{name};\n' \
                    'pDataTemplate->property[{property_id}].data_property.type = {parameter_type};\n\n'.format(
                        name=key,
                        property_id='ePROPERTY_{ID}'.format(ID=key.upper()),
                        parameter_type=DATA_PROPERTY_TYPE[property_defines[key][KEY_TYPE]]
                    )
                """
        property_code[CODE_PROPERTY_INIT] = property_init_code[:-1].replace('\n', '\n    ')

        # # CODE_PROPERTY_FREE = '#To Generate Free Properties'
        # property_free_code = 'HAL_Free(container_of(pTemplateData->property[{property_id}].data_property.data, '\
        #                      'ProductDataDefine, m_{name}));\n'.format(
        #                          name=property_id[0],
        #                          property_id='ePROPERTY_{ID}'.format(ID=property_id[0].upper())
        #                      )
        # property_code[CODE_PROPERTY_FREE] = property_free_code

        ## CODE_PROPERTY_GEN_PARAMETER = '#To generate Get Product Parameter'
        property_get_product_para_code = 'pTemplateData->property[{property_id}].data_property.data, ProductDataDefine, m_{name}'.format(
                                            name=property_id[0],
                                            property_id='ePROPERTY_{ID}'.format(ID=property_id[0].upper())
                                        )
        property_code[CODE_PROPERTY_GEN_PARAMETER] = property_get_product_para_code
        return property_code

    def generate_events_code(self):
        """Generate event code."""
        events = self.__get_events()
        event_count = 0
        event_id = []
        event_parameters = {}
        event_types = {}
        for items in events:
            event_id.append(items[KEY_ID])
            event_types[items[KEY_ID]] = items[KEY_TYPE]
            event_parameters[items[KEY_ID]] = items[KEY_PARAMS]
            event_count = event_count + 1

        event_code = {}

        #CODE_EVENT_TOTAL_COUNT = '#To Generate Event Count'
        event_count_code = '#define TOTAL_EVENT_COUNTS {count}'.format(count=event_count)
        event_code[CODE_EVENT_TOTAL_COUNT] = event_count_code

        #CODE_EVENT_PARAMETER_NUM = '#To Generate Events Parameter Num'
        event_parameters_count = 0
        event_para_num_code = ''
        for key in event_id:
            for items in event_parameters[key]:
                event_parameters_count = event_parameters_count + 1
            event_para_num_code += '#define EVENT_{name}_PARA_NUM {count}\n'.format(
                name=key.upper(),
                count=event_parameters_count
                )
            event_parameters_count = 0    
        event_code[CODE_EVENT_PARAMETER_NUM] = event_para_num_code

        #CODE_EVENT_ID = '#To Generate Events ID'
        event_id_code = ''
        for key in event_id:
            event_id_code += 'eEVENT_{ID},\n'.format(ID=key.upper())
        event_code[CODE_EVENT_ID] = event_id_code[:-1].replace('\n', '\n    ')

        #CODE_EVENT_PARAMETER = '#To Generate Events Parameters'
        event_parameter_code = ''
        for event in event_id:
            for params in event_parameters[event]:
                event_parameter_defines = params[KEY_DEFINE]
                if event_parameter_defines[KEY_TYPE] == 'string':
                    event_parameter_code += '{parameter_type} m_{event_name}_{parameter_name}[{max_len} + 1];\n'.format(
                        parameter_type=PARAMETER_TYPE[event_parameter_defines[KEY_TYPE]],
                        event_name=event,
                        parameter_name=params[KEY_ID],
                        max_len=event_parameter_defines[KEY_MAX]
                    )
                else:
                    event_parameter_code += '{parameter_type} m_{event_name}_{parameter_name};\n'.format(
                        parameter_type=PARAMETER_TYPE[event_parameter_defines[KEY_TYPE]],
                        event_name=event,
                        parameter_name=params[KEY_ID]
                    )
        event_code[CODE_EVENT_PARAMETER] = event_parameter_code[:-1].replace('\n', '\n    ')

        #CODE_EVENT_NUM_SUM = '#To Generate Events Parameter Sum'
        event_num_sum_code = ''
        for key in event_id:
            event_num_sum_code += 'EVENT_{name}_PARA_NUM + '.format(name=key.upper())
        event_code[CODE_EVENT_NUM_SUM] = event_num_sum_code[:-3] # get rid of '+' in the end

        #CODE_EVENT_INIT = '#To Generate Init Events'
        event_int_code = ''
        event_param_num_sum = '0'
        for event in event_id:
            for params in event_parameters[event]:
                event_parameter_defines = params[KEY_DEFINE]
                if event_parameter_defines[KEY_TYPE] == 'string':
                    event_int_code += 'ADD_EVENT_PARA_STR({event_name}, {parameter_name}, TYPE_TEMPLATE_STRING, pEventsPara, para_no++);\n'.format(
                        event_name=event,
                        parameter_name=params[KEY_ID]
                    )
                else:
                    event_int_code += 'ADD_EVENT_PARA({event_name}, {parameter_name}, {parameter_type}, pEventsPara, para_no++);\n'.format(
                        event_name=event,
                        parameter_name=params[KEY_ID],
                        parameter_type=DATA_PROPERTY_TYPE[event_parameter_defines[KEY_TYPE]]
                    )

            event_int_code += 'ADD_EVENT({event_name}, {event_type}, &pEvents[{event_id}], &pEventsPara->evt_para[{param_num_sum}], {parameter_num});\n\n'.format(
                event_name=event,
                event_type=event_types[event],
                event_id='eEVENT_{ID}'.format(ID=event.upper()),
                param_num_sum=event_param_num_sum,
                parameter_num='EVENT_{name}_PARA_NUM'.format(name=event.upper())
            )
            if event_param_num_sum == '0':
                event_param_num_sum = 'EVENT_{name}_PARA_NUM'.format(name=event.upper())
            else:
                event_param_num_sum = event_param_num_sum + ' + ' + 'EVENT_{name}_PARA_NUM'.format(name=event.upper())
        event_code[CODE_EVENT_INIT] = event_int_code[:-1].replace('\n', '\n    ')
        return event_code

    def generate_actions_code(self):
        """Generate action code."""
        actions = self.__get_actions()
        action_count = 0
        action_id = []
        action_inputs = {}
        action_outputs = {}
        for items in actions:
            action_id.append(items[KEY_ID])
            action_inputs[items[KEY_ID]] = items[KEY_ACTION_INPUT]
            action_outputs[items[KEY_ID]] = items[KEY_ACTION_OUTPUT]
            action_count = action_count + 1

        action_code = {}

        #CODE_ACTION_TOTAL_COUNT = '#To Generate Action Count'
        action_count_code = '#define TOTAL_ACTION_COUNTS {count}'.format(count=action_count)
        action_code[CODE_ACTION_TOTAL_COUNT] = action_count_code

        #CODE_ACTION_PARAMETER_NUM = '#To Generate Actions Parameter Num'
        action_input_parameters_count = 0
        for items in action_inputs:
            for params in action_inputs[items]:
                action_input_parameters_count = action_input_parameters_count + 1

        action_output_parameters_count = 0
        for items in action_outputs:
            for params in action_outputs[items]:
                action_output_parameters_count = action_output_parameters_count + 1

        action_para_num_code = ''
        for key in action_id:
            action_para_num_code += '#define ACTION_{name}_INPUT_PARA_NUM {count}\n'.format(
                name=key.upper(),
                count=action_input_parameters_count
                )
            action_para_num_code += '#define ACTION_{name}_OUTPUT_PARA_NUM {count}\n'.format(
                name=key.upper(),
                count=action_output_parameters_count
                )
        action_code[CODE_ACTION_PARAMETER_NUM] = action_para_num_code

        #CODE_ACTION_ID = '#To Generate Actions ID'
        action_id_code = ''
        for key in action_id:
            action_id_code += 'eACTION_{ID},\n'.format(ID=key.upper())
        action_code[CODE_ACTION_ID] = action_id_code[:-1].replace('\n', '\n    ')

        #CODE_ACTION_PARAMETER = '#To Generate Actions Parameter'
        action_parameter_code = ''
        for action in action_id:
            for params in action_inputs[action]:
                action_parameter_defines = params[KEY_DEFINE]
                if action_parameter_defines[KEY_TYPE] == 'string':
                    action_parameter_code += '{parameter_type} m_{action_name}_input_{parameter_name}[{max_len} + 1];\n'.format(
                        parameter_type=PARAMETER_TYPE[action_parameter_defines[KEY_TYPE]],
                        action_name=action,
                        parameter_name=params[KEY_ID],
                        max_len=action_parameter_defines[KEY_MAX]
                    )
                else:
                    action_parameter_code += '{parameter_type} m_{action_name}_input_{parameter_name};\n'.format(
                        parameter_type=PARAMETER_TYPE[action_parameter_defines[KEY_TYPE]],
                        action_name=action,
                        parameter_name=params[KEY_ID]
                    )
            for params in action_outputs[action]:
                action_parameter_defines = params[KEY_DEFINE]
                if action_parameter_defines[KEY_TYPE] == 'string':
                    action_parameter_code += '{parameter_type} m_{action_name}_output_{parameter_name}[{max_len} + 1];\n'.format(
                        parameter_type=PARAMETER_TYPE[action_parameter_defines[KEY_TYPE]],
                        action_name=action,
                        parameter_name=params[KEY_ID],
                        max_len=action_parameter_defines[KEY_MAX]
                    )
                else:
                    action_parameter_code += '{parameter_type} m_{action_name}_output_{parameter_name};\n'.format(
                        parameter_type=PARAMETER_TYPE[action_parameter_defines[KEY_TYPE]],
                        action_name=action,
                        parameter_name=params[KEY_ID]
                    )
        action_code[CODE_ACTION_PARAMETER] = action_parameter_code[:-1].replace('\n', '\n    ')

        #CODE_ACTION_NUM_SUM = '#To Generate Actions Parameter Num Sum'
        action_num_sum_code = ''
        for key in action_id:
            action_num_sum_code += 'ACTION_{name}_INPUT_PARA_NUM + '.format(name=key.upper())
            action_num_sum_code += 'ACTION_{name}_OUTPUT_PARA_NUM + '.format(name=key.upper())
        action_code[CODE_ACTION_NUM_SUM] = action_num_sum_code[:-3] # get rid of '+' in the end

        #CODE_ACTION_INIT = '#To Generate Init Actions'
        action_int_code = ''
        action_param_num_sum = '0'
        for action in action_id:
            for params in action_inputs[action]:
                action_parameter_defines = params[KEY_DEFINE]
                if action_parameter_defines[KEY_TYPE] == 'string':
                    action_int_code += 'ADD_ACTION_PARA({action_name}, {parameter_name}, TYPE_TEMPLATE_STRING, pActionsPara, para_no++, input);\n'.format(
                        action_name=action,
                        parameter_name=params[KEY_ID]
                    )
                else:
                    action_int_code += 'ADD_ACTION_PARA({action_name}, {parameter_name}, {parameter_type}, pActionsPara, para_no++, input);\n'.format(
                        action_name=action,
                        parameter_name=params[KEY_ID],
                        parameter_type=DATA_PROPERTY_TYPE[action_parameter_defines[KEY_TYPE]]
                    )
            for params in action_outputs[action]:
                action_parameter_defines = params[KEY_DEFINE]
                if action_parameter_defines[KEY_TYPE] == 'string':
                    action_int_code += 'ADD_ACTION_PARA({action_name}, {parameter_name}, TYPE_TEMPLATE_STRING, pActionsPara, para_no++, output);\n'.format(
                        action_name=action,
                        parameter_name=params[KEY_ID]
                    )
                else:
                    action_int_code += 'ADD_ACTION_PARA({action_name}, {parameter_name}, {parameter_type}, pActionsPara, para_no++, output);\n'.format(
                        action_name=action,
                        parameter_name=params[KEY_ID],
                        parameter_type=DATA_PROPERTY_TYPE[action_parameter_defines[KEY_TYPE]]
                    )

            action_int_code += \
                        'ADD_ACTION({action_name}, &pActions[{action_id}], &pActionsPara->action_para[{param_num_sum}], ' \
                        '{input_parameter_num}, &pActionsPara->action_para[{output_num_sum}], {output_parameter_num});\n\n'.format(
                action_name=action,
                action_id='eACTION_{ID}'.format(ID=action.upper()),
                param_num_sum=action_param_num_sum,
                input_parameter_num='ACTION_{name}_INPUT_PARA_NUM'.format(name=action.upper()),
                output_num_sum=action_param_num_sum + ' + ' + 'ACTION_{name}_INPUT_PARA_NUM'.format(name=action.upper()),
                output_parameter_num='ACTION_{name}_OUTPUT_PARA_NUM'.format(name=action.upper())
            )
            if action_param_num_sum == '0':
                action_param_num_sum = 'ACTION_{name}_INPUT_PARA_NUM'.format(name=action.upper())
                action_param_num_sum += 'ACTION_{name}_OUTPUT_PARA_NUM'.format(name=action.upper())
            else:
                action_param_num_sum = action_param_num_sum + ' + ' + \
                                      'ACTION_{name}_INPUT_PARA_NUM'.format(name=action.upper()) + ' + ' + \
                                      'ACTION_{name}_OUTPUT_PARA_NUM'.format(name=action.upper())
        action_code[CODE_ACTION_INIT] = action_int_code[:-1].replace('\n', '\n    ')
        return action_code

if __name__ == '__main__':
    PARSER = argparse.ArgumentParser(
        description='IoT Explorer config code generator.',
        usage='use "./codegen.py -c <json file> -d <output file path>'
              'eg: ./codegen.py -c example_config.json  -d ."'
    )
    PARSER.add_argument(
        '-c', '--config',
        dest='config',
        metavar='xxx.json',
        required=True, default=None,
        help='copy the generated file (data_config.c) to sample/data_tamplate/ '
        'or your own code dir with datatemplate.'
        '\n config file can be download from tencent IoT Explorer platform.'
        '\n See https://console.cloud.tencent.com/iotexplorer'
    )
    PARSER.add_argument(
        '-d', '--dest',
        dest='dest',
        required=False, default='.',
        help='dest directory for generated code files, no / at the end.'
    )
    PARSER = PARSER.parse_args()

    if not os.path.exists(PARSER.config):
        print('错误：配置文件{config_file}不存在，请重新指定数据模板配置文件路径,请参考用法 '
              './codegen.py -c xx/data_template.json'.format(config_file=PARSER.config))

    try:
        TEMPLATE = DataTemplate(PARSER.config)
        SRC_CONFIG_C_FILE = os.path.dirname(os.path.abspath(__file__)) + '/data_template_config_base.c'
        SRC_CONFIG_H_FILE = os.path.dirname(os.path.abspath(__file__)) + '/data_template_config_base.h'
        DST_CONFIG_C_FILE = PARSER.dest + '/data_template_config.c'
        DST_CONFIG_H_FILE = PARSER.dest + '/data_template_config.h'

        # get code
        PROPERTY_CODE = TEMPLATE.generate_properties_code()
        EVENT_CODE = TEMPLATE.generate_events_code()
        ACTION_CODE = TEMPLATE.generate_actions_code()

        # generate data_template_config.c in dest path
        with open(SRC_CONFIG_C_FILE, 'r', encoding='UTF-8') as src_file:
            with open(DST_CONFIG_C_FILE, 'w+', encoding='UTF-8') as dst_file:
                content = src_file.read()
                for code in PROPERTY_CODE:
                    if code in content:
                        content = content.replace(code, PROPERTY_CODE[code])
                for code in EVENT_CODE:
                    if code in content:
                         content = content.replace(code, EVENT_CODE[code])
                for code in ACTION_CODE:
                    if code in content:
                         content = content.replace(code, ACTION_CODE[code])
                dst_file.write(content)
        print("文件 {file} 生成成功".format(file=DST_CONFIG_C_FILE))

        # generate data_template_config.h in dest path
        with open(SRC_CONFIG_H_FILE, 'r', encoding='UTF-8') as src_file:
            with open(DST_CONFIG_H_FILE, 'w+', encoding='UTF-8') as dst_file:
                content = src_file.read()
                for code in PROPERTY_CODE:
                    if code in content:
                        content = content.replace(code, PROPERTY_CODE[code])
                for code in EVENT_CODE:
                    if code in content:
                         content = content.replace(code, EVENT_CODE[code])
                for code in ACTION_CODE:
                    if code in content:
                         content = content.replace(code, ACTION_CODE[code])
                dst_file.write(content)
        print("文件 {file} 生成成功".format(file=DST_CONFIG_H_FILE))
        
    except ValueError:
        print("错误：文件格式非法，请检查 {config_file} 文件是否与物联网开发平台定义一致。".format(config_file=PARSER.config))
