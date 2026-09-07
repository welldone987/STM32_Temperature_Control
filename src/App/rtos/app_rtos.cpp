// FreeRTOS 应用：创建四个应用任务、内核钩子，以及任务间的全部通信对象。
//
// 数据流（所有对象均为静态分配，无任何堆内存）：
//
//   sensor_task  --SensorSample-->  [sensor_q]   --> control_task
//   control_task --AppSnapshot-->   [snapshot_q] --> display_task
//   control_task --Thresholds--->   [threshold_q] -> storage_task
//   storage_task --SaveResult--->   [save_msg_q] --> display_task
//   storage_task --任务通知(保存完成)--> control_task
//
// sensor_q / snapshot_q / save_msg_q 都是长度 1 的覆写队列：消费者只关心
// 最新值，不关心积压，新值直接覆盖旧值。
// threshold_q 用于合并快速的阈值调整：连续改动只会保留最后一次，
// 存储任务在最后一次改动后静默 3 秒才真正写 Flash，避免频繁擦写。

#include "app_api.h"

#include "common/app_config.hpp"
#include "common/app_types.hpp"
#include "control/motor.hpp"
#include "display/oled_display.hpp"
#include "main.h"
#include "sensor/ntc_sensor.hpp"
#include "storage/settings_store.hpp"

#include <cstdint>
#include <cstring>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

