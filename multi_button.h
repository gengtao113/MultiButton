/*
 * Copyright (c) 2016 Zibin Zheng <znbin@qq.com>
 * All rights reserved
 */

#ifndef _MULTI_BUTTON_H_
#define _MULTI_BUTTON_H_

#include <stdint.h>
#include <string.h>

// 配置常量 - 可以根据需求进行修改
/* 定义定时器中断的时间间隔为5毫秒。这个间隔决定了系统的时间粒度，通常用于计时和控制事件的触发频率。 */

#define TICKS_INTERVAL          5    // ms - 定时器中断的间隔时间（5毫秒）

/* 定义去抖动计数的深度为3。去抖动机制用于避免由于按键物理弹跳引起的多次触发。这里的3表示按键状态变化时，必须连续3次电平稳定才会认为状态有效。 */
#define DEBOUNCE_TICKS          3    // 最大为7（0 ~ 7）- 去抖动过滤深度

/* 计算短按的阈值。假设短按事件发生的时间限制为300毫秒，通过将其除以定时器中断间隔来得到对应的计数值。
 *  例如，TICKS_INTERVAL = 5，则 SHORT_TICKS = 300 / 5 = 60，表示短按的阈值为60个定时器中断。 
*/
#define SHORT_TICKS             (300 / TICKS_INTERVAL)   // // 短按阈值


/* 计算长按的阈值。假设长按事件发生的时间限制为1000毫秒，同样通过定时器中断间隔进行转换。
 *  例如，TICKS_INTERVAL = 5，则 LONG_TICKS = 1000 / 5 = 200，表示长按的阈值为200个定时器中断。
 */
#define LONG_TICKS              (1000 / TICKS_INTERVAL)  // 长按阈值

/* 定义按键重复按下的最大次数为15。用于在按键被持续按住时，控制事件触发的最大次数，防止触发无限循环事件。 */
#define PRESS_REPEAT_MAX_NUM    15   // 最大重复计数值


// Forward declaration
typedef struct _Button Button;

// Button callback function type
typedef void (*BtnCallback)(Button* btn_handle);

// Button event types
// Button event types
typedef enum {
    BTN_PRESS_DOWN = 0,     // button pressed down, 按键被按下时的事件
    BTN_PRESS_UP,           // button released, 按键释放时的事件
    BTN_PRESS_REPEAT,       // repeated press detected, 按键被重复按下的事件
    BTN_SINGLE_CLICK,       // single click completed, 单击事件完成
    BTN_DOUBLE_CLICK,       // double click completed, 双击事件完成
    BTN_LONG_PRESS_START,   // long press started, 长按事件开始
    BTN_LONG_PRESS_HOLD,    // long press holding, 长按事件持续中
    BTN_EVENT_COUNT,        // total number of events, 按键事件总数
    BTN_NONE_PRESS          // no event, 没有事件发生
} ButtonEvent;


// Button state machine states
typedef enum {
    BTN_STATE_IDLE = 0,     // idle state, 空闲状态，表示按键处于未按下状态
    BTN_STATE_PRESS,        // pressed state, 按下状态，表示按键正在被按下
    BTN_STATE_RELEASE,      // released state waiting for timeout, 释放状态，表示按键被释放并在等待超时
    BTN_STATE_REPEAT,       // repeat press state, 重复按下状态，表示按键被重复按下
    BTN_STATE_LONG_HOLD     // long press hold state, 长按状态，表示按键处于长按状态
} ButtonState;


// Button structure
// 按键结构体定义
struct _Button {
    uint16_t ticks;                     ///< 节拍计数器，用于记录当前状态下持续的时间（单位为扫描周期）

    uint8_t  repeat : 4;                ///< 重复次数计数器，占 4 位（0~15），用于识别连按/双击等操作

    uint8_t  event : 4;                 ///< 当前按键事件，占 4 位（0~15），例如按下、释放、单击等，用于状态传递

    uint8_t  state : 3;                 ///< 按键状态机的状态，占 3 位（0~7），如空闲、按下、释放、长按等状态

    uint8_t  debounce_cnt : 3;          ///< 去抖动计数器，占 3 位（0~7），用于过滤按键抖动，确保状态稳定再切换

    uint8_t  active_level : 1;          ///< 按键有效电平，占 1 位（0 或 1），表示按键按下时的 GPIO 有效电平

    uint8_t  button_level : 1;          ///< 当前读取的按键电平，占 1 位（0 或 1），表示实际读取到的电平状态

    uint8_t  button_id;                 ///< 按键标识符，用于区分多个按键或在 HAL 层回调中传递参数

    uint8_t  (*hal_button_level)(uint8_t button_id);  ///< HAL 层函数指针，根据按键 ID 读取 GPIO 电平

    BtnCallback cb[BTN_EVENT_COUNT];    ///< 回调函数数组，对应不同事件（如按下、释放、单击、双击、长按等）的处理函数

    Button* next;                       ///< 指向下一个按键结构体指针，用于将多个按键结构组织成单向链表
};



#ifdef __cplusplus
extern "C" {
#endif

// Public API functions
void button_init(Button* handle, uint8_t(*pin_level)(uint8_t), uint8_t active_level, uint8_t button_id);
void button_attach(Button* handle, ButtonEvent event, BtnCallback cb);
void button_detach(Button* handle, ButtonEvent event);
ButtonEvent button_get_event(Button* handle);
int  button_start(Button* handle);
void button_stop(Button* handle);
void button_ticks(void);

// Utility functions
uint8_t button_get_repeat_count(Button* handle);
void button_reset(Button* handle);
int button_is_pressed(Button* handle);

#ifdef __cplusplus
}
#endif

#endif
