/*-----------------data config start  -------------------*/

#define TOTAL_PROPERTY_COUNT 6

static sDataPoint sg_DataTemplate[TOTAL_PROPERTY_COUNT];

typedef struct _ProductDataDefine {
    TYPE_DEF_TEMPLATE_BOOL       m_power_switch;
    TYPE_DEF_TEMPLATE_ENUM       m_color;
    TYPE_DEF_TEMPLATE_INT        m_brightness;
    TYPE_DEF_TEMPLATE_STRING     m_name[64 + 1];
    TYPE_DEF_TEMPLATE_OBJECT     m_position;
    TYPE_DEF_TEMPLATE_STRINGENUM m_power[6 + 1];
} ProductDataDefine;

static ProductDataDefine sg_ProductData;

#define TOTAL_PROPERTY_STRUCT_POSITION_COUNT 2

static sDataPoint sg_StructTemplatePosition[TOTAL_PROPERTY_STRUCT_POSITION_COUNT];

typedef struct _StructDefinePosition {
    TYPE_DEF_TEMPLATE_INT m_longitude;
    TYPE_DEF_TEMPLATE_INT m_latitude;
} StructDefinePosition;

static StructDefinePosition sg_StructDataPosition;

static void _init_struct_position(void)
{
    sg_StructDataPosition.m_longitude               = 1;
    sg_StructTemplatePosition[0].data_property.data = &sg_StructDataPosition.m_longitude;
    sg_StructTemplatePosition[0].data_property.key  = "longitude";
    sg_StructTemplatePosition[0].data_property.type = TYPE_TEMPLATE_INT;
    sg_StructTemplatePosition[0].state              = eCHANGED;

    sg_StructDataPosition.m_latitude                = 1;
    sg_StructTemplatePosition[1].data_property.data = &sg_StructDataPosition.m_latitude;
    sg_StructTemplatePosition[1].data_property.key  = "latitude";
    sg_StructTemplatePosition[1].data_property.type = TYPE_TEMPLATE_INT;
    sg_StructTemplatePosition[1].state              = eCHANGED;
};

static void _init_data_template(void)
{
    sg_ProductData.m_power_switch         = 0;
    sg_DataTemplate[0].data_property.data = &sg_ProductData.m_power_switch;
    sg_DataTemplate[0].data_property.key  = "power_switch";
    sg_DataTemplate[0].data_property.type = TYPE_TEMPLATE_BOOL;
    sg_DataTemplate[0].state              = eCHANGED;

    sg_ProductData.m_color                = 0;
    sg_DataTemplate[1].data_property.data = &sg_ProductData.m_color;
    sg_DataTemplate[1].data_property.key  = "color";
    sg_DataTemplate[1].data_property.type = TYPE_TEMPLATE_ENUM;
    sg_DataTemplate[1].state              = eCHANGED;

    sg_ProductData.m_brightness           = 1;
    sg_DataTemplate[2].data_property.data = &sg_ProductData.m_brightness;
    sg_DataTemplate[2].data_property.key  = "brightness";
    sg_DataTemplate[2].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[2].state              = eCHANGED;

    sg_ProductData.m_name[0]                       = '\0';
    sg_DataTemplate[3].data_property.data          = sg_ProductData.m_name;
    sg_DataTemplate[3].data_property.data_buff_len = sizeof(sg_ProductData.m_name) / sizeof(sg_ProductData.m_name[0]);
    sg_DataTemplate[3].data_property.key           = "name";
    sg_DataTemplate[3].data_property.type          = TYPE_TEMPLATE_STRING;
    sg_DataTemplate[3].state                       = eCHANGED;

    _init_struct_position();
    sg_ProductData.m_position                       = (void *)&sg_StructTemplatePosition;
    sg_DataTemplate[4].data_property.data           = sg_ProductData.m_position;
    sg_DataTemplate[4].data_property.struct_obj_num = TOTAL_PROPERTY_STRUCT_POSITION_COUNT;
    sg_DataTemplate[4].data_property.key            = "position";
    sg_DataTemplate[4].data_property.type           = TYPE_TEMPLATE_JOBJECT;
    sg_DataTemplate[4].state                        = eCHANGED;

#define E_POWER_HIGH   "high"
#define E_POWER_MEDIUM "medium"
#define E_POWER_LOW    "low"
    sg_ProductData.m_power[0]                      = '\0';
    sg_DataTemplate[5].data_property.data          = sg_ProductData.m_power;
    sg_DataTemplate[5].data_property.data_buff_len = sizeof(sg_ProductData.m_power) / sizeof(sg_ProductData.m_power[0]);
    sg_DataTemplate[5].data_property.key           = "power";
    sg_DataTemplate[5].data_property.type          = TYPE_TEMPLATE_STRINGENUM;
    sg_DataTemplate[5].state                       = eCHANGED;
};
