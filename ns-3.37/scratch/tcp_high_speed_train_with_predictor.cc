#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include <ns3/point-to-point-helper.h>
#include <ns3/buildings-module.h>
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include <ns3/netanim-module.h>
#include <ns3/log.h>
#include "ns3/error-model.h"
#include "ns3/red-queue-disc.h"
#include "ns3/queue-size.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/tcp-cubic.h"
#include "ns3/tcp-rx-buffer.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/handover_predictor.h" // 引入切换预测器

#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <iomanip>
#include <ios>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("LteGridWithPredictor");

// 全局变量
std::string cwnd_tr_file_name = "cwndTr.txt";
std::string ssthresh_tr_file_name = "sshThTr.txt";
std::string rtt_tr_file_name = "rttTr.txt";
std::string rto_tr_file_name = "rtoTr.txt";
std::string rwnd_tr_file_name = "rwndTr.txt";
std::string advWND_tr_file_name = "advWNDTr.txt";
std::string hrack_tr_file_name = "hrackTr.txt";
std::string trFileDir = "./traces/";
std::string thrputStream_tr_file_name = "throughput.txt";
std::string basicTracePath = "/NodeList/3/$ns3::TcpL4Protocol/SocketList/";

bool firstCwnd = true;
bool firstSshThr = true;
bool firstRtt = true;
bool firstRto = true;
bool firstRwnd = true;
bool firstAdvWND = true;
bool firstHrack = true;

Ptr<OutputStreamWrapper> cWndStream;
Ptr<OutputStreamWrapper> ssThreshStream;
Ptr<OutputStreamWrapper> rttStream;
Ptr<OutputStreamWrapper> rtoStream;
Ptr<OutputStreamWrapper> thrputStream;
Ptr<OutputStreamWrapper> rwndStream;
Ptr<OutputStreamWrapper> advWNDStream;
Ptr<OutputStreamWrapper> hrackStream;

bool isHandoverStart = false;
uint32_t cWndValue;
uint32_t ssThreshValue;

ApplicationContainer sourceApps;
ApplicationContainer sinkApps;
Time *lastReportTime;
std::vector<double> lastRxMbits;

Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

// 全局切换预测器
Ptr<HandoverPredictor> g_handoverPredictor;
NodeContainer g_enbNodes;
NodeContainer g_ueNodes;

// 预测结果输出文件
std::ofstream predictionLogFile;

// 手动添加测量数据来测试预测器
//void AddTestMeasurementData() {
//    if (g_handoverPredictor && g_ueNodes.GetN() > 0) {
//        uint64_t imsi = 1;
//
//        // 获取UE当前位置和速度
//        Ptr<Node> ueNode = g_ueNodes.Get(0);
//        Ptr<MobilityModel> mobilityModel = ueNode->GetObject<MobilityModel>();
//
//        if (mobilityModel) {
//            Vector position = mobilityModel->GetPosition();
//            Vector velocity = Vector(0, 0, 0);
//
//            Ptr<ConstantVelocityMobilityModel> constVelModel = DynamicCast<ConstantVelocityMobilityModel>(mobilityModel);
//            if (constVelModel) {
//                velocity = constVelModel->GetVelocity();
//            }
//
//            // 创建模拟的测量数据
//            HandoverPredictor::MeasurementData measurementData;
//            measurementData.timestamp = Simulator::Now();
//            measurementData.position = position;
//            measurementData.velocity = velocity;
//            measurementData.cellId = 1;  // 假设当前服务小区ID为1
//
//            // 模拟RSRP随距离衰减
//            double distance = std::sqrt(position.x * position.x + position.y * position.y);
//            measurementData.rsrp = -70.0 - 20 * std::log10(distance / 100.0);  // 简单的路径损耗模型
//            measurementData.rsrq = -10.0;  // 固定RSRQ值
//
//            // 添加邻区RSRP（模拟）
//            measurementData.neighborRsrp[2] = measurementData.rsrp - 5.0;  // 邻区信号稍弱
//            measurementData.neighborRsrp[3] = measurementData.rsrp - 8.0;
//
//            // 添加测量数据到预测器
//            g_handoverPredictor->AddMeasurement(imsi, measurementData);
//
//            std::cout << "==========添加测试测量数据========== " << Simulator::Now().GetSeconds() << "s" << std::endl;
//            std::cout << "位置: (" << position.x << ", " << position.y << ") RSRP: " << measurementData.rsrp << " dBm" << std::endl;
//        }
//    }
//
//    // 每0.5秒添加一次测量数据
//    Simulator::Schedule(Seconds(0.5), &AddTestMeasurementData);
//}

