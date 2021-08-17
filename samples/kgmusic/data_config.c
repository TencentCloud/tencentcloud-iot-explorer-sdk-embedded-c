/*-----------------data config start  -------------------*/ 

#define TOTAL_PROPERTY_COUNT 10

static sDataPoint    sg_DataTemplate[TOTAL_PROPERTY_COUNT];

typedef struct _ProductDataDefine {
    TYPE_DEF_TEMPLATE_BOOL m_pause_play;
    TYPE_DEF_TEMPLATE_STRING m_cur_play_list[2048+1];
    TYPE_DEF_TEMPLATE_ENUM m_pre_next;
    TYPE_DEF_TEMPLATE_ENUM m_play_mode;
    TYPE_DEF_TEMPLATE_INT m_volume;
    TYPE_DEF_TEMPLATE_INT m_play_position;
    TYPE_DEF_TEMPLATE_STRING m_cur_song_id[2048+1];
    TYPE_DEF_TEMPLATE_INT m_control_seq;
    TYPE_DEF_TEMPLATE_ENUM m_recommend_quality;
    TYPE_DEF_TEMPLATE_INT m_song_index;
} ProductDataDefine;

static   ProductDataDefine     sg_ProductData;

static void _init_data_template(void)
{
    sg_ProductData.m_pause_play = 0;
    sg_DataTemplate[0].data_property.data = &sg_ProductData.m_pause_play;
    sg_DataTemplate[0].data_property.key  = "_sys_pause_play";
    sg_DataTemplate[0].data_property.type = TYPE_TEMPLATE_BOOL;
    sg_DataTemplate[0].state = eNOCHANGE;

    sg_ProductData.m_cur_play_list[0] = '\0';
    sg_DataTemplate[1].data_property.data = sg_ProductData.m_cur_play_list;
    sg_DataTemplate[1].data_property.data_buff_len = sizeof(sg_ProductData.m_cur_play_list)/sizeof(sg_ProductData.m_cur_play_list[0]);
    sg_DataTemplate[1].data_property.key  = "_sys_cur_play_list";
    sg_DataTemplate[1].data_property.type = TYPE_TEMPLATE_STRING;
    sg_DataTemplate[1].state = eNOCHANGE;

    sg_ProductData.m_pre_next = 0;
    sg_DataTemplate[2].data_property.data = &sg_ProductData.m_pre_next;
    sg_DataTemplate[2].data_property.key  = "_sys_pre_next";
    sg_DataTemplate[2].data_property.type = TYPE_TEMPLATE_ENUM;
    sg_DataTemplate[2].state = eNOCHANGE;

    sg_ProductData.m_play_mode = 0;
    sg_DataTemplate[3].data_property.data = &sg_ProductData.m_play_mode;
    sg_DataTemplate[3].data_property.key  = "_sys_play_mode";
    sg_DataTemplate[3].data_property.type = TYPE_TEMPLATE_ENUM;
    sg_DataTemplate[3].state = eNOCHANGE;

    sg_ProductData.m_volume = 0;
    sg_DataTemplate[4].data_property.data = &sg_ProductData.m_volume;
    sg_DataTemplate[4].data_property.key  = "_sys_volume";
    sg_DataTemplate[4].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[4].state = eNOCHANGE;

    sg_ProductData.m_play_position = 0;
    sg_DataTemplate[5].data_property.data = &sg_ProductData.m_play_position;
    sg_DataTemplate[5].data_property.key  = "_sys_play_position";
    sg_DataTemplate[5].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[5].state = eNOCHANGE;

    sg_ProductData.m_cur_song_id[0] = '\0';
    sg_DataTemplate[6].data_property.data = sg_ProductData.m_cur_song_id;
    sg_DataTemplate[6].data_property.data_buff_len = sizeof(sg_ProductData.m_cur_song_id)/sizeof(sg_ProductData.m_cur_song_id[0]);
    sg_DataTemplate[6].data_property.key  = "_sys_cur_song_id";
    sg_DataTemplate[6].data_property.type = TYPE_TEMPLATE_STRING;
    sg_DataTemplate[6].state = eNOCHANGE;

    sg_ProductData.m_control_seq = 0;
    sg_DataTemplate[7].data_property.data = &sg_ProductData.m_control_seq;
    sg_DataTemplate[7].data_property.key  = "_sys_control_seq";
    sg_DataTemplate[7].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[7].state = eNOCHANGE;

    sg_ProductData.m_recommend_quality = 0;
    sg_DataTemplate[8].data_property.data = &sg_ProductData.m_recommend_quality;
    sg_DataTemplate[8].data_property.key  = "_sys_recommend_quality";
    sg_DataTemplate[8].data_property.type = TYPE_TEMPLATE_ENUM;
    sg_DataTemplate[8].state = eNOCHANGE;

    sg_ProductData.m_song_index = 0;
    sg_DataTemplate[9].data_property.data = &sg_ProductData.m_song_index;
    sg_DataTemplate[9].data_property.key  = "_sys_song_index";
    sg_DataTemplate[9].data_property.type = TYPE_TEMPLATE_INT;
    sg_DataTemplate[9].state = eNOCHANGE;
};

