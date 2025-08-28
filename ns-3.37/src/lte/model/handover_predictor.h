#ifndef HANDOVER_PREDICTOR_H
#define HANDOVER_PREDICTOR_H

#include "ns3/core-module.h"
#include "ns3/lte-module.h"
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <cmath>

namespace ns3 {

/**
 * \brief 切换预测器类，实现多种预测算法
 */
class HandoverPredictor : public Object
{
public:
    /**
     * \brief 预测算法类型枚举
     */
    enum PredictionAlgorithm
    {
        RSRP_TREND_BASED,      // 基于RSRP趋势的预测
        VELOCITY_AWARE,        // 考虑速度的预测
        MACHINE_LEARNING,      // 机器学习预测
        HYBRID                 // 混合预测算法
    };

    /**
     * \brief 测量数据结构
     */
    struct MeasurementData
    {
        Time timestamp;
        double rsrp;           // dBm
        double rsrq;           // dB
        Vector position;       // UE位置
        Vector velocity;       // UE速度
        uint16_t cellId;       // 服务小区ID
        std::map<uint16_t, double> neighborRsrp; // 邻区RSRP
    };

    /**
     * \brief 预测结果结构
     */
    struct PredictionResult
    {
        bool willHandover;           // 是否会发生切换
        Time predictedTime;          // 预测切换时间
        uint16_t targetCellId;       // 目标小区ID
        double confidence;           // 预测置信度 [0,1]
        std::string reason;          // 预测原因
    };

    static TypeId GetTypeId();
    HandoverPredictor();
    virtual ~HandoverPredictor();

    /**
     * \brief 设置预测算法类型
     */
    void SetPredictionAlgorithm(PredictionAlgorithm algorithm);

    /**
     * \brief 添加测量数据
     */
    void AddMeasurement(uint64_t imsi, const MeasurementData& data);

//    /**
//         * \brief 设置小区位置信息
//         */
//        void SetCellPosition(uint16_t cellId, Vector position);
//
//        /**
//         * \brief 设置小区覆盖范围
//         */
//        void SetCellCoverage(uint16_t cellId, double coverage);

    /**
     * \brief 执行切换预测
     */
    PredictionResult PredictHandover(uint64_t imsi);

    /**
     * \brief 设置预测参数
     */
    void SetPredictionWindow(Time window);
    void SetRsrpThreshold(double threshold);
    void SetVelocityThreshold(double threshold);
    void SetConfidenceThreshold(double threshold);

private:
    /**
     * \brief 基于RSRP趋势的预测
     */
    PredictionResult PredictByRsrpTrend(uint64_t imsi);

    /**
     * \brief 考虑速度的预测
     */
    PredictionResult PredictByVelocity(uint64_t imsi);

    /**
     * \brief 机器学习预测
     */
    PredictionResult PredictByMachineLearning(uint64_t imsi);

    /**
     * \brief 混合预测算法
     */
    PredictionResult PredictByHybrid(uint64_t imsi);

    /**
     * \brief 计算RSRP变化趋势
     */
    double CalculateRsrpTrend(const std::deque<MeasurementData>& measurements);

    /**
     * \brief 计算到小区边界的距离
     */
    double CalculateDistanceToCellBoundary(const MeasurementData& current, uint16_t cellId);

    /**
     * \brief 预测UE未来位置
     */
    Vector PredictFuturePosition(const std::deque<MeasurementData>& measurements, Time futureTime);

    /**
     * \brief 计算信号质量衰减率
     */
    double CalculateSignalDecayRate(const std::deque<MeasurementData>& measurements);

    /**
     * \brief Calculate RSRP variance for recent measurements
     * \param measurements the measurement history
     * \param windowSize number of recent measurements to consider
     * \return the variance of RSRP values
     */
    double CalculateRsrpVariance(const std::deque<MeasurementData>& measurements, int windowSize = 5);

    /**
     * \brief 检测信号质量突变
     */
    bool DetectSignalAnomaly(const std::deque<MeasurementData>& measurements);

    // 成员变量
    PredictionAlgorithm m_algorithm;
    Time m_predictionWindow;
    double m_rsrpThreshold;
    double m_velocityThreshold;
    double m_confidenceThreshold;

    // 存储每个UE的测量历史
    std::map<uint64_t, std::deque<MeasurementData>> m_measurementHistory;

    // 预测窗口大小
    uint32_t m_maxHistorySize;

    // 小区信息
    std::map<uint16_t, Vector> m_cellPositions;
    std::map<uint16_t, double> m_cellCoverage;
};

} // namespace ns3

#endif /* HANDOVER_PREDICTOR_H */