namespace {

namespace cfg = app::cfg;

/* ------------------------------------------------------------------ */
/* 任务优先级。优先级体现的是"紧迫性/安全影响"，不是"功能重要性"：          */
/*   control  最高 —— 负责报警、风扇等安全动作和按键响应延迟；              */
/*   sensor   次之 —— 固定 100ms 采样周期，需稳定不被显示拖慢；            */
/*   display  较低 —— 刷屏慢一点可以接受；                                 */
/*   storage  最低 —— 后台写 Flash，慢且会阻塞取指，理应垫底。              */
/* ------------------------------------------------------------------ */

constexpr UBaseType_t kStoragePriority = tskIDLE_PRIORITY + 1;
constexpr UBaseType_t kDisplayPriority = tskIDLE_PRIORITY + 2;
constexpr UBaseType_t kSensorPriority = tskIDLE_PRIORITY + 3;
constexpr UBaseType_t kControlPriority = tskIDLE_PRIORITY + 4;

/* 栈大小（单位：字）。初值按调用深度 + 局部变量估算，属保守值；
 * 上板后应用 uxTaskGetStackHighWaterMark() 实测，再按需收紧。 */
constexpr std::uint32_t kSensorStackWords = 192;   /* logf + 中值排序，无文本格式化 */
constexpr std::uint32_t kControlStackWords = 192;  /* 仅 GPIO + 队列，无格式化 */
constexpr std::uint32_t kDisplayStackWords = 256;  /* 文本缓冲 + OLED 调用链 */
constexpr std::uint32_t kStorageStackWords = 128;  /* Flash 擦写调用链 */

/* ------------------------------------------------------------------ */
/* 静态 RTOS 对象：任务栈 / 任务控制块 / 队列存储，全部静态，不用堆          */
/* ------------------------------------------------------------------ */

StackType_t sensor_stack[kSensorStackWords];
StaticTask_t sensor_tcb;
StackType_t control_stack[kControlStackWords];
StaticTask_t control_tcb;
StackType_t display_stack[kDisplayStackWords];
StaticTask_t display_tcb;
StackType_t storage_stack[kStorageStackWords];
StaticTask_t storage_tcb;
StackType_t idle_stack[configMINIMAL_STACK_SIZE];
StaticTask_t idle_tcb;

QueueHandle_t sensor_q;
uint8_t sensor_q_storage[sizeof(app::SensorSample)];
StaticQueue_t sensor_q_tcb;

QueueHandle_t snapshot_q;
uint8_t snapshot_q_storage[sizeof(app::AppSnapshot)];
StaticQueue_t snapshot_q_tcb;

QueueHandle_t threshold_q;
uint8_t threshold_q_storage[sizeof(app::Thresholds)];
StaticQueue_t threshold_q_tcb;

QueueHandle_t save_msg_q;
uint8_t save_msg_q_storage[sizeof(app::SaveResult)];
StaticQueue_t save_msg_q_tcb;

TaskHandle_t control_task_handle = nullptr;

/* ------------------------------------------------------------------ */
/* 启动状态：app_init() 填充，任务启动时消费                              */
/* ------------------------------------------------------------------ */

// 开机时从 Flash 读到的阈值及其有效性；无效则控制任务用默认阈值。
app::TempSettings boot_settings{};
bool boot_settings_valid = false;

// 全局唯一的 OLED 实例，由显示任务独占（见 OledDisplay 头文件的线程安全说明）。
app::OledDisplay oled;

/* ------------------------------------------------------------------ */
/* 文本格式化辅助：有界的整数/定点小数转字符串，刻意不用 stdio，           */
/* 避免引入 printf 的重代码和潜在堆分配。                                  */
/* ------------------------------------------------------------------ */

// 温度文本缓冲大小（含符号、小数点和结尾 '\0'）。
constexpr std::size_t kTempTextSize = 10;

// 把无符号整数按十进制追加到 dest，返回新的写入位置。
char* append_unsigned(char* dest, int value) {
    char digits[4];
    int count = 0;
    if (value == 0) {
        digits[count++] = '0';
    }
    while (value > 0 && count < static_cast<int>(sizeof(digits))) {
        digits[count++] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    while (count > 0) {
        *dest++ = digits[--count];
    }
    return dest;
}

// 温度 -> "[-]NN.N" 一位小数字符串（四舍五入），写入定长缓冲。
void format_temperature(float value, char (&out)[kTempTextSize]) {
    const bool negative = value < 0.0f;
    if (negative) {
        value = -value;
    }
    const int scaled = static_cast<int>(value * 10.0f + 0.5f);

    char* p = out;
    if (negative) {
        *p++ = '-';
    }
    p = append_unsigned(p, scaled / 10);
    *p++ = '.';
    *p++ = static_cast<char>('0' + scaled % 10);
    *p = '\0';
}

// 风扇速度 -> 右对齐的 3 字符字符串（如 "  0"、" 55"、"100"）。
void format_fan_speed(std::uint8_t speed, char (&out)[4]) {
    out[0] = (speed >= 100) ? static_cast<char>('0' + speed / 100) : ' ';
    out[1] = (speed >= 10) ? static_cast<char>('0' + (speed / 10) % 10) : ' ';
    out[2] = static_cast<char>('0' + speed % 10);
    out[3] = '\0';
}

/* ------------------------------------------------------------------ */
/* 采样任务：周期性推进滤波链，把最新温度发布到覆写队列                      */
/* ------------------------------------------------------------------ */

void sensor_task(void*) {
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        // vTaskDelayUntil 保证固定 100ms 周期（相对延时会累积漂移）。
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(cfg::kTempSamplePeriodMs));

        app::ntc_sensor().update();
        const app::SensorSample sample{app::ntc_sensor().stable_temperature_c()};
        // 覆写发布：控制任务只取最新值，采样快于消费也不会堆积。
        xQueueOverwrite(sensor_q, &sample);
    }
}

/* ------------------------------------------------------------------ */
/* 控制任务：按键状态机、报警/蜂鸣器、风扇调速——整机的"大脑"               */
/* ------------------------------------------------------------------ */

// 控制任务的全部私有状态（只有控制任务读写，天然无竞争）。
struct ControlState {
    app::AppMode mode = app::AppMode::display;  // 当前界面状态
    float threshold_high = 35.0f;               // 高温报警阈值 A
    float threshold_low = 20.0f;                // 低温报警阈值 B
    float temperature_c = 25.0f;                // 最近一次采样温度
    std::uint8_t fan_speed = 0;                 // 当前风扇占空比
    bool fan_mode_enabled = false;              // 温控（自动风扇）开关
    bool save_pending = false;                  // 阈值已改、等待写盘
    std::uint32_t save_request_time = 0;

    // 按键边沿检测与防抖所需的上次状态/时间戳。
    GPIO_PinState mode_button_last = GPIO_PIN_RESET;
    std::uint32_t mode_button_last_tick = 0;
    GPIO_PinState temp_up_last = GPIO_PIN_SET;
    GPIO_PinState temp_down_last = GPIO_PIN_SET;
    GPIO_PinState fan_mode_button_last = GPIO_PIN_RESET;
    std::uint32_t fan_mode_button_tick = 0;
};

