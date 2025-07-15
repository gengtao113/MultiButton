/*
 * Copyright (c) 2016 Zibin Zheng <znbin@qq.com>
 * All rights reserved
 */

#include "multi_button.h"

/**
 * @brief 执行按键事件对应的回调函数
 *
 * @param ev 事件类型（如 BTN_PRESS_DOWN、BTN_SINGLE_CLICK 等）
 *
 * @note
 * - 回调函数数组 `handle->cb[]` 中，每个索引对应一个具体事件；
 * - 如果对应事件的回调函数非空（即已注册），则调用它并传入当前按键结构体指针 `handle`；
 * - 使用 `do { ... } while(0)` 包裹，确保宏展开在多语句结构中行为一致，避免语法问题。
 *
 * @example
 * EVENT_CB(BTN_SINGLE_CLICK); // 如果注册了单击事件的回调函数，则执行它
 */
#define EVENT_CB(ev)   do { if(handle->cb[ev]) handle->cb[ev](handle); } while(0)

// Button handle list head
static Button* head_handle = NULL;

// Forward declarations
static void button_handler(Button* handle);
static inline uint8_t button_read_level(Button* handle);

/**
  * @brief  Initialize the button struct handle
  * @param  handle: the button handle struct 按键结构体句柄，用于存储按键信息
  * @param  pin_level: read the HAL GPIO of the connected button level 读取按键连接的GPIO电平的函数指针
  * @param  active_level: pressed GPIO level  按键被按下时的GPIO电平
  * @param  button_id: the button id  按键的唯一标识符
  * @retval None
  */
void button_init(Button* handle, uint8_t(*pin_level)(uint8_t), uint8_t active_level, uint8_t button_id)
{
	if (!handle || !pin_level) return;  // parameter validation 检查传入的参数是否合法，如果句柄或GPIO读取函数为空，则直接返回
	
	memset(handle, 0, sizeof(Button));  	 //清零按键结构体，初始化为0，防止未初始化的字段产生问题
	handle->event = (uint8_t)BTN_NONE_PRESS; // 设置事件为BTN_NONE_PRESS，表示当前没有事件发生
	handle->hal_button_level = pin_level;    // 保存HAL GPIO读取函数，用于读取按钮的当前电平状态
	handle->button_level = !active_level;    // 将当前按钮电平设置为活动电平的反值，初始化为按键未按下状态（GPIO电平可能是高电平或低电平）
	handle->active_level = active_level;     // 保存按键活动电平，表示按下时GPIO电平的状态
	handle->button_id = button_id;           // 保存按钮的唯一标识符，用于区分不同的按钮
	handle->state = BTN_STATE_IDLE;          // 初始化状态机为BTN_STATE_IDLE状态，表示按键处于空闲状态，未被按下
}

/**
  * @brief  注册（绑定）指定事件的回调函数
  * @param  handle: 按键句柄结构体指针
  * @param  event: 需要绑定的按键事件类型（如单击、双击、长按等）
  * @param  cb: 回调函数指针，在事件触发时调用
  * @retval None
  */
void button_attach(Button* handle, ButtonEvent event, BtnCallback cb)
{
    // 参数校验：确保按键句柄非空，事件编号合法
    if (!handle || event >= BTN_EVENT_COUNT) return;

    // 将回调函数赋值到事件对应的数组元素中
    handle->cb[event] = cb;
}

/**
  * @brief  解绑（移除）指定事件的回调函数
  * @param  handle: 按键句柄结构体指针
  * @param  event: 要解绑的按键事件类型
  * @retval None
  */
void button_detach(Button* handle, ButtonEvent event)
{
    // 参数校验：确保按键句柄非空，事件编号合法
    if (!handle || event >= BTN_EVENT_COUNT) return;

    // 将事件回调清空，表示不再处理此事件
    handle->cb[event] = NULL;
}


/**
  * @brief  获取发生的按键事件
  * @param  handle: 按键句柄结构体指针
  * @retval 按键事件类型（`ButtonEvent` 枚举值）
  */
ButtonEvent button_get_event(Button* handle)
{
    // 参数校验：如果按键句柄为空，返回未按下事件
    if (!handle) return BTN_NONE_PRESS;

    // 返回当前按键的事件类型
    return (ButtonEvent)(handle->event);
}


/**
  * @brief  获取按键的重复按下次数
  * @param  handle: 按键句柄结构体指针
  * @retval 重复按下次数
  */
uint8_t button_get_repeat_count(Button* handle)
{
    // 参数校验：如果按键句柄为空，返回 0（没有按下）
    if (!handle) return 0;

    // 返回当前按键的重复按下次数
    return handle->repeat;
}

/**
  * @brief  重置按键状态为初始状态（空闲）
  * @param  handle: 按键句柄结构体指针
  * @retval None
  */
void button_reset(Button* handle)
{
    // 参数校验：如果按键句柄为空，直接返回
    if (!handle) return;

    // 将按键状态重置为空闲状态
    handle->state = BTN_STATE_IDLE;

    // 重置计时器和事件相关变量
    handle->ticks = 0;                   // 重置 ticks 计数器
    handle->repeat = 0;                  // 重置重复计数器
    handle->event = (uint8_t)BTN_NONE_PRESS;  // 清空当前事件标识
    handle->debounce_cnt = 0;            // 清空去抖动计数器
}


