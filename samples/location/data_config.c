/*-----------------data config start  -------------------*/

#define TOTAL_PROPERTY_COUNT 1

static sDataPoint sg_DataTemplate[TOTAL_PROPERTY_COUNT];

typedef struct _ProductDataDefine {
    TYPE_DEF_TEMPLATE_OBJECT m_GPS_Info;
} ProductDataDefine;

static ProductDataDefine sg_ProductData;

#define TOTAL_PROPERTY_STRUCT_GPS_INFO_COUNT 2

static sDataPoint sg_StructTemplateGps_Info[TOTAL_PROPERTY_STRUCT_GPS_INFO_COUNT];

typedef struct _StructDefineGps_Info {
    TYPE_DEF_TEMPLATE_FLOAT m_longtitude;
    TYPE_DEF_TEMPLATE_FLOAT m_latitude;
} StructDefineGps_Info;

static StructDefineGps_Info sg_StructDataGps_Info;

static void _init_struct_GPS_Info(void)
{
    // WGS84
    sg_StructDataGps_Info.m_longtitude              = 108.884031;
    sg_StructTemplateGps_Info[0].data_property.data = &sg_StructDataGps_Info.m_longtitude;
    sg_StructTemplateGps_Info[0].data_property.key  = "longtitude";
    sg_StructTemplateGps_Info[0].data_property.type = TYPE_TEMPLATE_FLOAT;
    sg_StructTemplateGps_Info[0].state              = eCHANGED;

    sg_StructDataGps_Info.m_latitude                = 34.212282;
    sg_StructTemplateGps_Info[1].data_property.data = &sg_StructDataGps_Info.m_latitude;
    sg_StructTemplateGps_Info[1].data_property.key  = "latitude";
    sg_StructTemplateGps_Info[1].data_property.type = TYPE_TEMPLATE_FLOAT;
    sg_StructTemplateGps_Info[1].state              = eCHANGED;
};

static void _init_data_template(void)
{
    _init_struct_GPS_Info();
    sg_ProductData.m_GPS_Info                       = (void *)&sg_StructTemplateGps_Info;
    sg_DataTemplate[0].data_property.data           = sg_ProductData.m_GPS_Info;
    sg_DataTemplate[0].data_property.struct_obj_num = TOTAL_PROPERTY_STRUCT_GPS_INFO_COUNT;
    sg_DataTemplate[0].data_property.key            = "GPS_Info";
    sg_DataTemplate[0].data_property.type           = TYPE_TEMPLATE_JOBJECT;
    sg_DataTemplate[0].state                        = eCHANGED;
};