// 定期检查预测器状态的函数
//void CheckPredictorStatus() {
//    if (g_handoverPredictor && g_ueNodes.GetN() > 0) {
//        // 获取UE的IMSI（通常是1）
//        uint64_t imsi = 1;
//
//        // 手动触发预测检查
//        HandoverPredictor::PredictionResult prediction = g_handoverPredictor->PredictHandover(imsi);
//
//        std::cout << "==========预测器状态检查========== " << Simulator::Now().GetSeconds() << "s" << std::endl;
//        std::cout << "IMSI=" << imsi << " 预测结果: " << (prediction.willHandover ? "会切换" : "不会切换")
//                  << " 置信度=" << prediction.confidence << std::endl;
//
//        if (prediction.willHandover) {
//            predictionLogFile << Simulator::Now().GetSeconds()
//                              << " MANUAL_CHECK IMSI=" << imsi
//                              << " WILL_HANDOVER=YES"
//                              << " CONFIDENCE=" << prediction.confidence
//                              << " TARGET_CELL=" << prediction.targetCellId
//                              << " REASON=" << prediction.reason << std::endl;
//        }
//    }

    // 每秒检查一次
//    Simulator::Schedule(Seconds(1.0), &CheckPredictorStatus);
//}

// 回调函数
void RxCallback(Ptr<const Packet> packet, const Address& from) {
    NS_LOG_INFO("Packet received with size: " << packet->GetSize()
                 << " from address: " << InetSocketAddress::ConvertFrom(from).GetIpv4());
}

void RxWithAddressesCallback(Ptr<const Packet> packet, const Address& from, const Address& to) {
    NS_LOG_INFO("Packet received with size: " << packet->GetSize()
                 << " from: " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                 << " to: " << InetSocketAddress::ConvertFrom(to).GetIpv4());
}

void RxWithSeqTsSizeCallback(Ptr<const Packet> packet, const Address& from, const Address& to, const SeqTsSizeHeader& header) {
    NS_LOG_INFO("Packet received with Seq: " << header.GetSeq()
                 << " Size: " << header.GetSize()
                 << " Timestamp: " << header.GetTs());
}

void attachinfuture(NetDeviceContainer &ues, int ueid)
{
    lteHelper->Attach(ues.Get(ueid));
    std::cout << "attach" << std::endl;
}

static void RxDrop(Ptr<const Packet> p)
{
    NS_LOG_UNCOND("RxDrop at " << Simulator::Now().GetSeconds());
}

void TraceTcpDupAck(std::string context, uint32_t seqNo, uint32_t ackNo)
{
    std::cout << "Dup ACK: " << "seqNo = " << seqNo << ", ackNo = " << ackNo
              << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
}

void TraceTcpRetransmission(std::string context, uint32_t seqNo, uint32_t ackNo)
{
    std::cout << "TCP Retransmission: " << "seqNo = " << seqNo << ", ackNo = " << ackNo
              << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
}

void MyCheckSocketFunction(Ptr<PacketSink> packetSink)
{
    Ptr<Socket> socket = packetSink->GetListeningSocket();
    if (socket)
    {
        Ptr<TcpSocket> tcpSocket = DynamicCast<TcpSocket>(socket);
        if (tcpSocket)
        {
            Ptr<TcpSocketBase> tcpSocketBase = DynamicCast<TcpSocketBase>(tcpSocket);
            if (tcpSocketBase)
            {
                if (isHandoverStart)
                {
                    tcpSocketBase->SetHandoverStatus(isHandoverStart);
                    Simulator::Schedule(MilliSeconds(100), [=]() {
                        isHandoverStart = false;
                    });
                }
            }
        }
        else
        {
            NS_LOG_ERROR("Socket is not a TCP socket.");
        }
    }
    else
    {
        std::cout << "m_socket error" << std::endl;
    }
    Simulator::Schedule(Seconds(0.001), &MyCheckSocketFunction, packetSink);
}

void NotifyConnectionEstablishedUe(std::string context,
                                   uint64_t imsi,
                                   uint16_t cellid,
                                   uint16_t rnti)
{
    std::cout << "at " << Simulator::Now().GetSeconds() << "s " << context
              << " UE IMSI " << imsi
              << ": connected to CellId " << cellid
              << " with RNTI " << rnti
              << std::endl;
}

void NotifyHandoverStartUe(std::string context,
                           uint64_t imsi,
                           uint16_t cellid,
                           uint16_t rnti,
                           uint16_t targetCellId)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context
              << " UE IMSI " << imsi
              << ": previously connected to CellId " << cellid
              << " with RNTI " << rnti
              << ", doing handover to CellId " << targetCellId
              << std::endl;

    // 记录实际切换事件
    predictionLogFile << Simulator::Now().GetSeconds()
                      << " ACTUAL_HANDOVER IMSI=" << imsi
                      << " FROM=" << cellid
                      << " TO=" << targetCellId << std::endl;
}

void NotifyHandoverEndOkUe(std::string context,
                           uint64_t imsi,
                           uint16_t cellid,
                           uint16_t rnti)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context
              << " UE IMSI " << imsi
              << ": successful handover to CellId " << cellid
              << " with RNTI " << rnti
              << std::endl;
}

