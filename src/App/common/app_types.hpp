#pragma once

/*
 * 任务间传递的数据类型。
 *
 * 设计原则：任务之间不共享可变全局变量，所有数据都通过队列/通知传递；
 * 这里集中定义这些消息结构，生产者和消费者都只依赖本头文件。
 */

#include <cstdint>

namespace app {

// 界面状态机：主显示 / 设置高温阈值 / 设置低温阈值（模式键循环切换）。
enum class AppMode : std::uint8_t { display, set_high, set_low };

// 采样任务 -> 控制任务（经长度 1 的覆写队列传递）。
// 覆写语义：消费者永远只关心最新温度，旧样本直接丢弃。
struct SensorSample {
    float temperature_c;  // 已滤波 + 已校准的温度，单位摄氏度
};

// 控制任务 -> 显示任务（经长度 1 的覆写队列传递）。
// 控制任务每 10ms 写入一次当前整机状态，显示任务取最新一份渲染。
struct AppSnapshot {
    AppMode mode = AppMode::display;  // 当前界面
    float temperature_c = 25.0f;      // 当前温度（摄氏度）
    float threshold_high = 35.0f;     // 高温报警阈值 A（摄氏度）
    float threshold_low = 20.0f;      // 低温报警阈值 B（摄氏度）
    std::uint8_t fan_speed = 0;       // 风扇占空比百分比 0-100
    bool fan_mode_enabled = false;    // 温控（自动风扇）开关
    bool save_pending = false;        // 阈值已修改、尚未写入 Flash
};

// 阈值变更消息：控制任务每次改阈值都会覆写进存储队列；
// 存储任务以"最后一次变更"为准做 3 秒防抖后写 Flash。
struct Thresholds {
    float high;
    float low;
};

// 存储任务 -> 显示任务（经长度 1 的覆写队列传递）：写 Flash 的结果，
// 显示任务据此在屏幕上提示 "SAVED!" 或 "SAVE FAIL!"。
enum class SaveResult : std::uint8_t { saved, failed };

}  // namespace app