// 阈值被改动时调用：置脏标志，并把最新阈值覆写进存储队列。
// 覆写语义保证快速连按只保留最终值，存储任务只会写一次盘。
void mark_settings_dirty(ControlState& s, std::uint32_t now) {
    s.save_pending = true;
    s.save_request_time = now;

    const app::Thresholds thresholds{s.threshold_high, s.threshold_low};
    xQueueOverwrite(threshold_q, &thresholds);
}

// 模式键（板上外部下拉，按下=高电平）：检测上升沿并防抖，循环切换界面。
// 若切换前处于设置界面，先把当前阈值标记为待保存。
void handle_mode_button(ControlState& s, std::uint32_t now) {
    const GPIO_PinState current = HAL_GPIO_ReadPin(MODE_SWITCH_GPIO_Port, MODE_SWITCH_Pin);

    // 上升沿（松开 -> 按下）。
    if (s.mode_button_last == GPIO_PIN_RESET && current == GPIO_PIN_SET) {
        if (now - s.mode_button_last_tick >= cfg::kModeDebounceMs) {
            s.mode_button_last_tick = now;

            if (s.mode == app::AppMode::set_high || s.mode == app::AppMode::set_low) {
                mark_settings_dirty(s, now);
            }

            // 界面循环：主显示 -> 设高温 -> 设低温 -> 主显示。
            switch (s.mode) {
                case app::AppMode::display:
                    s.mode = app::AppMode::set_high;
                    break;
                case app::AppMode::set_high:
                    s.mode = app::AppMode::set_low;
                    break;
                case app::AppMode::set_low:
                    s.mode = app::AppMode::display;
                    break;
            }
        }
    }
    s.mode_button_last = current;
}

// 加/减温键（下降沿触发）：只在设置界面生效，按步进调整当前阈值。
// 两条约束：阈值不越界(0-80)，且高阈值始终比低阈值至少大一个步进，
// 防止两个阈值交叉导致报警区间消失。
void handle_threshold_buttons(ControlState& s, std::uint32_t now) {
    if (s.mode != app::AppMode::set_high && s.mode != app::AppMode::set_low) {
        return;
    }

    const GPIO_PinState temp_up = HAL_GPIO_ReadPin(TEMP_UP_GPIO_Port, TEMP_UP_Pin);
    if (s.temp_up_last == GPIO_PIN_SET && temp_up == GPIO_PIN_RESET) {
        mark_settings_dirty(s, now);
        if (s.mode == app::AppMode::set_high) {
            s.threshold_high += cfg::kThresholdStep;
            if (s.threshold_high > cfg::kThresholdMax) {
                s.threshold_high = cfg::kThresholdMax;
            }
            if (s.threshold_high <= s.threshold_low) {
                s.threshold_high = s.threshold_low + cfg::kThresholdStep;
            }
        } else {
            s.threshold_low += cfg::kThresholdStep;
            if (s.threshold_low > cfg::kThresholdMax) {
                s.threshold_low = cfg::kThresholdMax;
            }
            if (s.threshold_low >= s.threshold_high) {
                s.threshold_low = s.threshold_high - cfg::kThresholdStep;
            }
        }
    }
    s.temp_up_last = temp_up;

    const GPIO_PinState temp_down = HAL_GPIO_ReadPin(TEMP_DOWN_GPIO_Port, TEMP_DOWN_Pin);
    if (s.temp_down_last == GPIO_PIN_SET && temp_down == GPIO_PIN_RESET) {
        mark_settings_dirty(s, now);
        if (s.mode == app::AppMode::set_high) {
            s.threshold_high -= cfg::kThresholdStep;
            if (s.threshold_high < s.threshold_low + cfg::kThresholdStep) {
                s.threshold_high = s.threshold_low + cfg::kThresholdStep;
            }
            if (s.threshold_high < cfg::kThresholdMin) {
                s.threshold_high = cfg::kThresholdMin;
            }
        } else {
            s.threshold_low -= cfg::kThresholdStep;
            if (s.threshold_low > s.threshold_high - cfg::kThresholdStep) {
                s.threshold_low = s.threshold_high - cfg::kThresholdStep;
            }
            if (s.threshold_low < cfg::kThresholdMin) {
                s.threshold_low = cfg::kThresholdMin;
            }
        }
    }
    s.temp_down_last = temp_down;
}

