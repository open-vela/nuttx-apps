#include "hourglass_page.h"

#define PART_WIDTH_HEIGHT 130
#define CHILD_WIDTH_HEIGHT 16
#define EXPAND_WIDTH 50
#define PADDING_WIDTH 5
#define ROWS_VALUE 8
#define CLOUMS_VALUE 8

static void SetStyle_PartChildren(lv_style_t *style);
static void SetStyle_Part(lv_style_t *style);
static void Menu_Create( lv_obj_t *parent,lv_obj_t *obj);

typedef enum{
    LV_EVENT_USER_START = 100,
}lv_event_user_code_t;

static void plus_event_cb(lv_event_t * e);
static void minus_event_cb(lv_event_t * e);
static void start_event_cb(lv_event_t * e);
static void excu_event_cb(lv_event_t * e);

lv_obj_t *start_btn = NULL;
lv_obj_t *part_parent = NULL;
lv_obj_t *plus_label = NULL;
lv_obj_t *minus_label = NULL;
lv_obj_t *time_label = NULL;

int time_value = 1;

typedef struct post1{
    lv_obj_t *obj;
    bool state;
}Post1_t;

Post1_t (*matrix_top_ptr)[CLOUMS_VALUE] = NULL;
Post1_t (*matrix_bottom_ptr)[CLOUMS_VALUE] = NULL;
Post1_t matrix_top[ROWS_VALUE][CLOUMS_VALUE];
Post1_t matrix_bottom[ROWS_VALUE][CLOUMS_VALUE];

//bool status[64];
//bool backupStatus[64];


