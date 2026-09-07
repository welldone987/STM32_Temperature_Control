#pragma once

#include <array>
#include <cstdint>

namespace app {

/*
 * NTC 温度采集。
 *
 * 信号链（与旧固件逐级一致）：
 *   ADC 连续 DMA 采样到环形缓冲(50 点)
 *     -> 求均值得到 ADC 码值
 *     -> 分压公式 + Steinhart-Hart 方程换算成"原始温度"
 *     -> 5 点中值滤波去尖峰
 *     -> 30 点滑动平均平滑
 *     -> 分段线性校准（见 ntc_sensor.cpp 中的校准表）
 *     -> 叠加校准偏移 = 最终温度
 *
 * 所有权：仅采样任务可以调用（RTOS 拆分后）。唯一例外是
 * recover_from_dma_error()，它运行在 ADC 错误中断上下文。
 */
class NtcSensor {
public:
    // 分段校准表的一个标定点：measured=滤波后的示值，actual=真实温度。
    struct CalibrationPoint {
        float measured;
        float actual;
    };

    static constexpr std::size_t kAdcBufferSize = 50;  // DMA 环形缓冲点数

    NtcSensor() = default;
    // 传感器代表一组硬件资源（ADC + DMA 缓冲），禁止拷贝。
    NtcSensor(const NtcSensor&) = delete;
    NtcSensor& operator=(const NtcSensor&) = delete;

    // 复位所有滤波器并启动循环 DMA 采样。只能在调度器启动前调用一次。
    void start();
    void stop();

    // 消费当前 DMA 缓冲内容、推进一步滤波链。
    // 内部按 kSampleIntervalMs 自行节流：调用更频繁时是廉价的空操作。
    void update();

    // 返回已滤波、已校准的温度（摄氏度）。
    float stable_temperature_c();

    // 全局校准偏移（叠加在校准表结果之上），默认 0。
    void set_calibration_offset(float offset);
    float calibration_offset() const { return calibration_offset_; }

    // 仅 ISR 上下文：ADC/DMA 出错后重启循环 DMA。由 HAL 错误回调调用，
    // 不在这里做延时——优先级 0 的中断里调用 HAL_Delay 会死等。
    void recover_from_dma_error();

private:
    static constexpr std::size_t kMedianSize = 5;          // 中值滤波窗口
    static constexpr std::size_t kMovingAverageSize = 30;  // 滑动平均窗口
    static constexpr std::uint32_t kSampleIntervalMs = 100;  // update() 节流间隔
    static constexpr float kAdcMaxValue = 4095.0f;         // 12 位 ADC 满量程
    static constexpr float kSeriesResistorOhm = 10000.0f;  // 分压电路串联电阻

    // 板上 NTC 的 Steinhart-Hart 系数（由器件手册/标定得到）。
    static constexpr float kCoefficientA = 1.12924e-3f;
    static constexpr float kCoefficientB = 2.34108e-4f;
    static constexpr float kCoefficientC = 8.77547e-8f;

    std::uint16_t average_adc_counts() const;
    float raw_temperature_c() const;
    static float piecewise_calibrate(float measured);

    float median_filter(float new_value);

    // 由 DMA1_Channel1 写入、update() 读取。volatile 保证外设持续写入时
    // 编译器不会把读取优化掉（这是 C++ 下表达"硬件共享内存"的方式）。
    volatile std::uint16_t dma_buffer_[kAdcBufferSize] = {};

    std::array<float, kMedianSize> median_buffer_{};
    std::uint8_t median_index_ = 0;

    std::array<float, kMovingAverageSize> moving_average_buffer_{};
    std::uint8_t moving_average_index_ = 0;
    std::uint8_t moving_average_count_ = 0;

    std::uint32_t last_sample_time_ = 0;
    float calibration_offset_ = 0.0f;
};

// 全局唯一的传感器实例。HAL ADC 错误回调也通过它访问（extern "C" 桥接）。
NtcSensor& ntc_sensor();

}  // namespace app