// 温控开关键（PC13 内部上拉，按下=低电平）：下降沿切换风扇自动模式，
// 两次触发之间加冷却时间，避免一次按键被当成多次。
void handle_fan_mode_button(ControlState& s) {
    const GPIO_PinState current = HAL_GPIO_ReadPin(THERMO_MODE_GPIO_Port, THERMO_MODE_Pin);
    if (s.fan_mode_button_last == GPIO_PIN_SET && current == GPIO_PIN_RESET) {
        if (HAL_GetTick() - s.fan_mode_button_tick > cfg::kFanModeCooldownMs) {
            s.fan_mode_button_tick = HAL_GetTick();
            s.fan_mode_enabled = !s.fan_mode_enabled;
        }
    }
    s.fan_mode_button_last = current;
}

// 报警判断 + 双色 LED + 蜂鸣器。
// 注意两个 LED 是低电平点亮（写 RESET 才亮），这是硬件接法决定的。
//   超高温：红灯亮；超低温：绿灯亮；正常：两灯都灭。
// 报警时蜂鸣器按 1s 周期、前半周期鸣响（间歇报警）。
// 返回蜂鸣器当前是否鸣响（调用方暂未使用，保留语义）。
bool update_alarm_and_buzzer(const ControlState& s) {
    bool alarming = false;
    if (s.temperature_c > s.threshold_high) {
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
        alarming = true;
    } else if (s.temperature_c < s.threshold_low) {
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_RESET);
        alarming = true;
    } else {
        HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);
    }

    const bool buzzer_on =
        alarming && (HAL_GetTick() % cfg::kBuzzerBlinkPeriodMs > cfg::kBuzzerOnTimeMs);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, buzzer_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return buzzer_on;
}

// 风扇调速：温控开启时按温度线性映射占空比（区间外钳到最小/最大），
// 温控关闭时停转。速度的物理效果由 motor() 驱动实现。
void update_fan(ControlState& s) {
    if (s.fan_mode_enabled) {
        std::uint8_t speed = 0;
        if (s.temperature_c < cfg::kFanRampStartC) {
            speed = cfg::kFanMinPercent;
        } else if (s.temperature_c > cfg::kFanRampEndC) {
            speed = cfg::kFanMaxPercent;
        } else {
            const float ramp = (s.temperature_c - cfg::kFanRampStartC) *
                               static_cast<float>(cfg::kFanMaxPercent - cfg::kFanMinPercent) /
                               (cfg::kFanRampEndC - cfg::kFanRampStartC);
            speed = static_cast<std::uint8_t>(cfg::kFanMinPercent + static_cast<std::uint8_t>(ramp));
        }
        s.fan_speed = speed;
        app::motor().set_speed(speed);
    } else {
        s.fan_speed = 0;
        app::motor().stop();
    }
}

// 控制任务主循环。开机先等待启动门控时间，期间不响应按键、不动作报警，
// 对齐旧固件"启动序列走完才进入主循环"的行为。
void control_task(void*) {
    ControlState s;
    if (boot_settings_valid) {
        s.threshold_high = boot_settings.high;
        s.threshold_low = boot_settings.low;
    }

    vTaskDelay(pdMS_TO_TICKS(cfg::kStartupGateMs));

    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(cfg::kControlPeriodMs));
        const std::uint32_t now = HAL_GetTick();

        // 有新采样就更新当前温度；没有就沿用上一次的值。
        app::SensorSample sample;
        if (xQueueReceive(sensor_q, &sample, 0) == pdTRUE) {
            s.temperature_c = sample.temperature_c;
        }

        handle_mode_button(s, now);
        handle_threshold_buttons(s, now);
        handle_fan_mode_button(s);
        update_alarm_and_buzzer(s);
        update_fan(s);

        // 存储任务写盘完成后会发通知，这里清掉"待保存"标志，
        // 屏幕上的 '*' 未保存提示随之消失。
        if (s.save_pending && xTaskNotifyWait(0, UINT32_MAX, nullptr, 0) == pdTRUE) {
            s.save_pending = false;
        }

        // 把当前整机状态打包发布给显示任务（覆写，只留最新）。
        const app::AppSnapshot snapshot{s.mode,
                                        s.temperature_c,
                                        s.threshold_high,
                                        s.threshold_low,
                                        s.fan_speed,
                                        s.fan_mode_enabled,
                                        s.save_pending};
        xQueueOverwrite(snapshot_q, &snapshot);
    }
}

