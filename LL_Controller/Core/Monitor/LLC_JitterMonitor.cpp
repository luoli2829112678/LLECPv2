#include "LLC_JitterMonitor.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

JitterMonitor::JitterMonitor(double expected_cycle_us)
    : expected_cycle_(expected_cycle_us),
      last_timestamp_(0),
      current_jitter_(0),
      max_jitter_(0),
      min_jitter_(0),
      avg_jitter_(0),
      total_jitter_(0),
      cycle_count_(0),
      // 设置默认阈值
      single_cycle_threshold_(2500),    // 默认单周期阈值2500µs
      multi_cycle_threshold_(2000),     // 默认多周期阈值2000µs
      continuous_cycle_tolerance_(2),  // 默认连续2个周期
      multi_cycle_tolerance_(5),       // 默认10周期内允许5次
      current_continuous_exceed_count_(0),
      alarm_triggered_(false) 
{
}

double JitterMonitor::getCurrentTimestampUs() 
{
    // 获取当前微秒级时间戳
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 1000.0;
}

void JitterMonitor::updateCycleTimestamp() 
{
    double current_timestamp = getCurrentTimestampUs();
    
    if (last_timestamp_ != 0) 
    {
        // 计算实际周期
        double actual_cycle = current_timestamp - last_timestamp_;
        //printf ("actual_cycle: %.3fµs\n", actual_cycle);    
        // 计算抖动值（实际周期与期望周期之差）
        current_jitter_ = actual_cycle - expected_cycle_;
        
        // 取绝对值，只关注抖动幅度
        current_jitter_ = std::abs(current_jitter_);
        //printf ("current_jitter_: %.3fµs\n", current_jitter_);
        // 更新统计信息
        updateStatistics(current_jitter_);
        
        // 记录周期历史用于多周期判断
        recordCycleHistory(current_jitter_);
    }
    
    // 更新最后时间戳
    last_timestamp_ = current_timestamp;

}

void JitterMonitor::updateStatistics(double current_jitter) 
{
    cycle_count_++;
    total_jitter_ += current_jitter;
    
    // 更新最大最小值
    if (cycle_count_ == 1) 
    {
        max_jitter_ = current_jitter;
        min_jitter_ = current_jitter;
    } 
    else 
    {
        if (current_jitter > max_jitter_) 
        {
            max_jitter_ = current_jitter;
        }
        if (current_jitter < min_jitter_) 
        {
            min_jitter_ = current_jitter;
        }
    }

    // 计算平均抖动（浮点除法，保留小数）
    avg_jitter_ = total_jitter_ /cycle_count_;

    //输出抖动统计信息到标准输出
    auto now = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    /*std::cout << "[" << std::ctime(&time) << "]"
              << "Current: " << std::fixed << std::setprecision (3) << current_jitter << "µs, "
              << "Max: " << max_jitter_ << "µs, "
              << "Min: " << min_jitter_ << "µs, "
              << "Avg: " << avg_jitter_ << "µs, "
              << "Cycles: " << cycle_count_ << std::endl;*/
}

void JitterMonitor::recordCycleHistory(double current_jitter) 
{
    // 记录当前周期是否超出多周期阈值
    bool exceeded = current_jitter > multi_cycle_threshold_;
    cycle_exceed_history_.push_back(exceeded);
    
    // 保持历史记录最多10个周期
    if (cycle_exceed_history_.size() > 10) {
        cycle_exceed_history_.pop_front();
    }
    
    // 更新连续超限计数
    if (exceeded) 
    {
        current_continuous_exceed_count_++;
    } 
    else 
    {
        current_continuous_exceed_count_ = 0;
    }
}

bool JitterMonitor::checkAndTriggerAlarm() 
{
    bool new_alarm = false;
    std::string alarm_reason;

    // 条件1: 单个周期超过单周期阈值
    if (current_jitter_ > single_cycle_threshold_) 
    {
        //printf("Single cycle jitter exceeds threshold,%.3f,%.3f\n",current_jitter_,single_cycle_threshold_);
        alarm_reason = "Single cycle jitter exceeds threshold";
        new_alarm = true;
    }
    // 条件2: 连续超过多周期阈值
    else if (current_continuous_exceed_count_ >= continuous_cycle_tolerance_) 
    {
        printf("Continuous cycles exceed jitter threshold\n");
        alarm_reason = "Continuous cycles exceed jitter threshold";
        new_alarm = true;
    }
    // 条件3: 10个周期内超限次数超过允许值
    else if (cycle_exceed_history_.size() == 10) 
    {
        int exceed_count = std::count(cycle_exceed_history_.begin(), 
                                      cycle_exceed_history_.end(), true);
        if (exceed_count > multi_cycle_tolerance_) 
        {
            printf("Too many jitter exceedances in 10 cycles\n");
            alarm_reason = "Too many jitter exceedances in 10 cycles";
            new_alarm = true;
        }
    }
    
    // 如果触发报警
    if (new_alarm) 
    {
        //triggerAlarm(alarm_reason);
        alarm_triggered_ = true;
        // 新增：输出报警信息到标准输出
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        /*std::cout << "[" << std::ctime(&time) << "]"
                  << "alarm: " << alarm_reason << ", "
                  << "Current jitter: " << current_jitter_ << ", "
                  << "single_cycle_threshold_: " << single_cycle_threshold_ << ", "
                  << "multi_cycle_threshold_: " << multi_cycle_threshold_ << std::endl;*/
    }
    return new_alarm;
}

void JitterMonitor::updateSharedMemory(JitterSharedMemory* shm) 
{
    if (!shm) return;

    // 更新共享内存数据
    shm->Jitter_current = current_jitter_;
    shm->Jitter_max = max_jitter_;
    shm->Jitter_min = min_jitter_;
    shm->Jitter_avg = avg_jitter_;
    shm->Jitter_status = alarm_triggered_ ? 1 : 0;
}

void JitterMonitor::setSingleCycleThreshold(double threshold) 
{
    if (threshold < 0 || threshold > 3000) 
    {
        return; // 无效值，不更新
    }
    single_cycle_threshold_ = threshold;
}

void JitterMonitor::setMultiCycleThreshold(double threshold) 
{
    if (threshold < 0 || threshold > 3000) 
    {
        return; // 无效值，不更新
    }
    multi_cycle_threshold_ = threshold;
}

void JitterMonitor::setContinuousCycleTolerance(int count) 
{
    if (count < 0 || count > 10) 
    {
        return; // 无效值，不更新
    }
    
    continuous_cycle_tolerance_ = count;
}

void JitterMonitor::setMultiCycleTolerance(int count) 
{
    if (count < 0 || count > 10) 
    {
        return; // 无效值，不更新
    }
    multi_cycle_tolerance_ = count;
}
