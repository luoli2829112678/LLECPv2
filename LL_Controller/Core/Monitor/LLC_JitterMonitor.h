#ifndef JITTER_MONITOR_H
#define JITTER_MONITOR_H

#include <cstdint>
#include <atomic>
#include <deque>
#include <string>

// 共享内存结构定义
struct JitterSharedMemory {
    std::atomic<double> Jitter_current;  // 当前抖动值(µs)
    std::atomic<double> Jitter_max;      // 最大抖动值(µs)
    std::atomic<double> Jitter_min;      // 最小抖动值(µs)
    std::atomic<double> Jitter_avg;      // 平均抖动值(µs)
    std::atomic<double> Jitter_status;   // 状态: 0-正常, 1-报警
};

class JitterMonitor {
public:
    // 构造函数，参数为机器人ID和期望周期(µs)
    JitterMonitor(double expected_cycle_us);
    
    // 析构函数
    ~JitterMonitor() = default;
    
    // 更新周期时间戳，计算抖动值
    void updateCycleTimestamp();
    
    // 检查是否触发报警
    bool checkAndTriggerAlarm();
    
    // 更新共享内存数据
    void updateSharedMemory(JitterSharedMemory* shm);
    
    // 设置单周期抖动阈值
    void setSingleCycleThreshold(double threshold);
    
    // 设置多周期抖动阈值
    void setMultiCycleThreshold(double threshold);
    
    // 设置连续周期抖动保护阈值
    void setContinuousCycleTolerance(int count);
    
    // 设置多周期内允许的抖动超限次数
    void setMultiCycleTolerance(int count);

private:
    // 更新统计信息
    void updateStatistics(double current_jitter);
    
    // 记录周期历史
    void recordCycleHistory(double current_jitter);
    
    // 触发报警
    void triggerAlarm(const std::string& reason);
    
    // 期望周期(µs)
    double expected_cycle_;
    
    // 上一次时间戳(µs)
    double last_timestamp_;
    
    // 当前抖动值(µs)
    double current_jitter_;
    
    // 最大抖动值(µs)
    double max_jitter_;
    
    // 最小抖动值(µs)
    double min_jitter_;
    
    // 平均抖动值(µs)
    double avg_jitter_;
    
    // 抖动总和，用于计算平均值
    double total_jitter_;
    
    // 周期计数
    uint64_t cycle_count_;
    
    // 阈值参数
    double single_cycle_threshold_;       // 单周期抖动阈值(µs)
    double multi_cycle_threshold_;        // 多周期抖动阈值(µs)
    int continuous_cycle_tolerance_;       // 连续周期容忍次数
    int multi_cycle_tolerance_;            // 多周期内允许超限次数
    
    // 周期历史记录
    std::deque<bool> cycle_exceed_history_;
    
    // 当前连续超限次数
    int current_continuous_exceed_count_;
    
    // 报警状态
    bool alarm_triggered_;
    
    // 获取当前时间戳(µs)
    double getCurrentTimestampUs();
};

#endif // JITTER_MONITOR_H
