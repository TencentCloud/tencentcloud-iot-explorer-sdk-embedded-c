
#define TOTAL_ACTION_COUNTS (1)

static TYPE_DEF_TEMPLATE_BOOL sg_pre_or_next_in_pre_or_next = 0;
static DeviceProperty         g_actionInput_pre_or_next[]   = {

    {.key = "pre_or_next", .data = &sg_pre_or_next_in_pre_or_next, .type = TYPE_TEMPLATE_ENUM},
};
static TYPE_DEF_TEMPLATE_INT sg_pre_or_next_out_code      = 0;
static DeviceProperty        g_actionOutput_pre_or_next[] = {

    {.key = "code", .data = &sg_pre_or_next_out_code, .type = TYPE_TEMPLATE_INT},
};

static DeviceAction g_actions[] = {

    {
        .pActionId  = "_sys_pre_next",
        .timestamp  = 0,
        .input_num  = sizeof(g_actionInput_pre_or_next) / sizeof(g_actionInput_pre_or_next[0]),
        .output_num = sizeof(g_actionOutput_pre_or_next) / sizeof(g_actionOutput_pre_or_next[0]),
        .pInput     = g_actionInput_pre_or_next,
        .pOutput    = g_actionOutput_pre_or_next,
    },
};