/**
  * @brief  检查按键当前是否被按下
  * @param  handle: 按键句柄结构体指针
  * @retval 1: 按下，0: 未按下，-1: 错误
  */
int button_is_pressed(Button* handle)
{
    // 参数校验：如果按键句柄为空，返回 -1 表示错误
    if (!handle) return -1;

    // 判断按键电平是否为活动电平，如果是则表示按键被按下
    return (handle->button_level == handle->active_level) ? 1 : 0;
}


/**
  * @brief  以内联优化方式读取按键电平
  * @param  handle: 按键句柄结构体指针
  * @retval 按键电平（0 或 1）
  */
static inline uint8_t button_read_level(Button* handle)
{
    // 调用 HAL 层的函数读取按键电平状态
    return handle->hal_button_level(handle->button_id);
}

/**
  * @brief  按键驱动核心函数，驱动状态机
  * @param  handle: 按键结构体句柄
  * @retval None
  */
static void button_handler(Button* handle)
{
	// 读取按键的GPIO电平状态
	uint8_t read_gpio_level = button_read_level(handle);

	// 如果当前状态不是空闲状态，则递增 ticks 计数器
	if (handle->state > BTN_STATE_IDLE) 
	{
		handle->ticks++;
	}

	 /*------------按键去抖动处理---------------*/
	 // 如果当前读取的电平与上次记录的电平不一致，表示电平发生了变化
	if (read_gpio_level != handle->button_level) 
	{
		//去抖动计数器累加：当电平变化时，去抖动计数器 (handle->debounce_cnt) 增加 1
		//如果计数器的值大于等于设定的去抖动阈值 DEBOUNCE_TICKS（例如 3 次变化），那么认为电平变化是真正有效的（即去除掉了可能的抖动）。
		//此时，更新按键的电平状态 (handle->button_level = read_gpio_level)，并将计数器重置为 0
		if (++(handle->debounce_cnt) >= DEBOUNCE_TICKS) 
		{
			handle->button_level = read_gpio_level; // 更新按钮电平状态
			handle->debounce_cnt = 0;               // 重置去抖动计数器
		}
	} 
	else 
	{
		// 如果电平没有变化，重置去抖动计数器
		handle->debounce_cnt = 0;
	}

	/*-----------------状态机-------------------*/
	switch (handle->state) 
	{
	case BTN_STATE_IDLE:
        // 检测到按键被按下
		if (handle->button_level == handle->active_level) 
		{
			// 设置事件为BTN_PRESS_DOWN，表示按键被按下
			handle->event = (uint8_t)BTN_PRESS_DOWN;
			// 调用按键按下的事件回调
			EVENT_CB(BTN_PRESS_DOWN);
			handle->ticks = 0;    // 重置ticks计数器
			handle->repeat = 1;   // 设置重复计数器为1
			handle->state = BTN_STATE_PRESS; // 转到按下状态
		} 
		else 
		{
			handle->event = (uint8_t)BTN_NONE_PRESS; // 没有按键事件
		}
		break;

	case BTN_STATE_PRESS:
	 	// 按键被释放
		if (handle->button_level != handle->active_level) 
		{
			// 设置事件为BTN_PRESS_UP，表示按键被释放
			handle->event = (uint8_t)BTN_PRESS_UP;
			// 调用按键释放的事件回调
			EVENT_CB(BTN_PRESS_UP);
			// 重置ticks计数器
			handle->ticks = 0;
			// 转到释放状态，等待超时
			handle->state = BTN_STATE_RELEASE;
		} 
		else if (handle->ticks > LONG_TICKS) // 按键没有被释放，并且长按事件被触发
		{
			// 设置事件为BTN_LONG_PRESS_START，表示长按开始
			handle->event = (uint8_t)BTN_LONG_PRESS_START;
			// 调用长按开始的事件回调
			EVENT_CB(BTN_LONG_PRESS_START);
			// 转到长按状态
			handle->state = BTN_STATE_LONG_HOLD;
		}
		break;

	case BTN_STATE_RELEASE:
	    // 按键被重新按下
		if (handle->button_level == handle->active_level) 
		{
			// 设置事件为BTN_PRESS_DOWN
			handle->event = (uint8_t)BTN_PRESS_DOWN;
			// 调用按键按下的事件回调
			EVENT_CB(BTN_PRESS_DOWN);
			// 如果按键重复次数小于最大值 15，递增重复计数
			if (handle->repeat < PRESS_REPEAT_MAX_NUM) 
			{
				handle->repeat++;
			}
			 // 调用重复按键的事件回调
			EVENT_CB(BTN_PRESS_REPEAT);
			// 重置ticks计数器
			handle->ticks = 0;
			// 转到重复按状态
			handle->state = BTN_STATE_REPEAT;
		} 
		else if (handle->ticks > SHORT_TICKS)  //按键已经松开，并且判断是否超过短按阈值
		{
			// 超时，根据重复次数判断点击类型
			if (handle->repeat == 1) 
			{
				handle->event = (uint8_t)BTN_SINGLE_CLICK; // 设置事件为BTN_SINGLE_CLICK，表示单击
				EVENT_CB(BTN_SINGLE_CLICK);                // 调用单击的事件回调
			} 
			else if (handle->repeat == 2)  
			{
				handle->event = (uint8_t)BTN_DOUBLE_CLICK;   // 设置事件为BTN_DOUBLE_CLICK，表示双击
				EVENT_CB(BTN_DOUBLE_CLICK);                  // 调用双击的事件回调
			}
			handle->state = BTN_STATE_IDLE;                   // 转到空闲状态
		}
		break;

	case BTN_STATE_REPEAT:
	    //按键被释放
		if (handle->button_level != handle->active_level)
		{
			// 设置事件为BTN_PRESS_UP
			handle->event = (uint8_t)BTN_PRESS_UP;
			// 调用按键释放的事件回调
			EVENT_CB(BTN_PRESS_UP);
			// 如果按键按下时间小于SHORT_TICKS，继续等待释放
			if (handle->ticks < SHORT_TICKS) 
			{
				handle->ticks = 0;
				handle->state = BTN_STATE_RELEASE;  // Continue waiting for more presses
			} 
			else 
			{
				handle->state = BTN_STATE_IDLE;  // 按键释放后，回到空闲状态
			}
		} 
		else if (handle->ticks > SHORT_TICKS)   // 如果按下时间过长，视为正常按键
		{
			// Held down too long, treat as normal press
			handle->state = BTN_STATE_PRESS;
		}
		break;

	case BTN_STATE_LONG_HOLD:
	    // 长按持续中
		if (handle->button_level == handle->active_level) 
		{
			// 设置事件为BTN_LONG_PRESS_HOLD
			handle->event = (uint8_t)BTN_LONG_PRESS_HOLD;
			EVENT_CB(BTN_LONG_PRESS_HOLD);   // 调用长按持续的事件回调
		} 
		else 
		{
			// // 长按释放
			handle->event = (uint8_t)BTN_PRESS_UP; // 设置事件为BTN_PRESS_UP
			EVENT_CB(BTN_PRESS_UP);                // 调用按键释放的事件回调
			handle->state = BTN_STATE_IDLE;         // 转到空闲状态
		}
		break;

	default:
		// 如果状态无效，重置为空闲状态
		handle->state = BTN_STATE_IDLE;
		break;
	}
}