/* ------------------------------------------------------------------ */
/* 显示任务：独占 OLED，负责开机画面和主界面刷新                            */
/* ------------------------------------------------------------------ */

// 增量刷新缓存：记住上一次画过的内容，只重绘变化的字段，减少 I2C 流量。
struct DisplayCache {
    app::AppMode last_mode = app::AppMode::display;  // 上次的界面（变了就整屏重绘）
    bool first_draw = true;                          // 进入新界面后先画一次静态框架
    char last_temp[kTempTextSize] = "";              // 上次显示的温度文本
    char last_temp_a[kTempTextSize] = "";            // 上次显示的阈值 A 文本
    char last_temp_b[kTempTextSize] = "";            // 上次显示的阈值 B 文本
    std::uint8_t last_fan_speed = 0xFF;              // 0xFF 表示"还没画过"
    std::uint8_t last_fan_mode = 0xFF;
};

// 开机画面：标题 -> 初始化进度点 -> 设置读取结果提示。
// 用 vTaskDelay 而非 HAL_Delay，延时期间让出 CPU 给其他任务。
void run_splash() {
    oled.init(true);  // 本板 OLED 安装方向需要翻转 180 度
    oled.show_text(10, 1, "SMART THERMOMETER");
    vTaskDelay(pdMS_TO_TICKS(1500));

    oled.show_text(5, 1, "INITIALIZING");
    for (int i = 0; i < 3; ++i) {
        vTaskDelay(pdMS_TO_TICKS(300));
        oled.show_text(static_cast<std::uint8_t>(20 + i * 10), 3, ".");
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    oled.clear();

    oled.show_text(10, 2, boot_settings_valid ? "LOAD SETTINGS OK" : "USE DEFAULT SET");
    vTaskDelay(pdMS_TO_TICKS(1000));
    oled.clear();
}

// 按当前快照渲染一帧。采用增量刷新：每个字段只在"与上次不同"时才重画，
// 避免每 500ms 整屏闪烁。界面切换时清屏并重画静态框架一次。
//   主显示界面：温度 / 模式 / 风速 / 阈值 A、B / 保存状态指示
//   设置界面  ：标题 + 正在调整的阈值 + 可调范围提示
void render(const app::AppSnapshot& snap, DisplayCache& c) {
    char temp_buf[kTempTextSize];
    char temp_a_buf[kTempTextSize];
    char temp_b_buf[kTempTextSize];
    format_temperature(snap.temperature_c, temp_buf);
    format_temperature(snap.threshold_high, temp_a_buf);
    format_temperature(snap.threshold_low, temp_b_buf);

    if (snap.mode != c.last_mode) {
        oled.clear();
        c.first_draw = true;
        c.last_mode = snap.mode;
    }

    if (snap.mode == app::AppMode::display) {
        if (c.first_draw) {
            c.first_draw = false;
            oled.show_text(0, 0, "Temp:       C");
            oled.show_text(0, 2, "Mode:          %");
            oled.show_text(0, 4, "A:    B:    ");
            oled.show_text(0, 6, "RUNNING...");

            c.last_temp[0] = '\0';
            c.last_temp_a[0] = '\0';
            c.last_temp_b[0] = '\0';
            c.last_fan_mode = 0xFF;
            c.last_fan_speed = 0xFF;
        }

        if (std::strcmp(temp_buf, c.last_temp) != 0) {
            oled.fill_area(30, 0, 40, 1, 0x00);
            oled.show_text(30, 0, temp_buf);
            std::strcpy(c.last_temp, temp_buf);
        }

        const std::uint8_t fan_mode_flag = snap.fan_mode_enabled ? 1 : 0;
        if (fan_mode_flag != c.last_fan_mode) {
            oled.fill_area(30, 2, 20, 1, 0x00);
            oled.show_text(30, 2, snap.fan_mode_enabled ? "ON " : "OFF");
            c.last_fan_mode = fan_mode_flag;
        }

        if (snap.fan_speed != c.last_fan_speed) {
            char speed_str[4];
            format_fan_speed(snap.fan_speed, speed_str);
            oled.fill_area(60, 2, 20, 1, 0x00);
            oled.show_text(60, 2, speed_str);
            c.last_fan_speed = snap.fan_speed;
        }

        if (std::strcmp(temp_a_buf, c.last_temp_a) != 0) {
            oled.fill_area(10, 4, 25, 1, 0x00);
            oled.show_text(10, 4, temp_a_buf);
            std::strcpy(c.last_temp_a, temp_a_buf);
        }

        if (std::strcmp(temp_b_buf, c.last_temp_b) != 0) {
            oled.fill_area(45, 4, 25, 1, 0x00);
            oled.show_text(45, 4, temp_b_buf);
            std::strcpy(c.last_temp_b, temp_b_buf);
        }

        if (snap.save_pending) {
            oled.show_text(90, 6, "*");
        } else {
            oled.fill_area(90, 6, 10, 1, 0x00);
        }
    } else if (snap.mode == app::AppMode::set_high) {
        if (c.first_draw) {
            oled.clear();
            oled.show_text(10, 0, "Set HIGH Temp");
            c.first_draw = false;
        }
        oled.show_text(30, 2, temp_a_buf);
        oled.show_text(5, 5, "Range: 0-80 C");
    } else {
        if (c.first_draw) {
            oled.clear();
            oled.show_text(10, 0, "Set LOW Temp");
            c.first_draw = false;
        }
        oled.show_text(30, 2, temp_b_buf);
        oled.show_text(5, 5, "Range: 0-80 C");
    }
}

// 显示任务主循环：先放开机画面，然后以 100ms 节拍巡检：
//  1) 有保存结果就显示 "SAVED!"/"SAVE FAIL!"，保留 2 秒后擦除；
//  2) 非阻塞取最新快照（取不到就沿用上一份）；
//  3) 距离上次刷新满 500ms 才真正重绘，控制刷屏频率。
void display_task(void*) {
    run_splash();

    DisplayCache cache;
    app::AppSnapshot snap;

    bool message_active = false;       // 保存提示是否正在显示
    TickType_t message_deadline = 0;   // 提示到期的节拍
    TickType_t last_refresh = xTaskGetTickCount();
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(cfg::kDisplayCheckPeriodMs));
        const TickType_t now = xTaskGetTickCount();

        // 优先处理保存结果提示（新消息覆盖旧消息）。
        app::SaveResult result;
        if (xQueueReceive(save_msg_q, &result, 0) == pdTRUE) {
            oled.show_text(20, 6, result == app::SaveResult::saved ? "SAVED!" : "SAVE FAIL!");
            message_active = true;
            message_deadline = now + pdMS_TO_TICKS(cfg::kSaveMessageMs);
        } else if (message_active &&
                   static_cast<std::int32_t>(now - message_deadline) >= 0) {
            oled.fill_area(20, 6, 60, 1, 0x00);  // 到期擦除提示
            message_active = false;
        }

        // 非阻塞取最新快照；控制任务还没发布新值时沿用上一份。
        (void)xQueueReceive(snapshot_q, &snap, 0);

        if (now - last_refresh >= pdMS_TO_TICKS(cfg::kOledRefreshPeriodMs)) {
            last_refresh = now;
            render(snap, cache);
        }
    }
}