void NotifyConnectionEstablishedEnb(std::string context,
                                    uint64_t imsi,
                                    uint16_t cellid,
                                    uint16_t rnti)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context
              << " eNB CellId " << cellid
              << ": successful connection of UE with IMSI " << imsi
              << " RNTI " << rnti
              << std::endl;
}

void NotifyHandoverStartEnb(std::string context,
                            uint64_t imsi,
                            uint16_t cellid,
                            uint16_t rnti,
                            uint16_t targetCellId)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context
              << " eNB CellId " << cellid
              << ": start handover of UE with IMSI " << imsi
              << " RNTI " << rnti
              << " to CellId " << targetCellId
              << std::endl;
}

void NotifyHandoverEndOkEnb(std::string context,
                            uint64_t imsi,
                            uint16_t cellid,
                            uint16_t rnti)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context
              << " eNB CellId " << cellid
              << ": completed handover of UE with IMSI " << imsi
              << " RNTI " << rnti
              << std::endl;
}

// 智能切换预测的测量报告处理函数
void NotifyUeMeasurement(std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti, LteRrcSap::MeasurementReport report, double rawRsrp, double rawRsrq) {
    // 提取服务小区RSRP
//    double rsrpResults = (double)EutranMeasurementMapping::RsrpRange2Dbm(report.measResults.measResultPCell.rsrpResult);
    double rsrpResults = rawRsrp;

    std::cout << "==========测量报告========== " << Simulator::Now().GetSeconds() << "s "
              << " UE IMSI=" << imsi << " 服务小区ID=" << cellId
              << " 服务小区RSRP: " << rsrpResults << " dBm" << std::endl;

    // 获取UE位置和速度信息
    Ptr<Node> ueNode = g_ueNodes.Get(0); // 假设只有一个UE
    Ptr<MobilityModel> mobilityModel = ueNode->GetObject<MobilityModel>();

    if (mobilityModel && g_handoverPredictor)
    {
        Vector position = mobilityModel->GetPosition();
        Vector velocity = Vector(0, 0, 0);

        // 如果是ConstantVelocityMobilityModel，获取速度
        Ptr<ConstantVelocityMobilityModel> constVelModel = DynamicCast<ConstantVelocityMobilityModel>(mobilityModel);
        if (constVelModel)
        {
            velocity = constVelModel->GetVelocity();
        }

        // 构建测量数据
        HandoverPredictor::MeasurementData measurementData;
        measurementData.timestamp = Simulator::Now();
        measurementData.rsrp = rsrpResults;
        measurementData.rsrq = 0.0; // 可以从report中提取RSRQ
        measurementData.position = position;
        measurementData.velocity = velocity;
        measurementData.cellId = cellId;

        // 提取邻区RSRP
        if (report.measResults.haveMeasResultNeighCells)
        {
            for (auto it = report.measResults.measResultListEutra.begin();
                 it != report.measResults.measResultListEutra.end(); ++it)
            {
                if (it->haveRsrpResult)
                {
                    double neighborRsrp = (double)EutranMeasurementMapping::RsrpRange2Dbm(it->rsrpResult);
                    measurementData.neighborRsrp[it->physCellId] = neighborRsrp;
                }
            }
        }

        // 添加测量数据到预测器
        g_handoverPredictor->AddMeasurement(imsi, measurementData);

        // 执行切换预测
        HandoverPredictor::PredictionResult prediction = g_handoverPredictor->PredictHandover(imsi);

        // 记录预测结果
        predictionLogFile << Simulator::Now().GetSeconds()
                          << " PREDICTION IMSI=" << imsi
                          << " WILL_HANDOVER=" << (prediction.willHandover ? "YES" : "NO")
                          << " CONFIDENCE=" << prediction.confidence
                          << " TARGET_CELL=" << prediction.targetCellId
                          << " PREDICTED_TIME=" << prediction.predictedTime.GetSeconds()
                          << " REASON=" << prediction.reason
                          << " CURRENT_RSRP=" << rsrpResults << std::endl;

        // 如果预测会发生切换且置信度足够高，设置切换标志
        if (prediction.willHandover && prediction.confidence > 0.3)  // 降低阈值
        {
            isHandoverStart = true;
            std::cout << "==========预测切换即将发生========== 置信度: "
                      << prediction.confidence << " 目标小区: " << prediction.targetCellId << std::endl;
        }

        // 输出预测结果到控制台
        std::cout << "预测结果: " << (prediction.willHandover ? "会切换" : "不会切换")
                  << " 置信度: " << prediction.confidence
                  << " 原因: " << prediction.reason << std::endl;
    }

    // 保留原有的简单阈值判断作为备用
    if (rsrpResults < -90)
    {
        isHandoverStart = true;
        std::cout << "==========简单阈值触发切换========== RSRP: " << rsrpResults << " dBm" << std::endl;
    }

    // 强制刷新输出
    std::cout.flush();
    predictionLogFile.flush();
}

