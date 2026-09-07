#pragma once

/*
 * 全局配置常量：所有周期、超时、阈值、按键参数集中在这里，
 * 代码中不允许出现魔法数字。
 *
 * 这些数值 1:1 复现原超级循环版本的行为；修改前请先理解对应功能。
 */

#include <cstdint>

namespace app::cfg {

// ---- 周期 ----
// 温度采样：每 100ms 消费一次 DMA 缓冲并更新滤波器
inline constexpr std::uint32_t kTempSamplePeriodMs = 100;
// OLED 整屏刷新节流：最快 500ms 重绘一次
inline constexpr std::uint32_t kOledRefreshPeriodMs = 500;

// ---- 任务节奏 ----
// 控制任务：按键扫描 + 报警/风扇执行的节拍（比原 50ms 更快，响应更好）
inline constexpr std::uint32_t kControlPeriodMs = 10;
// 显示任务：检查消息/快照的节拍（实际重绘仍受 kOledRefreshPeriodMs 节流）
inline constexpr std::uint32_t kDisplayCheckPeriodMs = 100;

// 启动门控：调度器启动后这段时间内不处理按键、不动作报警。
// 时长对齐旧固件的阻塞式启动序列：开机画面 1500 + 进度点 3*300 + 500
// + 设置读取提示 1000 + 稳定等待 2000 + 面板初始化延时约 200，共约 6.1s。
inline constexpr std::uint32_t kStartupGateMs = 6100;

// ---- 设置持久化 ----
// 防抖：阈值最后一次变更后静默 3 秒才真正写 Flash（避免频繁擦写最后一页）
inline constexpr std::uint32_t kSaveDebounceMs = 3000;
// "SAVED!"/"SAVE FAIL!" 提示在屏幕上保留的时长
inline constexpr std::uint32_t kSaveMessageMs = 2000;

// ---- 按键 ----
// 模式键软件防抖（板上已有外部下拉，按下为高电平）
inline constexpr std::uint32_t kModeDebounceMs = 20;
// 温控开关键两次触发的最小间隔（PC13 内部上拉，按下为低电平）
inline constexpr std::uint32_t kFanModeCooldownMs = 200;

// ---- 报警蜂鸣器 ----
// 报警时蜂鸣器按 1s 周期、50% 占空比间歇鸣响
inline constexpr std::uint32_t kBuzzerBlinkPeriodMs = 1000;
inline constexpr std::uint32_t kBuzzerOnTimeMs = 500;

// ---- 温度阈值调整范围 ----
// 用户在设置界面可调的阈值范围与步进（摄氏度）
inline constexpr float kThresholdMin = 0.0f;
inline constexpr float kThresholdMax = 80.0f;
inline constexpr float kThresholdStep = 0.1f;

// ---- 风扇转速曲线 ----
// 温控开启时按温度线性调速：温度 <= kFanRampStartC 输出 kFanMinPercent，
// >= kFanRampEndC 输出 kFanMaxPercent，中间线性插值。
inline constexpr float kFanRampStartC = 25.0f;
inline constexpr float kFanRampEndC = 50.0f;
inline constexpr std::uint8_t kFanMinPercent = 10;
inline constexpr std::uint8_t kFanMaxPercent = 100;

}  // namespace app::cfg