/* ------------------------------------------------------------------ */
/* 存储任务：防抖后把阈值写入 Flash 最后一页                               */
/* ------------------------------------------------------------------ */

// 优先级最低：Flash 擦写耗时长（几十毫秒）且期间阻塞取指，只适合后台做。
void storage_task(void*) {
    const app::SettingsStore store;

    for (;;) {
        // 阻塞等待第一次阈值变更。
        app::Thresholds thresholds;
        xQueueReceive(threshold_q, &thresholds, portMAX_DELAY);

        // 防抖：之后每收到一次新变更就刷新"最后值"并重新计时，
        // 直到连续 kSaveDebounceMs 没有新变更，才用最新值写一次盘。
        // 这样快速连按加/减键只会触发一次 Flash 擦写，保护寿命。
        app::Thresholds next;
        while (xQueueReceive(threshold_q, &next,
                             pdMS_TO_TICKS(cfg::kSaveDebounceMs)) == pdTRUE) {
            thresholds = next;
        }

        app::TempSettings settings{};
        settings.high = thresholds.high;
        settings.low = thresholds.low;
        const app::SaveResult result = (store.write(settings) == app::StoreStatus::ok)
                                           ? app::SaveResult::saved
                                           : app::SaveResult::failed;

        // 结果分两路：告诉显示任务在屏幕上提示，通知控制任务清脏标志。
        xQueueOverwrite(save_msg_q, &result);
        xTaskNotify(control_task_handle, 0, eNoAction);
    }
}

}  // namespace