// 确保目录存在
void EnsureDirectoryExists(const std::string& dir) {
    if (mkdir(dir.c_str(), 0755) == 0 || errno == EEXIST) {
        NS_LOG_INFO("Directory exists or created successfully.");
    } else {
        NS_LOG_ERROR("Failed to create directory: " << dir);
    }
}

void HandoverInitiatedCallback(Ptr<const MobilityModel> mobility)
{
    std::cout << "Handover initiated at time: " << Simulator::Now().GetSeconds() << "s" << std::endl;
    std::cout << "New Cell ID: " << mobility->GetPosition().x << ", " << mobility->GetPosition().y << std::endl;
}

// 声明全局变量用于存储吞吐量、RTT 和丢包数据
std::vector<double> txThroughputData;
std::vector<double> rxThroughputData;
std::vector<double> rttData;
std::vector<double> packetLossData;

double prevTxBytes = 0;
double prevRxBytes = 0;

void UpdateRTT(Ptr<const Packet> packet, const Address &from, const Address &to) {
    Time currentTime = Simulator::Now();
    double rtt = currentTime.GetSeconds() * 1000;
    rttData.push_back(rtt);
}

void UpdateStats(Ptr<FlowMonitor> monitor) {
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double currentTxBytes = 0;
    double currentRxBytes = 0;
    double totalPacketsSent = 0;
    double totalPacketsReceived = 0;
    double totalPacketsLost = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        currentTxBytes += it->second.txBytes;
        currentRxBytes += it->second.rxBytes;
        totalPacketsSent += it->second.txPackets;
        totalPacketsReceived += it->second.rxPackets;
        totalPacketsLost += it->second.lostPackets;
    }

    double txThroughputBytes = currentTxBytes - prevTxBytes;
    double txThroughputMbps = (txThroughputBytes * 8) / (0.1 * 1e6);

    double rxThroughputBytes = currentRxBytes - prevRxBytes;
    double rxThroughputMbps = (rxThroughputBytes * 8) / (0.1 * 1e6);

    txThroughputData.push_back(txThroughputMbps);
    rxThroughputData.push_back(rxThroughputMbps);
    prevTxBytes = currentTxBytes;
    prevRxBytes = currentRxBytes;

    double packetLossRate = 0;
    if (totalPacketsSent > 0) {
        packetLossRate = (totalPacketsLost / totalPacketsSent) * 100;
    }
    packetLossData.push_back(packetLossRate);

    Simulator::Schedule(Seconds(0.1), &UpdateStats, monitor);
}

void OutputStats() {
    std::ofstream txThroughputFile("txThroughput_data.txt");
    for (double throughput : txThroughputData) {
        txThroughputFile << throughput << std::endl;
    }
    txThroughputFile.close();

    std::ofstream rxThroughputFile("rxThroughput_data.txt");
    for (double throughput : rxThroughputData) {
        rxThroughputFile << throughput << std::endl;
    }
    rxThroughputFile.close();

    std::ofstream packetLossFile("packet_loss_data.txt");
    for (double lossRate : packetLossData) {
        packetLossFile << lossRate << std::endl;
    }
    packetLossFile.close();
}

// Tracers (省略具体实现，与原代码相同)
static void CwndTracer(uint32_t oldval, uint32_t newval) {
    if (firstCwnd) {
        firstCwnd = false;
    }
    *cWndStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    cWndValue = newval;

    if (!firstSshThr) {
        *ssThreshStream->GetStream() << Simulator::Now().GetSeconds() << " " << ssThreshValue << std::endl;
    }
}

static void SsThreshTracer(uint32_t oldval, uint32_t newval) {
    if (firstSshThr) {
        firstSshThr = false;
    }
    *ssThreshStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    ssThreshValue = newval;

    if (!firstCwnd) {
        *cWndStream->GetStream() << Simulator::Now().GetSeconds() << " " << cWndValue << std::endl;
    }
}

static void RttTracer(Time oldval, Time newval) {
    if (firstRtt) {
        firstRtt = false;
    }
    *rttStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval.GetSeconds() << std::endl;
}

static void RtoTracer(Time oldval, Time newval) {
    if (firstRto) {
        firstRto = false;
    }
    *rtoStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval.GetSeconds() << std::endl;
}

