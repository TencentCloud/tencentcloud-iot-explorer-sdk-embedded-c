/*-----------------data config start  -------------------*/

typedef enum {
    M_PLAY_MODE_E = 0,
    M_VOLUME_E,
    M_PLAY_POSITION_E,
    M_RECOMMEND_QUALITY_E,
    M_CUR_SONG_ID_E,
    M_PLAY_PAUSE_E,
    M_SONG_LIST_E,
    TOTAL_PROPERTY_COUNT,
} ProductDataDefineIndex_E;

static sDataPoint sg_DataTemplate[TOTAL_PROPERTY_COUNT];

typedef struct _ProductDataDefine {
    TYPE_DEF_TEMPLATE_INT    m_play_mode;
    TYPE_DEF_TEMPLATE_INT    m_volume;
    TYPE_DEF_TEMPLATE_INT    m_play_position;
    TYPE_DEF_TEMPLATE_ENUM   m_recommend_quality;
    TYPE_DEF_TEMPLATE_INT    m_song_id;
    TYPE_DEF_TEMPLATE_BOOL   m_pause_play;
    TYPE_DEF_TEMPLATE_STRING m_song_list[2048 + 1];
} ProductDataDefine;

static ProductDataDefine sg_ProductData;

static void _init_data_template(void)
{
    sg_ProductData.m_pause_play                        = 0;
    sg_DataTemplate[M_PLAY_PAUSE_E].data_property.data = &sg_ProductData.m_pause_play;
    sg_DataTemplate[M_PLAY_PAUSE_E].data_property.key  = "_sys_pause_play";
    sg_DataTemplate[M_PLAY_PAUSE_E].data_property.type = TYPE_TEMPLATE_BOOL;
    sg_DataTemplate[M_PLAY_PAUSE_E].state              = eNOCHANGE;

    sg_ProductData.m_play_mode                        = 0;
    sg_DataTemplate[M_PLAY_MODE_E].data_property.data = &sg_ProductData.m_play_mode;
    sg_DataTemplate[M_PLAY_MODE_E].data_property.key  = "_sys_play_mode";
    sg_DataTemplate[M_PLAY_MODE_E].data_property.type = TYPE_TEMPLATE_ENUM;
    sg_DataTemplate[M_PLAY_MODE_E].state              = eNOCHANGE;

    sg_ProductData.m_volume                        = 0;
    sg_DataTemplate[M_VOLUME_E].data_property.data = &sg_ProductData.m_volume;
    sg_DataTemplate[M_VOLUME_E].data_property.key  = "_sys_volume";
    sg_DataTemplate[M_VOLUME_E].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[M_VOLUME_E].state              = eNOCHANGE;

    sg_ProductData.m_play_position                        = 0;
    sg_DataTemplate[M_PLAY_POSITION_E].data_property.data = &sg_ProductData.m_play_position;
    sg_DataTemplate[M_PLAY_POSITION_E].data_property.key  = "_sys_play_position";
    sg_DataTemplate[M_PLAY_POSITION_E].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[M_PLAY_POSITION_E].state              = eNOCHANGE;

    sg_ProductData.m_play_position                      = 0;
    sg_DataTemplate[M_CUR_SONG_ID_E].data_property.data = &sg_ProductData.m_song_id;
    sg_DataTemplate[M_CUR_SONG_ID_E].data_property.key  = "_sys_song_id";
    sg_DataTemplate[M_CUR_SONG_ID_E].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[M_CUR_SONG_ID_E].state              = eNOCHANGE;

    sg_ProductData.m_recommend_quality                        = 0;
    sg_DataTemplate[M_RECOMMEND_QUALITY_E].data_property.data = &sg_ProductData.m_recommend_quality;
    sg_DataTemplate[M_RECOMMEND_QUALITY_E].data_property.key  = "_sys_recommend_quality";
    sg_DataTemplate[M_RECOMMEND_QUALITY_E].data_property.type = TYPE_TEMPLATE_ENUM;
    sg_DataTemplate[M_RECOMMEND_QUALITY_E].state              = eNOCHANGE;

    sg_ProductData.m_song_list[0]                     = '\0';
    sg_DataTemplate[M_SONG_LIST_E].data_property.data = sg_ProductData.m_song_list;
    sg_DataTemplate[M_SONG_LIST_E].data_property.data_buff_len =
        sizeof(sg_ProductData.m_song_list) / sizeof(sg_ProductData.m_song_list[0]);
    sg_DataTemplate[M_SONG_LIST_E].data_property.key  = "_sys_song_list";
    sg_DataTemplate[M_SONG_LIST_E].data_property.type = TYPE_TEMPLATE_STRING;
    sg_DataTemplate[M_SONG_LIST_E].state              = eNOCHANGE;
};