/* ------------------------------------------------------------------ */
/* C 语言入口（供 main.c 调用，见 app_api.h）                              */
/* ------------------------------------------------------------------ */

// 调度器启动前的一次性初始化：读设置、启动采样和风扇驱动。
// 注意：读 Flash 设置放在这里同步做，保证控制任务启动时阈值已就绪。
extern "C" void app_init(void) {
    const app::SettingsStore store;
    boot_settings_valid = store.read(boot_settings) == app::StoreStatus::ok;

    app::ntc_sensor().start();
    app::motor().init();
}

// 创建所有队列和任务，然后启动调度器（正常不会返回）。
// 全部用 xQueueCreateStatic / xTaskCreateStatic 静态创建，不依赖堆；
// 任一对象创建失败都直接 configASSERT，避免带病运行。
extern "C" void app_start(void) {
    sensor_q = xQueueCreateStatic(1, sizeof(app::SensorSample), sensor_q_storage,
                                  &sensor_q_tcb);
    snapshot_q = xQueueCreateStatic(1, sizeof(app::AppSnapshot), snapshot_q_storage,
                                    &snapshot_q_tcb);
    threshold_q = xQueueCreateStatic(1, sizeof(app::Thresholds), threshold_q_storage,
                                     &threshold_q_tcb);
    save_msg_q = xQueueCreateStatic(1, sizeof(app::SaveResult), save_msg_q_storage,
                                    &save_msg_q_tcb);
    configASSERT(sensor_q && snapshot_q && threshold_q && save_msg_q);

    configASSERT(xTaskCreateStatic(sensor_task, "sensor", kSensorStackWords, nullptr,
                                   kSensorPriority, sensor_stack, &sensor_tcb) != nullptr);

    control_task_handle = xTaskCreateStatic(control_task, "control", kControlStackWords,
                                            nullptr, kControlPriority, control_stack,
                                            &control_tcb);
    configASSERT(control_task_handle != nullptr);

    configASSERT(xTaskCreateStatic(display_task, "display", kDisplayStackWords, nullptr,
                                   kDisplayPriority, display_stack, &display_tcb) != nullptr);

    configASSERT(xTaskCreateStatic(storage_task, "storage", kStorageStackWords, nullptr,
                                   kStoragePriority, storage_stack, &storage_tcb) != nullptr);

    vTaskStartScheduler();

    // 调度器正常不会返回；若返回说明出了严重问题，停在这里便于调试。
    configASSERT(false);
}

/* ------------------------------------------------------------------ */
/* 内核钩子：静态分配必需的回调 + 栈溢出/断言的现场保留                      */
/* ------------------------------------------------------------------ */

// 静态分配模式下，内核创建空闲任务时向这里要栈和控制块。
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** idle_task_tcb,
                                              StackType_t** idle_task_stack,
                                              uint32_t* idle_task_stack_size) {
    *idle_task_tcb = &idle_tcb;
    *idle_task_stack = idle_stack;
    *idle_task_stack_size = configMINIMAL_STACK_SIZE;
}

// 栈溢出钩子（configCHECK_FOR_STACK_OVERFLOW=2 时触发）。
// 关中断并停住，保留现场：用调试器看 task/name 即可定位溢出的任务，
// 再据此调整该任务的栈大小（见文件头部的栈大小说明）。
extern "C" void vApplicationStackOverflowHook(TaskHandle_t task, char* name) {
    (void)task;
    (void)name;
    portDISABLE_INTERRUPTS();
    for (;;) {
    }
}

// configASSERT 失败回调：同样关中断停住，便于调试器定位断言位置。
extern "C" void app_rtos_assert_failed(const char* file, unsigned long line) {
    (void)file;
    (void)line;
    portDISABLE_INTERRUPTS();
    for (;;) {
    }
}