static void RwndTracer(uint32_t oldval, uint32_t newval) {
    if (firstRwnd) {
        firstRwnd = false;
    }
    *rwndStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

static void AdvWNDTracer(uint32_t oldval, uint32_t newval) {
    if (firstAdvWND) {
        firstAdvWND = false;
    }
    *advWNDStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

static void HrackTracer(const SequenceNumber32 oldval, const SequenceNumber32 newval) {
    if (firstHrack) {
        firstHrack = false;
    }
    *hrackStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

static void TraceCwnd(std::string cwnd_tr_file_name) {
    AsciiTraceHelper ascii;
    cWndStream = ascii.CreateFileStream((trFileDir + cwnd_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndTracer));
}

static void TraceSsThresh(std::string ssthresh_tr_file_name) {
    AsciiTraceHelper ascii;
    ssThreshStream = ascii.CreateFileStream((trFileDir + ssthresh_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold", MakeCallback(&SsThreshTracer));
}

static void TraceRtt(std::string rtt_tr_file_name) {
    AsciiTraceHelper ascii;
    rttStream = ascii.CreateFileStream(rtt_tr_file_name.c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback(&RttTracer));
}

static void TraceRto(std::string rto_tr_file_name) {
    AsciiTraceHelper ascii;
    rtoStream = ascii.CreateFileStream((trFileDir + rto_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/RTO", MakeCallback(&RtoTracer));
}

static void TraceRwnd(std::string rwnd_tr_file_name) {
    AsciiTraceHelper ascii;
    rwndStream = ascii.CreateFileStream((trFileDir + rwnd_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/11/$ns3::TcpL4Protocol/SocketList/0/RWND", MakeCallback(&RwndTracer));
}

static void TraceAdvWND(std::string advWND_tr_file_name) {
    AsciiTraceHelper ascii;
    advWNDStream = ascii.CreateFileStream((trFileDir + advWND_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/11/$ns3::TcpL4Protocol/SocketList/0/AdvWND", MakeCallback(&AdvWNDTracer));
}

static void TraceHighestRxAck(std::string hrack_tr_file_name) {
    AsciiTraceHelper ascii;
    hrackStream = ascii.CreateFileStream((trFileDir + hrack_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/HighestRxAck", MakeCallback(&HrackTracer));
}

void PrintProgress(Ptr<OutputStreamWrapper> stream_throughput) {
    Time now_t = Simulator::Now();

    if (*(lastReportTime) == MicroSeconds(0)) {
        AsciiTraceHelper ascii;
        stream_throughput = ascii.CreateFileStream((trFileDir + thrputStream_tr_file_name).c_str());

        Simulator::Schedule(Seconds(0.25), PrintProgress, stream_throughput);
        lastRxMbits.resize(sinkApps.GetN(), 0.0);
        *(lastReportTime) = now_t;
        return;
    }

    *stream_throughput->GetStream() << now_t;
    *stream_throughput->GetStream() << std::fixed << std::setprecision(6);

    for (uint32_t appIdx = 0; appIdx < sinkApps.GetN(); appIdx++) {
        uint64_t rx_bytes;
        double cur_rx_Mbits;

        rx_bytes = DynamicCast<PacketSink>(sinkApps.Get(appIdx))->GetTotalRx();
        cur_rx_Mbits = (rx_bytes * 8.0) / 1e6;

        *stream_throughput->GetStream() << " " << (cur_rx_Mbits - lastRxMbits[appIdx]) / ((now_t - *(lastReportTime)).GetNanoSeconds() / 1e9) << " ";

        lastRxMbits[appIdx] = cur_rx_Mbits;
    }

    *stream_throughput->GetStream() << std::endl;

    Simulator::Schedule(Seconds(0.25), PrintProgress, stream_throughput);

    *(lastReportTime) = now_t;
}

int main(int argc, char *argv[])
{
    // 打开预测结果日志文件
    predictionLogFile.open("handover_prediction_log.txt");
        // 打开日志文件
        std::ofstream logFile("handover_predic_log.txt");

        // 保存原标准输出流
        std::clog.rdbuf(logFile.rdbuf());  // 重定向到文件

        // 启用日志
    //    LogComponentEnable("TcpSocketBase", LogLevel(LOG_LEVEL_INFO));
        LogComponentEnable("HandoverPredictor", LogLevel(LOG_LEVEL_INFO));

    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpCubic"));

    // 读取基站位置文件
    ifstream EnbinFile("scratch/EnbLocations2.txt");
    int enblines;

    if (!EnbinFile.is_open()) {
        std::cerr << "Error: Could not open EnbLocations2.txt" << std::endl;
        return -1;
    } else {
        cout << "access txt" << endl;
    }

    enblines = count(istreambuf_iterator<char>(EnbinFile), istreambuf_iterator<char>(), '\n');
    cout << enblines << endl;
    EnbinFile.close();

    ifstream Enbcin("scratch/EnbLocations2.txt");

    uint16_t numberOfUes = 1;
    uint16_t numberOfEnbs = enblines;
    uint16_t numBearersPerUe = 1;
    double simTime = 30.0;
    double enbTxPowerDbm = 46.0;

    // 命令行参数
    unsigned int dlinterval = 300, dlpacketsize = 1024, ulinterval = 300, ulpacketsize = 1024;
    CommandLine cmd;
    cmd.AddValue("simTime", "Total duration of the simulation (in seconds)", simTime);
    cmd.AddValue("enbTxPowerDbm", "TX power [dBm] used by HeNBs (default = 46.0)", enbTxPowerDbm);
    cmd.AddValue("dlpacketSize", "Size (bytes) of packets generated (default = 1024 bytes)", dlpacketsize);
    cmd.AddValue("dlpacketsInterval", "The time (ms) wait between packets (default = 500 ms)", dlinterval);
    cmd.AddValue("ulpacketSize", "Size (bytes) of packets generated (default = 1024 bytes)", ulpacketsize);
    cmd.AddValue("ulpacketsInterval", "The time (ms) wait between packets (default = 500 ms)", ulinterval);
    cmd.Parse(argc, argv);

    // 设置默认属性
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(500)));
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(4294967295));
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(false));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    // 设置衰落模型
    lteHelper->SetFadingModel("ns3::TraceFadingLossModel");
    lteHelper->SetFadingModelAttribute("TraceFilename", StringValue("src/lte/model/fading-traces/fading_trace_Highspeed_300kmph.fad"));

    // 设置信道干扰和噪声
    Config::SetDefault("ns3::LteEnbPhy::NoiseFigure", DoubleValue(9.0));

    // 增加路径损耗
    Ptr<LogDistancePropagationLossModel> propagationLossModel = CreateObject<LogDistancePropagationLossModel>();
    propagationLossModel->SetReference(1.0, 46.0);
    propagationLossModel->SetPathLossExponent(3.5);

    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();

    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");
    lteHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(1.0));  // 降低滞后，更容易触发切换
    lteHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(128)));  // 缩短触发时间

    // 配置测量参数，确保测量报告能够触发
    Config::SetDefault("ns3::LteUeRrc::T310", TimeValue(MilliSeconds(1000)));
    Config::SetDefault("ns3::LteUeRrc::N310", UintegerValue(1));
    Config::SetDefault("ns3::LteUeRrc::N311", UintegerValue(1));

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // 创建远程主机
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    MobilityHelper remoteHostMobility;
    Ptr<ListPositionAllocator> remoteHostPositionAlloc = CreateObject<ListPositionAllocator>();
    remoteHostPositionAlloc->Add(Vector(0.0, 0.0, 0.0));

    remoteHostMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    remoteHostMobility.SetPositionAllocator(remoteHostPositionAlloc);
    remoteHostMobility.Install(remoteHost);

    // 创建互联网连接
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.020)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // 路由设置
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // 创建节点
    g_enbNodes.Create(numberOfEnbs);
    g_ueNodes.Create(numberOfUes);

    string words;
    // 安装基站移动模型
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();

    for (int i = 0; i < enblines; i++) {
        string enbtmp_ch, locationx, locationy;
        getline(Enbcin, words);
        int xory = 0;
        for (unsigned int j = 0; j < words.length(); j++) {
            enbtmp_ch = words[j];

            if (enbtmp_ch == ":") {
                xory += 1;
                continue;
            }
            if (enbtmp_ch == " ") {
                if (xory != 0) {
                    Vector enbPosition(stof(locationx), stof(locationy), 0);
                    enbPositionAlloc->Add(enbPosition);
                    cout << locationx << " " << locationy << endl;
                }
                xory += 1;
                continue;
            } else if (xory == 1) {
                locationx.append(enbtmp_ch);
            } else if (xory == 2) {
                locationy.append(enbtmp_ch);
            }
        }

        xory = 0;
        locationx.erase();
        locationy.erase();
        std::cout << "here" << std::endl;
    }

    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(g_enbNodes);

    // 设置小区位置信息给预测器
    for (uint32_t i = 0; i < g_enbNodes.GetN(); ++i) {
        Ptr<MobilityModel> mobilityModel = g_enbNodes.Get(i)->GetObject<MobilityModel>();
        Vector cellPos = mobilityModel->GetPosition();
        // 暂时注释掉，因为HandoverPredictor类中没有这些方法
        // g_handoverPredictor->SetCellPosition(i+1, cellPos); // cellId从1开始
        // g_handoverPredictor->SetCellCoverage(i+1, 500.0);   // 设置500米覆盖范围
    }

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    ueMobility.Install(g_ueNodes);

    // 设置UE移动速度为300km/h (83.3 m/s)
    for (uint32_t i = 0; i < g_ueNodes.GetN(); i++) {
        Ptr<ConstantVelocityMobilityModel> mobilityModel = DynamicCast<ConstantVelocityMobilityModel>(g_ueNodes.Get(i)->GetObject<MobilityModel>());
        if (mobilityModel) {
            mobilityModel->SetPosition(Vector(0.0, 300.0, 0.0));
            mobilityModel->SetVelocity(Vector(83.3, 0, 0)); // 300 km/h
        }
        cout << i << endl;
    }

    // 安装LTE设备
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(g_enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(g_ueNodes);

    if (enbLteDevs.GetN() > 0 && ueLteDevs.GetN() > 0) {
        std::cout << "LTE devices installed successfully." << std::endl;
    } else {
        std::cerr << "Failed to install LTE devices!" << std::endl;
    }

    p2ph.EnablePcap("high-speed", g_ueNodes.Get(0)->GetId(), 0, true);
    p2ph.EnablePcapAll("tcp-high-speed");

    // 安装IP协议栈
    internet.Install(g_ueNodes);
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    NS_LOG_LOGIC("setting up applications");

    // 打印节点位置
    for (uint32_t i = 0; i < g_enbNodes.GetN(); ++i) {
        Ptr<MobilityModel> mobilityModel = g_enbNodes.Get(i)->GetObject<MobilityModel>();
        cout << "eNB Position: " << mobilityModel->GetPosition() << endl;
    }
    cout << g_enbNodes.GetN() << endl;

    for (uint32_t i = 0; i < g_ueNodes.GetN(); ++i) {
        Ptr<MobilityModel> mobilityModel = g_ueNodes.Get(i)->GetObject<MobilityModel>();
        cout << "UE Position: " << mobilityModel->GetPosition() << endl;
    }
    cout << g_ueNodes.GetN() << endl;

    // 创建和配置切换预测器
    g_handoverPredictor = CreateObject<HandoverPredictor>();
    g_handoverPredictor->SetPredictionAlgorithm(HandoverPredictor::RSRP_TREND_BASED);  // 使用单一算法更容易调试
    g_handoverPredictor->SetPredictionWindow(Seconds(2.0));
    g_handoverPredictor->SetRsrpThreshold(-88.0);  // 设置更高的阈值，更容易触发
    g_handoverPredictor->SetVelocityThreshold(10.0);  // 降低速度阈值
    g_handoverPredictor->SetConfidenceThreshold(0.1);  // 大幅降低置信度阈值

    // 设置小区位置信息给预测器
    for (uint32_t i = 0; i < g_enbNodes.GetN(); ++i) {
        Ptr<MobilityModel> mobilityModel = g_enbNodes.Get(i)->GetObject<MobilityModel>();
        Vector cellPos = mobilityModel->GetPosition();
        // 这里需要修改HandoverPredictor类来添加SetCellPosition方法
        // g_handoverPredictor->SetCellPosition(i+1, cellPos); // cellId从1开始
    }

    // 安装和启动应用程序
    uint16_t dlPort = 10000;
    uint16_t ulPort = 20000;

    Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable>();
    startTimeSeconds->SetAttribute("Min", DoubleValue(0.100));
    startTimeSeconds->SetAttribute("Max", DoubleValue(0.110));

    lteHelper->AddX2Interface(g_enbNodes);

    for (uint32_t u = 0; u < numberOfUes; ++u) {
        Ptr<Node> ue = g_ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

        std::cout << "Static Routing Table for UE " << u << ":" << std::endl;
        if (ueStaticRouting) {
            std::cout << "Default Route: " << ueStaticRouting->GetDefaultRoute() << std::endl;
        } else {
            std::cout << "No static routing found for this node." << std::endl;
        }

        for (uint32_t b = 0; b < numBearersPerUe; ++b) {
            ++dlPort;
            ++ulPort;
            cout << dlPort << endl;
            cout << ulPort << endl;

            NS_LOG_LOGIC("installing TCP DL app for UE " << u);
            BulkSendHelper dlClientHelper("ns3::TcpSocketFactory",
                                          InetSocketAddress(ueIpIfaces.GetAddress(u), dlPort));
            cout << "socket success" << endl;
            dlClientHelper.SetAttribute("SendSize", UintegerValue(1024));
            dlClientHelper.SetAttribute("MaxBytes", UintegerValue(4294967295));

            sourceApps.Add(dlClientHelper.Install(remoteHost));

            Ptr<PacketSink> source = DynamicCast<PacketSink>(sourceApps.Get(0));

            PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));

            sinkApps.Add(dlPacketSinkHelper.Install(ue));

            Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkApps.Get(0));

            Ptr<EpcTft> tft = Create<EpcTft>();
            EpcTft::PacketFilter dlpf;
            dlpf.localPortStart = dlPort;
            dlpf.localPortEnd = dlPort;
            tft->Add(dlpf);

            EpsBearer bearer(EpsBearer::NGBR_VIDEO_TCP_DEFAULT);
            lteHelper->ActivateDedicatedEpsBearer(ueLteDevs.Get(u), bearer, tft);

            cout << "start time:" << Seconds(startTimeSeconds->GetValue()) << endl;

            Time startTime = Seconds(startTimeSeconds->GetValue());
            sinkApps.Start(startTime);
            sourceApps.Start(startTime);

            Simulator::Schedule(Seconds(startTimeSeconds->GetValue() + 0.01), &MyCheckSocketFunction, packetSink);

            Ptr<TcpL4Protocol> tcp = ue->GetObject<TcpL4Protocol>();
            if (tcp) {
                cout << "TCP protocol is initialized." << endl;
            }
        }
    }

    // 启用跟踪
    lteHelper->EnableRlcTraces();
    lteHelper->EnablePdcpTraces();
    Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats();
    rlcStats->SetAttribute("EpochDuration", TimeValue(Seconds(0.001)));
    Ptr<RadioBearerStatsCalculator> pdcpStats = lteHelper->GetPdcpStats();
    pdcpStats->SetAttribute("EpochDuration", TimeValue(Seconds(0.001)));

    // 设置流量监控
    Ptr<FlowMonitor> monitor;
    FlowMonitorHelper flowHelper;
    NodeContainer flowEnd2End = g_ueNodes;
    flowEnd2End.Add(remoteHost);
    monitor = flowHelper.Install(flowEnd2End);
    Simulator::Schedule(Seconds(0.1), &UpdateStats, monitor);

    std::cout << "Total nodes in flow monitor: " << flowEnd2End.GetN() << std::endl;
    std::cout << "Number of UE nodes: " << g_ueNodes.GetN() << std::endl;
    std::cout << "Number of eNB nodes: " << g_enbNodes.GetN() << std::endl;
    std::cout << "Number of remoteHost nodes: " << remoteHostContainer.GetN() << std::endl;

    lastReportTime = new Time(MicroSeconds(0));
    EnsureDirectoryExists(trFileDir);

    // 调度跟踪器
    Simulator::Schedule(Seconds(0.11001), &TraceCwnd, cwnd_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceSsThresh, ssthresh_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceRtt, rtt_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceRto, rto_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceRwnd, rwnd_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceAdvWND, advWND_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceHighestRxAck, hrack_tr_file_name);

    // 连接自定义跟踪接收器
    Config::Connect("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                    MakeCallback(&NotifyConnectionEstablishedEnb));
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                    MakeCallback(&NotifyConnectionEstablishedUe));
    Config::Connect("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                    MakeCallback(&NotifyHandoverStartEnb));
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                    MakeCallback(&NotifyHandoverStartUe));
    Config::Connect("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                    MakeCallback(&NotifyHandoverEndOkEnb));
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                    MakeCallback(&NotifyHandoverEndOkUe));
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/ReportUeMeasurementWithRawRsrp",
                    MakeCallback(&NotifyUeMeasurement));

    Simulator::Stop(Seconds(60.0));

    // 动画设置
    AnimationInterface anim("lte-ananew-with-predictor.xml");
    anim.SetMaxPktsPerTraceFile(300000);
    anim.EnablePacketMetadata();

    for (uint32_t i = 0; i < numberOfUes; ++i) {
        anim.UpdateNodeColor(g_ueNodes.Get(i), 255, 0, 0);  // UE为红色
        anim.UpdateNodeSize(i, 15, 15);
    }

    anim.UpdateNodeColor(remoteHost, 0, 0, 255);  // 远程主机为蓝色
    anim.UpdateNodeSize(0, 15, 15);

    for (uint32_t i = 0; i < g_enbNodes.GetN(); ++i) {
        anim.UpdateNodeColor(g_enbNodes.Get(i), 0, 255, 0);  // 基站为绿色
        anim.UpdateNodeSize(i, 15, 15);
    }

    std::cout << "==========开始仿真，使用智能切换预测算法==========" << std::endl;

    // 启动预测器状态检查和测试数据生成
//    Simulator::Schedule(Seconds(0.5), &AddTestMeasurementData);
//    Simulator::Schedule(Seconds(1.0), &CheckPredictorStatus);

    Simulator::Run();

    // 仿真结束后的处理
    for (uint32_t appIdx = 0; appIdx < sinkApps.GetN(); appIdx++) {
        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(appIdx));
    }

    OutputStats();
    monitor->TraceConnectWithoutContext("RxPacket", MakeCallback(&UpdateRTT));

    monitor->CheckForLostPackets();
    double throughput = 0.0;

    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        std::cout << "FlowId:" << i->first << "\n";
        std::cout << "TxBytes:" << i->second.txBytes << "\t\t";
        std::cout << "RxBytes:" << i->second.rxBytes << "\t\t";
        std::cout << "lostPackets:" << i->second.lostPackets << "\t\t";

        throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
        std::cout << "throughput:" << throughput << " \n\n";
    }
    std::cout << "\n";

    monitor->SerializeToXmlFile("lte-grid-with-predictor.xml", true, true);

    // 关闭日志文件
    predictionLogFile.close();

    lteHelper = 0;
    Simulator::Destroy();
    return 0;
}
