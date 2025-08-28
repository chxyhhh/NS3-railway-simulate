#include "handover_predictor.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/mobility-model.h"

#include <vector>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("HandoverPredictor");
NS_OBJECT_ENSURE_REGISTERED(HandoverPredictor);

TypeId
HandoverPredictor::GetTypeId()
{
    static TypeId tid = TypeId("ns3::HandoverPredictor")
                            .SetParent<Object>()
                            .SetGroupName("Lte")
                            .AddConstructor<HandoverPredictor>()
                            .AddAttribute("PredictionWindow",
                                          "Time window for prediction",
                                          TimeValue(Seconds(2.0)),
                                          MakeTimeAccessor(&HandoverPredictor::m_predictionWindow),
                                          MakeTimeChecker())
                            .AddAttribute("RsrpThreshold",
                                          "RSRP threshold for handover prediction (dBm)",
                                          DoubleValue(-88.0),
                                          MakeDoubleAccessor(&HandoverPredictor::m_rsrpThreshold),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("VelocityThreshold",
                                          "Velocity threshold for handover prediction (m/s)",
                                          DoubleValue(10.0),
                                          MakeDoubleAccessor(&HandoverPredictor::m_velocityThreshold),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("ConfidenceThreshold",
                                          "Confidence threshold for prediction",
                                          DoubleValue(0.7),
                                          MakeDoubleAccessor(&HandoverPredictor::m_confidenceThreshold),
                                          MakeDoubleChecker<double>());
    return tid;
}

HandoverPredictor::HandoverPredictor()
    : m_algorithm(HYBRID),
      m_predictionWindow(Seconds(2.0)),
      m_rsrpThreshold(-100.0),
      m_velocityThreshold(10.0),
      m_confidenceThreshold(0.7),
      m_maxHistorySize(50)
{
    NS_LOG_FUNCTION(this);
}

HandoverPredictor::~HandoverPredictor()
{
    NS_LOG_FUNCTION(this);
}

void
HandoverPredictor::SetPredictionAlgorithm(PredictionAlgorithm algorithm)
{
    NS_LOG_FUNCTION(this << algorithm);
    m_algorithm = algorithm;
}

void
HandoverPredictor::SetPredictionWindow(Time window)
{
    NS_LOG_FUNCTION(this << window);
    m_predictionWindow = window;
}

void
HandoverPredictor::SetRsrpThreshold(double threshold)
{
    NS_LOG_FUNCTION(this << threshold);
    m_rsrpThreshold = threshold;
}

void
HandoverPredictor::SetVelocityThreshold(double threshold)
{
    NS_LOG_FUNCTION(this << threshold);
    m_velocityThreshold = threshold;
}

void
HandoverPredictor::SetConfidenceThreshold(double threshold)
{
    NS_LOG_FUNCTION(this << threshold);
    m_confidenceThreshold = threshold;
}

void
HandoverPredictor::AddMeasurement(uint64_t imsi, const MeasurementData& data)
{
    NS_LOG_FUNCTION(this << imsi);

    auto& history = m_measurementHistory[imsi];
    history.push_back(data);

    // 限制历史数据大小
    if (history.size() > m_maxHistorySize)
    {
        history.pop_front();
    }

    NS_LOG_DEBUG("Added measurement for IMSI " << imsi
                 << " RSRP: " << data.rsrp
                 << " Position: " << data.position);
}

HandoverPredictor::PredictionResult
HandoverPredictor::PredictHandover(uint64_t imsi)
{
    NS_LOG_FUNCTION(this << imsi);

    switch (m_algorithm)
    {
    case RSRP_TREND_BASED:
        return PredictByRsrpTrend(imsi);
    case VELOCITY_AWARE:
        return PredictByVelocity(imsi);
    case MACHINE_LEARNING:
        return PredictByMachineLearning(imsi);
    case HYBRID:
        return PredictByHybrid(imsi);
    default:
        return PredictByMachineLearning(imsi);
    }
}

HandoverPredictor::PredictionResult
HandoverPredictor::PredictByRsrpTrend(uint64_t imsi)
{
    NS_LOG_FUNCTION(this << imsi);

    PredictionResult result;
    result.willHandover = false;
    result.confidence = 0.0;
    result.targetCellId = 0;
    result.predictedTime = Simulator::Now();
    result.reason = "RSRP trend analysis";

    auto it = m_measurementHistory.find(imsi);
    if (it == m_measurementHistory.end() || it->second.size() < 3)
    {
        result.reason += " - insufficient data";
        return result;
    }

    const auto& measurements = it->second;

    // 计算RSRP变化趋势
    double trend = CalculateRsrpTrend(measurements);
    double currentRsrp = measurements.back().rsrp;

    // 预测未来RSRP值
    double futureRsrp = currentRsrp + trend * m_predictionWindow.GetSeconds();

    // 检查是否会低于阈值 - 进一步放宽条件
//    if ((futureRsrp < m_rsrpThreshold && trend < -0.5) || // 预测会低于阈值
//        (currentRsrp < m_rsrpThreshold + 5.0 && trend < -0.1)) // 或当前接近阈值且有下降趋势
    if (futureRsrp < m_rsrpThreshold + 1.0 && trend < -0.1)
    {
        result.willHandover = true;
        // 基于RSRP方差计算置信度
        double rsrpVariance = CalculateRsrpVariance(measurements, 5); // 使用最近5个测量值
        double rsrpStdDev = std::sqrt(rsrpVariance);

        // 方差越小，置信度越高
        // 标准差在0-5dB范围内映射到0.2-1.0的置信度
        double varianceBasedConfidence = 0.2 + 0.8 * std::exp(-rsrpStdDev / 2.0);
        varianceBasedConfidence = std::min(1.0, std::max(0.2, varianceBasedConfidence));

        // 结合趋势强度和方差稳定性
        double trendStrength = std::min(1.0, std::abs(trend) / 2.0);
        result.confidence = 0.6 * varianceBasedConfidence + 0.4 * trendStrength;

         NS_LOG_DEBUG("RSRP方差: " << rsrpVariance
                  << " 标准差: " << rsrpStdDev
                  << " 方差置信度: " << varianceBasedConfidence
                  << " 趋势强度: " << trendStrength
                  << " 最终置信度: " << result.confidence);

        // 寻找最佳邻区
        double bestNeighborRsrp = -200.0;
        for (const auto& neighbor : measurements.back().neighborRsrp)
        {
            if (neighbor.second > bestNeighborRsrp)
            {
                bestNeighborRsrp = neighbor.second;
                result.targetCellId = neighbor.first;
            }
        }

        // 估算切换时间
        if (trend < 0)
        {
            double timeToThreshold = (currentRsrp - m_rsrpThreshold) / std::abs(trend);
            result.predictedTime = Simulator::Now() + Seconds(timeToThreshold);
        }
    }

    NS_LOG_DEBUG("RSRP trend prediction for IMSI " << imsi
                 << " trend: " << trend
                 << " future RSRP: " << futureRsrp
                 << " will handover: " << result.willHandover);

    return result;
}

HandoverPredictor::PredictionResult
HandoverPredictor::PredictByVelocity(uint64_t imsi)
{
    NS_LOG_FUNCTION(this << imsi);

    PredictionResult result;
    result.willHandover = false;
    result.confidence = 0.0;
    result.targetCellId = 0;
    result.predictedTime = Simulator::Now();
    result.reason = "Velocity-aware prediction";

    auto it = m_measurementHistory.find(imsi);
    if (it == m_measurementHistory.end() || it->second.size() < 2)
    {
        result.reason += " - insufficient data";
        return result;
    }

    const auto& measurements = it->second;
    const auto& current = measurements.back();

    // 计算当前速度
    Vector velocity = current.velocity;
    double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

    if (speed < m_velocityThreshold)
    {
        result.reason += " - low velocity";
        return result;
    }

    // 预测未来位置
    Vector futurePosition = PredictFuturePosition(measurements, m_predictionWindow);

    // 计算到当前服务小区的距离
    double distanceToCurrentCell = CalculateDistanceToCellBoundary(current, current.cellId);

    // 如果高速移动且接近小区边界
    if (speed > m_velocityThreshold && distanceToCurrentCell < 200.0) // 200米内
    {
        result.willHandover = true;
                result.confidence = std::min(1.0, speed / 50.0); // 速度越快，置信度越高

                // 根据移动方向选择目标小区
                double bestScore = -1000.0;
                for (const auto& neighbor : current.neighborRsrp)
                {
                    // 计算移动方向与邻区方向的匹配度
                    Vector cellPos = m_cellPositions[neighbor.first];
                    Vector direction = cellPos - current.position;
                    double directionMagnitude = std::sqrt(direction.x * direction.x + direction.y * direction.y);
                    if (directionMagnitude > 0) {
                        direction.x = direction.x / directionMagnitude;
                        direction.y = direction.y / directionMagnitude;
                        direction.z = direction.z / directionMagnitude;
                    }

                    Vector velocityDir;
                    if (speed > 0) {
                        velocityDir.x = velocity.x / speed;
                        velocityDir.y = velocity.y / speed;
                        velocityDir.z = velocity.z / speed;
                    }
                    double alignment = direction.x * velocityDir.x + direction.y * velocityDir.y;

                    double score = neighbor.second + alignment * 10.0; // RSRP + 方向匹配度
                    if (score > bestScore)
                    {
                        bestScore = score;
                        result.targetCellId = neighbor.first;
                    }
                }

        // 估算切换时间
        result.predictedTime = Simulator::Now() + Seconds(distanceToCurrentCell / speed);
    }

    NS_LOG_DEBUG("Velocity prediction for IMSI " << imsi
                 << " speed: " << speed
                 << " distance to cell boundary: " << distanceToCurrentCell
                 << " will handover: " << result.willHandover);

    return result;
}

HandoverPredictor::PredictionResult
HandoverPredictor::PredictByMachineLearning(uint64_t imsi)
{
    NS_LOG_FUNCTION(this << imsi);

    // 简化的机器学习预测（实际应用中可以使用更复杂的ML模型）
    PredictionResult result;
    result.willHandover = false;
    result.confidence = 0.0;
    result.targetCellId = 0;
    result.predictedTime = Simulator::Now();
    result.reason = "Machine learning prediction";

    auto it = m_measurementHistory.find(imsi);
    if (it == m_measurementHistory.end() || it->second.size() < 5)
    {
        result.reason += " - insufficient training data";
        return result;
    }

    const auto& measurements = it->second;

    // 特征提取
    double rsrpTrend = CalculateRsrpTrend(measurements);
    double signalDecayRate = CalculateSignalDecayRate(measurements);
    bool hasAnomaly = DetectSignalAnomaly(measurements);

    Vector velocity = measurements.back().velocity;
    double speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);

    // 简单的决策树模型
    double score = 0.0;

    // RSRP趋势权重
    if (rsrpTrend < -2.0) score += 0.3;
    else if (rsrpTrend < -1.0) score += 0.1;

    // 信号衰减率权重
    if (signalDecayRate > 1.5) score += 0.25;
    else if (signalDecayRate > 1.0) score += 0.1;

    // 速度权重
    if (speed > 30.0) score += 0.2; // 高速
    else if (speed > 15.0) score += 0.1; // 中速

    // 信号异常权重
    if (hasAnomaly) score += 0.15;

    // 当前RSRP权重
    double currentRsrp = measurements.back().rsrp;
    if (currentRsrp < -95.0) score += 0.1;

    result.confidence = std::min(1.0, score);

    if (score > m_confidenceThreshold)
    {
        result.willHandover = true;

        // 选择最佳目标小区
        double bestRsrp = -200.0;
        for (const auto& neighbor : measurements.back().neighborRsrp)
        {
            if (neighbor.second > bestRsrp)
            {
                bestRsrp = neighbor.second;
                result.targetCellId = neighbor.first;
            }
        }

        // 预测切换时间
        result.predictedTime = Simulator::Now() + Seconds(1.0 / score);
    }

    NS_LOG_DEBUG("ML prediction for IMSI " << imsi
                 << " score: " << score
                 << " confidence: " << result.confidence
                 << " will handover: " << result.willHandover);

    return result;
}

HandoverPredictor::PredictionResult
HandoverPredictor::PredictByHybrid(uint64_t imsi)
{
    NS_LOG_FUNCTION(this << imsi);

    // 综合多种预测方法
    PredictionResult rsrpResult = PredictByRsrpTrend(imsi);
    PredictionResult velocityResult = PredictByVelocity(imsi);
    PredictionResult mlResult = PredictByMachineLearning(imsi);

    PredictionResult result;
    result.willHandover = false;
    result.confidence = 0.0;
    result.targetCellId = 0;
    result.predictedTime = Simulator::Now();
    result.reason = "Hybrid prediction (RSRP+Velocity+ML)";

    // 加权投票
    double totalWeight = 0.0;
    double weightedConfidence = 0.0;

    if (rsrpResult.willHandover)
    {
        totalWeight += 0.4;
        weightedConfidence += 0.4 * rsrpResult.confidence;
    }

    if (velocityResult.willHandover)
    {
        totalWeight += 0.3;
        weightedConfidence += 0.3 * velocityResult.confidence;
    }

    if (mlResult.willHandover)
    {
        totalWeight += 0.3;
        weightedConfidence += 0.3 * mlResult.confidence;
    }

    if (totalWeight > 0.5) // 至少有一半的算法认为会切换
    {
        result.willHandover = true;
        result.confidence = weightedConfidence;

        // 选择置信度最高的算法的目标小区
        if (rsrpResult.confidence >= velocityResult.confidence &&
            rsrpResult.confidence >= mlResult.confidence)
        {
            result.targetCellId = rsrpResult.targetCellId;
            result.predictedTime = rsrpResult.predictedTime;
        }
        else if (velocityResult.confidence >= mlResult.confidence)
        {
            result.targetCellId = velocityResult.targetCellId;
            result.predictedTime = velocityResult.predictedTime;
        }
        else
        {
            result.targetCellId = mlResult.targetCellId;
            result.predictedTime = mlResult.predictedTime;
        }
    }

    NS_LOG_DEBUG("Hybrid prediction for IMSI " << imsi
                 << " total weight: " << totalWeight
                 << " confidence: " << result.confidence
                 << " will handover: " << result.willHandover);

    return result;
}

double
HandoverPredictor::CalculateRsrpTrend(const std::deque<MeasurementData>& measurements)
{
    if (measurements.size() < 2)
        return 0.0;

    // 使用线性回归计算趋势
    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;
    int n = measurements.size();

    for (int i = 0; i < n; ++i)
    {
        double x = i;
        double y = measurements[i].rsrp;
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }

    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    return slope;
}

double
HandoverPredictor::CalculateDistanceToCellBoundary(const MeasurementData& current, uint16_t cellId)
{
    // 简化计算：假设小区覆盖半径为500米
    auto it = m_cellPositions.find(cellId);
    if (it == m_cellPositions.end())
    {
        return 1000.0; // 默认距离
    }

    Vector cellPos = it->second;
    double distance = std::sqrt(std::pow(current.position.x - cellPos.x, 2) +
                               std::pow(current.position.y - cellPos.y, 2));

    double cellRadius = 500.0; // 默认小区半径
    auto coverageIt = m_cellCoverage.find(cellId);
    if (coverageIt != m_cellCoverage.end())
    {
        cellRadius = coverageIt->second;
    }

    return std::max(0.0, cellRadius - distance);
}

Vector
HandoverPredictor::PredictFuturePosition(const std::deque<MeasurementData>& measurements, Time futureTime)
{
    if (measurements.empty())
        return Vector(0, 0, 0);

    const auto& current = measurements.back();
    Vector futurePos = current.position;
    futurePos.x += current.velocity.x * futureTime.GetSeconds();
    futurePos.y += current.velocity.y * futureTime.GetSeconds();

    return futurePos;
}

double
HandoverPredictor::CalculateSignalDecayRate(const std::deque<MeasurementData>& measurements)
{
    if (measurements.size() < 3)
        return 0.0;

    // 计算最近几个测量值的衰减率
    double recentDecay = 0.0;
    int count = 0;

    for (size_t i = measurements.size() - 1; i > 0 && count < 3; --i, ++count)
    {
        double timeDiff = (measurements[i].timestamp - measurements[i-1].timestamp).GetSeconds();
        if (timeDiff > 0)
        {
            double rsrpDiff = measurements[i-1].rsrp - measurements[i].rsrp;
            recentDecay += rsrpDiff / timeDiff;
        }
    }

    return count > 0 ? recentDecay / count : 0.0;
}

bool
HandoverPredictor::DetectSignalAnomaly(const std::deque<MeasurementData>& measurements)
{
    if (measurements.size() < 5)
        return false;

    // 检测信号突然下降
    const auto& latest = measurements.back();
    const auto& previous = measurements[measurements.size() - 2];

    double rsrpDrop = previous.rsrp - latest.rsrp;

    // 如果RSRP突然下降超过5dB，认为是异常
    return rsrpDrop > 5.0;
}

double
HandoverPredictor::CalculateRsrpVariance(const std::deque<MeasurementData>& measurements, int windowSize)
{
    if (measurements.size() < 2)
        return 0.0;

    // 取最近windowSize个测量值，如果数据不够就用全部
    int actualSize = std::min(windowSize, (int)measurements.size());
    int startIndex = measurements.size() - actualSize;

    // 计算平均值
    double sum = 0.0;
    for (int i = startIndex; i < (int)measurements.size(); ++i)
    {
        sum += measurements[i].rsrp;
    }
    double mean = sum / actualSize;

    // 计算方差
    double variance = 0.0;
    for (int i = startIndex; i < (int)measurements.size(); ++i)
    {
        double diff = measurements[i].rsrp - mean;
        variance += diff * diff;
    }
    variance /= actualSize;

    return variance;
}

} // namespace ns3