void sandglass_page_create(void)
{

    lv_obj_t *menu = NULL;

    static lv_style_t  part_style;
    static lv_style_t  children_style;
    SetStyle_Part(&part_style);
    SetStyle_PartChildren(&children_style);
    lv_obj_t *screen = lv_screen_active();

// 创建父对象
    part_parent = lv_obj_create(screen);
    lv_obj_clear_flag(part_parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_opa(part_parent, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_size(part_parent, 480,272);
    lv_obj_align(part_parent, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(part_parent, 30, LV_PART_MAIN); // 四角圆角半径150
    lv_obj_set_style_bg_color(part_parent, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_border_width(part_parent, 0, LV_PART_MAIN); // 父对象无边框
    lv_obj_add_event_cb(part_parent, excu_event_cb, (lv_event_code_t)LV_EVENT_USER_START, NULL);


    lv_obj_t *part_top = lv_obj_create(part_parent);
    lv_obj_remove_style_all(part_top);
    lv_obj_add_style(part_top, &part_style, LV_PART_MAIN);
    lv_obj_align(part_top,LV_ALIGN_RIGHT_MID,-20,0);
    lv_obj_set_style_transform_pivot_x(part_top, PART_WIDTH_HEIGHT / 2,LV_PART_MAIN);   // 中心点 X 轴（百分比）
    lv_obj_set_style_transform_pivot_y(part_top, PART_WIDTH_HEIGHT / 2,LV_PART_MAIN);   // 中心点 Y 轴
    lv_obj_set_style_transform_rotation(part_top, 450,LV_PART_MAIN | LV_STATE_DEFAULT); // 旋转角度
    lv_obj_set_style_clip_corner(part_top,false,0);
    lv_obj_clear_flag(part_top,LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

// 配置子对象统一样式
    // const int rows = 8;
    // const int cloums = 8;
    // 生成子对象
    for (int i = 0; i < ROWS_VALUE ; i++) {
        for (int j = 0; j < CLOUMS_VALUE; j++) {
            lv_obj_t* child = lv_obj_create(part_top);
            lv_obj_add_style(child, &children_style, LV_PART_MAIN);
            lv_obj_set_pos(child,i*CHILD_WIDTH_HEIGHT,j*CHILD_WIDTH_HEIGHT);

           matrix_top[i][j].obj = child;
           matrix_top[i][j].state = true;
           if ((i == 6 || i == 7) && (j == 0 || j == 1))
           {
                lv_obj_set_style_bg_color(matrix_top[i][j].obj, lv_color_white(), LV_PART_MAIN);
                matrix_top[i][j].state = false;
           }
        }
    }
    matrix_top_ptr = matrix_top;

    lv_obj_t *part_bottom = lv_obj_create(part_parent);
    lv_obj_remove_style_all(part_bottom);
    lv_obj_add_style(part_bottom, &part_style, LV_PART_MAIN);
    lv_obj_align(part_bottom,LV_ALIGN_LEFT_MID,111,0);
    lv_obj_set_style_transform_pivot_x(part_bottom, PART_WIDTH_HEIGHT / 2,LV_PART_MAIN);   // 中心点 X 轴（百分比）
    lv_obj_set_style_transform_pivot_y(part_bottom, PART_WIDTH_HEIGHT / 2,LV_PART_MAIN);   // 中心点 Y 轴
    lv_obj_set_style_transform_rotation(part_bottom, 450,LV_PART_MAIN | LV_STATE_DEFAULT); // 旋转角度
    lv_obj_set_style_clip_corner(part_bottom,false,0);
    lv_obj_clear_flag(part_bottom,LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

// 生成子对象
    for (int i = 0; i < ROWS_VALUE; i++) {
        for (int j = 0; j < CLOUMS_VALUE; j++) {
            lv_obj_t* child = lv_obj_create(part_bottom);
            lv_obj_add_style(child, &children_style, LV_PART_MAIN);
            lv_obj_set_pos(child,i*CHILD_WIDTH_HEIGHT,j*CHILD_WIDTH_HEIGHT);

            matrix_bottom[i][j].obj = child;
            lv_obj_set_style_bg_color(matrix_bottom[i][j].obj, lv_color_white(), LV_PART_MAIN);
            matrix_bottom[i][j].state = false;
        }
    }
    matrix_bottom_ptr = matrix_bottom;
    Menu_Create(screen,menu);
}

static void SetStyle_PartChildren(lv_style_t *style)
{
    lv_style_init(style);
    lv_style_set_size(style, CHILD_WIDTH_HEIGHT,CHILD_WIDTH_HEIGHT);
    lv_style_set_border_width(style, 1);
    lv_style_set_border_color(style, lv_color_black());
    lv_style_set_border_opa(style, LV_OPA_20);
    lv_style_set_bg_color(style, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_opa(style, LV_OPA_COVER);
    lv_style_set_pad_all(style, 0); // 清除内边距[citation:3]
    lv_style_set_radius(style,0);

}

static void SetStyle_Part(lv_style_t *style)
{
    lv_style_init(style);
    lv_style_set_pad_all(style,0);
    lv_style_set_width(style,PART_WIDTH_HEIGHT);
    lv_style_set_height(style,PART_WIDTH_HEIGHT);
    lv_style_set_border_width(style, 1);
    lv_style_set_border_color(style, lv_color_black());
    lv_style_set_radius(style,0);

}

static void Menu_Create( lv_obj_t *parent,lv_obj_t *obj)
{

    obj =  lv_obj_create(parent);
    lv_obj_set_size(obj, 180, 52);
    lv_obj_align(obj,LV_ALIGN_LEFT_MID,-30,0);
    lv_obj_clear_flag(obj,LV_OBJ_FLAG_SCROLLABLE );
    lv_obj_set_style_transform_pivot_x(obj, 90,LV_PART_MAIN);   // 中心点 X 轴（百分比）
    lv_obj_set_style_transform_pivot_y(obj, 25,LV_PART_MAIN);   // 中心点 Y 轴
    lv_obj_set_style_transform_rotation(obj, 900,LV_PART_MAIN | LV_STATE_DEFAULT); // 旋转角度

    start_btn = lv_button_create(obj);
    lv_obj_align(start_btn,LV_ALIGN_LEFT_MID,-10,0);
    lv_obj_set_size(start_btn, 45, 25);
    lv_obj_clear_flag(start_btn,LV_OBJ_FLAG_SCROLLABLE );

    lv_obj_t * btn_label = lv_label_create(start_btn);
    lv_obj_center(btn_label);
    lv_label_set_text(btn_label, "Start");
    lv_obj_set_style_bg_color(btn_label, lv_color_hex(0xfe8d00), LV_PART_MAIN | LV_STATE_CHECKED);

    time_label = lv_label_create(obj);
    lv_obj_align(time_label,LV_ALIGN_LEFT_MID,75,0);
    lv_obj_set_size(time_label, 40,40);
    lv_obj_set_style_radius(time_label, 5, LV_PART_MAIN);
    lv_obj_set_style_border_width(time_label,1,LV_PART_MAIN);
    lv_obj_set_style_border_opa(time_label, LV_OPA_COVER,LV_PART_MAIN);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_16,LV_PART_MAIN);
    lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    char str[16] = {0};
    sprintf(str, "%d", time_value);
    lv_label_set_text(time_label, str);

    lv_obj_t *plus = lv_button_create(obj);
    lv_obj_align_to(plus,time_label,LV_ALIGN_OUT_LEFT_MID,15,0);
    lv_obj_set_size(plus, 20, 20);
    plus_label = lv_label_create(plus);
    lv_obj_align(plus_label,LV_ALIGN_CENTER,0,0);
    lv_label_set_text(plus_label, LV_SYMBOL_PLUS);

    lv_obj_t * minus = lv_button_create(obj);
    lv_obj_align_to(minus,time_label,LV_ALIGN_OUT_RIGHT_MID,5,0);
    lv_obj_set_size(minus, 20, 20);

    minus_label = lv_label_create(minus);
    lv_obj_align(minus_label,LV_ALIGN_CENTER,0,0);
    lv_label_set_text(minus_label, LV_SYMBOL_MINUS);

    lv_obj_add_event_cb(plus, plus_event_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(minus, minus_event_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(start_btn, start_event_cb, LV_EVENT_SHORT_CLICKED, NULL);

}

static void plus_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_SHORT_CLICKED) {
        LV_LOG_USER("Clicked");
        if (time_value >= 20)
        {
            time_value = 20;
        }
        else
        {
            time_value++;
        }
        char str[16] = {0};
        sprintf(str, "%d", time_value);
        lv_label_set_text(time_label, str);
    }
}

static void minus_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_SHORT_CLICKED) {
        LV_LOG_USER("Clicked");
        if (time_value <= 1)
        {
            time_value = 1;
        }
        else
        {
            time_value--;
        }
        char str[16] = {0};
        sprintf(str, "%d", time_value);
        lv_label_set_text(time_label, str);
    }
}

static void start_event_cb(lv_event_t * e){

    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_SHORT_CLICKED ) {
        return;
    }
    LV_LOG_USER("Clicked");
    reset_sand_hourglass();
}

static void excu_event_cb(lv_event_t * e){

    if(LV_EVENT_USER_START != (lv_event_user_code_t)lv_event_get_code(e)){
        return;
    }
}

