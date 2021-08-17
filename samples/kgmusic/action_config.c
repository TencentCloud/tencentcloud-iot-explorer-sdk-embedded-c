#ifdef ACTION_ENABLED

#define TOTAL_ACTION_COUNTS     (1)

static TYPE_DEF_TEMPLATE_BOOL sg_fresh_token_in_fresh_token = 0;
static DeviceProperty g_actionInput_fresh_token[] = {

   {.key = "_sys_fresh_token", .data = &sg_fresh_token_in_fresh_token, .type = TYPE_TEMPLATE_BOOL},
};
static TYPE_DEF_TEMPLATE_INT sg_fresh_token_out_Code = 0;
static DeviceProperty g_actionOutput_fresh_token[] = {

   {.key = "Code", .data = &sg_fresh_token_out_Code, .type = TYPE_TEMPLATE_INT},
};


static DeviceAction g_actions[]={

    {
     .pActionId = "_sys_fresh_token",
     .timestamp = 0,
     .input_num = sizeof(g_actionInput_fresh_token)/sizeof(g_actionInput_fresh_token[0]),
     .output_num = sizeof(g_actionOutput_fresh_token)/sizeof(g_actionOutput_fresh_token[0]),
     .pInput = g_actionInput_fresh_token,
     .pOutput = g_actionOutput_fresh_token,
    },
};

#endif