/**
  * @brief  启动按键处理，将按键结构体加入工作链表中
  * @param  handle: 目标按键结构体指针
  * @retval 0: 添加成功
  *         -1: 已存在，不能重复添加
  *         -2: 参数无效（为空指针）
  */
int button_start(Button* handle)
{
    // 参数检查：如果传入的按键指针为空，返回错误码 -2
    if (!handle) return -2;

    // 遍历按键链表，检查该按键是否已经存在于链表中，防止重复添加
    Button* target = head_handle;
    while (target) {
        if (target == handle)
            return -1;  // 该按键已存在，返回错误码 -1
        target = target->next;
    }

    // 将该按键插入链表头部（头插法）
    handle->next = head_handle;  // 当前按键的 next 指向原链表头
    head_handle = handle;        // 更新链表头指针为当前按键

    return 0;  // 添加成功，返回 0
}


/**
  * @brief  停止按键工作，从链表中移除指定按键句柄
  * @param  handle: 目标按键结构体指针
  * @retval None
  */
void button_stop(Button* handle)
{
    // 参数检查：如果传入指针为 NULL，则直接返回
    if (!handle) return;

    // 使用指向指针的指针来遍历链表，便于修改链表结构
    Button** curr;

    // 从链表头开始遍历【*curr 是当前节点（即 Button* 类型），只要当前节点不为空，循环继续，一旦 *curr == NULL（即链表遍历到末尾），循环结束】
    for (curr = &head_handle; *curr; ) 
	{
        Button* entry = *curr;

        // 如果找到目标按键
        if (entry == handle) 
		{
            *curr = entry->next;     // 将当前指针指向下一个节点，实现删除操作
            entry->next = NULL;      // 清空被删除节点的 next 指针，防止野指针
            return;                  // 删除完成，返回
        } 
		else 
		{
            // 没找到，继续遍历下一个节点
            curr = &entry->next;
        }
    }
}


/**
  * @brief  后台定时扫描处理函数，每隔固定时间（如5ms）被周期性调用
  * @param  None
  * @retval None
  *
  * @note 此函数通常在定时器中断或RTOS定时任务中被调用，负责轮询所有注册的按键并处理其状态变化
  */
void button_ticks(void)
{
    Button* target;

    // 遍历所有已注册的按键句柄（通过链表 head_handle 管理）
    for (target = head_handle; target; target = target->next) {
        // 对每一个按键，执行状态机处理逻辑（包括去抖动、状态切换、事件判断等）
        button_handler(target);
    }
}

