#include "ns3/core-module.h"
#include "ns3/handover_predictor.h"
#include <iostream>

using namespace ns3;
using namespace std;

int main() {
    // 创建预测器
    Ptr<HandoverPredictor> predictor = CreateObject<HandoverPredictor>();
    predictor->SetPredictionAlgorithm(HandoverPredictor::RSRP_TREND_BASED);
    predictor->SetRsrpThreshold(-88.0);
    predictor->SetConfidenceThreshold(0.1);
    
    cout << "==========测试HandoverPredictor==========" << endl;
    
    // 模拟测量数据
    uint64_t imsi = 1;
    
    // 添加几个测量数据点，模拟RSRP逐渐下降的情况
    for (int i = 0; i < 5; i++) {
        HandoverPredictor::MeasurementData data;
        data.timestamp = Seconds(i);
        data.rsrp = -80.0 - i * 2.0;  // RSRP逐渐下降
        data.rsrq = -10.0;
        data.position = Vector(i * 100, 0, 0);  // 位置移动
        data.velocity = Vector(83.3, 0, 0);     // 300km/h速度
        data.cellId = 1;
        
        // 添加邻区信息
        data.neighborRsrp[2] = data.rsrp - 5.0;
        data.neighborRsrp[3] = data.rsrp - 8.0;
        
        predictor->AddMeasurement(imsi, data);
        
        cout << "添加测量数据 " << i << ": RSRP=" << data.rsrp 
             << " dBm, 位置=(" << data.position.x << ", " << data.position.y << ")" << endl;
        
        // 每次添加数据后都进行预测
        HandoverPredictor::PredictionResult result = predictor->PredictHandover(imsi);
        cout << "  预测结果: " << (result.willHandover ? "会切换" : "不会切换")
             << " 置信度: " << result.confidence
             << " 原因: " << result.reason << endl;
    }
    
    return 0;
}