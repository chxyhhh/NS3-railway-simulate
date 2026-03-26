#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-helper.h" // 引入 FlowMonitor 模块
#include "ns3/internet-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/queue-size.h"
#include "ns3/red-queue-disc.h"
#include "ns3/tcp-cubic.h"
#include "ns3/tcp-rx-buffer.h"
#include "ns3/traffic-control-helper.h"
#include <ns3/buildings-module.h>
#include <ns3/log.h>
#include <ns3/netanim-module.h>
#include <ns3/point-to-point-helper.h>
//================================-------------------------------------
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
//=================================--------------------------------------
using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("LteGrid");

std::string cwnd_tr_file_name = "cwndTr.txt";
std::string ssthresh_tr_file_name = "sshThTr.txt";
std::string rtt_tr_file_name = "rttTr.txt";
std::string rto_tr_file_name = "rtoTr.txt";
std::string rwnd_tr_file_name = "rwndTr.txt";
std::string advWND_tr_file_name = "advWNDTr.txt";
std::string hrack_tr_file_name = "hrackTr.txt";
std::string trFileDir = "./traces/"; ////////////!!!!!!!!!!!!!!!!!!!not complet3

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

// ── 切换感知 TCP 优化：命令行参数 & 触发逻辑 ────────────────────
std::string g_expCase        = "joint";      // baseline / rwnd-only / ack-only / joint
std::string g_triggerSource  = "position";   // position / time / rsrp
std::string g_triggerTimesSec   = "";        // 逗号分隔的绝对触发时刻（秒）
std::string g_triggerPositionsM = "450,1450,2450,3450,4450,5450"; // 触发位置（米）
double      g_triggerAdvanceMs  = 500;       // 提前量（毫秒）
uint32_t    g_ackSplitCount     = 3;
double      g_ackSplitK         = 0.5;
double      g_holdDurationMs    = 200;
double      g_restoreDurationMs = 200;
double      g_rwndAlphaFloor    = 0.3;
double      g_rwndBeta          = 2.0;
double      g_rwndGamma         = 2.0;

bool g_enableRwnd     = true;
bool g_enableAckSplit = true;

std::vector<double> g_triggerPosList;
std::vector<bool>   g_triggeredFlags;

// ── 优化机制追踪：每 0.1s 记录 alpha / ACK 计数 / 有效窗口 ──
struct OptimRecord {
    double time;
    double alpha;
    uint64_t totalAck;
    uint64_t splitAck;
    bool   rwndActive;
    bool   ackSplitActive;
};
std::vector<OptimRecord> g_optimRecords;

void
LogOptimTrace()
{
    OptimRecord r;
    r.time          = Simulator::Now().GetSeconds();
    r.alpha         = TcpSocketBase::s_lastAlpha;
    r.totalAck      = TcpSocketBase::s_totalAckSent;
    r.splitAck      = TcpSocketBase::s_splitAckSent;
    r.rwndActive    = TcpSocketBase::s_rwndActive;
    r.ackSplitActive = TcpSocketBase::s_ackSplitActive;
    g_optimRecords.push_back(r);
    Simulator::Schedule(Seconds(0.1), &LogOptimTrace);
}

void
OutputOptimTrace()
{
    std::ofstream f(trFileDir + "optim_trace.csv");
    f << "time,alpha,totalAck,splitAck,rwndActive,ackSplitActive" << std::endl;
    f << std::fixed << std::setprecision(4);
    for (auto& r : g_optimRecords)
    {
        f << r.time << "," << r.alpha << "," << r.totalAck << ","
          << r.splitAck << "," << r.rwndActive << "," << r.ackSplitActive << std::endl;
    }
    f.close();
    std::cout << "[OUTPUT] traces/optim_trace.csv  (" << g_optimRecords.size() << " rows)" << std::endl;
}

// 解析逗号分隔的 double 列表
std::vector<double>
ParseDoubleList(const std::string& s)
{
    std::vector<double> result;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        if (!item.empty())
            result.push_back(std::stod(item));
    }
    return result;
}

// 执行一次触发
void
DoOptimTrigger()
{
    TcpSocketBase::TriggerHandoverOptim(
        MilliSeconds(g_triggerAdvanceMs),
        MilliSeconds(g_holdDurationMs),
        MilliSeconds(g_restoreDurationMs),
        g_enableRwnd,
        g_enableAckSplit,
        g_ackSplitCount,
        g_ackSplitK);
}

// 定期检查 UE 位置，到达指定坐标时触发
void
CheckPositionTrigger(Ptr<Node> ue)
{
    Ptr<MobilityModel> mob = ue->GetObject<MobilityModel>();
    double x = mob->GetPosition().x;
    for (size_t i = 0; i < g_triggerPosList.size(); i++)
    {
        if (!g_triggeredFlags[i] && x >= g_triggerPosList[i])
        {
            g_triggeredFlags[i] = true;
            std::cout << Simulator::Now().GetSeconds()
                      << "s [TRIGGER] position=" << g_triggerPosList[i]
                      << "m  UE_x=" << x << std::endl;
            DoOptimTrigger();
        }
    }
    Simulator::Schedule(MilliSeconds(10), &CheckPositionTrigger, ue);
}
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>.

ApplicationContainer sourceApps;
ApplicationContainer sinkApps;
Time* lastReportTime;
std::vector<double> lastRxMbits;

// Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

void
RxCallback(Ptr<const Packet> packet, const Address& from)
{
    NS_LOG_INFO("Packet received with size: " << packet->GetSize() << " from address: "
                                              << InetSocketAddress::ConvertFrom(from).GetIpv4());
}

void
RxWithAddressesCallback(Ptr<const Packet> packet, const Address& from, const Address& to)
{
    NS_LOG_INFO("Packet received with size: "
                << packet->GetSize() << " from: " << InetSocketAddress::ConvertFrom(from).GetIpv4()
                << " to: " << InetSocketAddress::ConvertFrom(to).GetIpv4());
}

void
RxWithSeqTsSizeCallback(Ptr<const Packet> packet,
                        const Address& from,
                        const Address& to,
                        const SeqTsSizeHeader& header)
{
    NS_LOG_INFO("Packet received with Seq: " << header.GetSeq() << " Size: " << header.GetSize()
                                             << " Timestamp: " << header.GetTs());
}

// void attachinfuture(NetDeviceContainer &ues, int ueid)
// {
//   lteHelper->Attach(ues.Get(ueid));
//   // lteHelper->Attach (ues, enbs.Get (1));
//   std::cout << "attach" << std::endl;
// }

static void
RxDrop(Ptr<const Packet> p) // 丢包 回调函数
{
    NS_LOG_UNCOND("RxDrop at " << Simulator::Now().GetSeconds());
}

void
TraceTcpDupAck(std::string context, uint32_t seqNo, uint32_t ackNo)
{
    std::cout << "Dup ACK: " << "seqNo = " << seqNo << ", ackNo = " << ackNo << " at time "
              << Simulator::Now().GetSeconds() << "s" << std::endl;
}

void
TraceTcpRetransmission(std::string context, uint32_t seqNo, uint32_t ackNo)
{
    std::cout << "TCP Retransmission: " << "seqNo = " << seqNo << ", ackNo = " << ackNo
              << " at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
}

void
MyCheckSocketFunction(Ptr<PacketSink> packetSink)
{
    // 保留轮询以便调试；切换优化触发已改为 TcpSocketBase 静态接口
    Simulator::Schedule(Seconds(0.001), &MyCheckSocketFunction, packetSink);
}

// ── 切换事件日志 ──
struct HandoverEvent {
    double time;
    std::string type;   // "HO_START" or "HO_END"
    uint16_t fromCell;
    uint16_t toCell;
};
std::vector<HandoverEvent> g_hoEvents;

void
OutputHandoverEvents()
{
    std::ofstream f(trFileDir + "handover_events.csv");
    f << "time,type,fromCell,toCell" << std::endl;
    for (auto& e : g_hoEvents)
    {
        f << std::fixed << std::setprecision(4)
          << e.time << "," << e.type << "," << e.fromCell << "," << e.toCell << std::endl;
    }
    f.close();
    std::cout << "[OUTPUT] traces/handover_events.csv  (" << g_hoEvents.size() << " events)" << std::endl;
}

void
NotifyConnectionEstablishedUe(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti)
{
    std::cout << "at " << Simulator::Now().GetSeconds() << "s " << context << " UE IMSI " << imsi
              << ": connected to CellId " << cellid << " with RNTI " << rnti << std::endl;
}

void
NotifyHandoverStartUe(std::string context,
                      uint64_t imsi,
                      uint16_t cellid,
                      uint16_t rnti,
                      uint16_t targetCellId)
{
    double t = Simulator::Now().GetSeconds();
    std::cout << t << "s UE IMSI " << imsi
              << ": HO_START CellId " << cellid << " → " << targetCellId << std::endl;
    g_hoEvents.push_back({t, "HO_START", cellid, targetCellId});
}

void
NotifyHandoverEndOkUe(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti)
{
    double t = Simulator::Now().GetSeconds();
    std::cout << t << "s UE IMSI " << imsi
              << ": HO_END   CellId " << cellid << std::endl;
    g_hoEvents.push_back({t, "HO_END", cellid, cellid});
}

void
NotifyConnectionEstablishedEnb(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context << " eNB CellId " << cellid
              << ": successful connection of UE with IMSI " << imsi << " RNTI " << rnti
              << std::endl;
}

void
NotifyHandoverStartEnb(std::string context,
                       uint64_t imsi,
                       uint16_t cellid,
                       uint16_t rnti,
                       uint16_t targetCellId)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context << " eNB CellId " << cellid
              << ": start handover of UE with IMSI " << imsi << " RNTI " << rnti << " to CellId "
              << targetCellId << std::endl;
    std::cout << BooleanValue(isHandoverStart) << std::endl;
}

void
NotifyHandoverEndOkEnb(std::string context, uint64_t imsi, uint16_t cellid, uint16_t rnti)
{
    std::cout << Simulator::Now().GetSeconds() << "s " << context << " eNB CellId " << cellid
              << ": completed handover of UE with IMSI " << imsi << " RNTI " << rnti << std::endl;
}

// 捕获UE的测量报告
void
NotifyUeMeasurement(std::string context,
                    uint64_t imsi,
                    uint16_t cellId,
                    uint16_t rnti,
                    LteRrcSap::MeasurementReport report)
{
    // 提取服务小区RSRP（单位：dBm）
    // if (!report.empty()) {
    // double rsrp_dBm =
    // EutranMeasurementMapping::RsrpRange2Dbm(report.measResults.measResultPCell.rsrpResult);

    // 添加到预测器
    // g_predictor.AddRsrpMeasurement(rsrp_dBm);

    // 打印实时数据（调试用）
    double rsrpResults = (double)EutranMeasurementMapping::RsrpRange2Dbm(
        report.measResults.measResultPCell.rsrpResult);
    std::cout << Simulator::Now().GetSeconds() << "s " << " UE IMSI=" << imsi
              << " 服务小区RSRP: " << rsrpResults << " dBm" << std::endl;
    // RSRP 触发模式：仅当 triggerSource == "rsrp" 时生效
    if (g_triggerSource == "rsrp" && rsrpResults < -90)
    {
        std::cout << Simulator::Now().GetSeconds() << "s "
                  << "[TRIGGER] RSRP=" << rsrpResults << " < -90dBm" << std::endl;
        DoOptimTrigger();
    }
    //}
}

// 确保目录存在
void
EnsureDirectoryExists(const std::string& dir)
{
    if (mkdir(dir.c_str(), 0755) == 0 || errno == EEXIST)
    {
        NS_LOG_INFO("Directory exists or created successfully.");
    }
    else
    {
        NS_LOG_ERROR("Failed to create directory: " << dir);
    }
}

void
HandoverInitiatedCallback(Ptr<const MobilityModel> mobility)
{
    std::cout << "Handover initiated at time: " << Simulator::Now().GetSeconds() << "s"
              << std::endl;
    std::cout << "New Cell ID: " << mobility->GetPosition().x << ", " << mobility->GetPosition().y
              << std::endl;
}

// ── 时序统计数据（带时间戳）──
struct TimeSeriesRecord {
    double time;
    double txThrMbps;
    double rxThrMbps;
    uint64_t cumLostPkts;
    double   instLossRate;   // 本周期丢包率 %
    double   avgDelayMs;
};
std::vector<TimeSeriesRecord> g_tsRecords;

double prevTxBytes = 0;
double prevRxBytes = 0;
uint64_t prevLostPkts = 0;
uint64_t prevTxPkts   = 0;

// 每 0.1s 采样一次关键指标
void
UpdateStats(Ptr<FlowMonitor> monitor)
{
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double   currentTxBytes = 0, currentRxBytes = 0;
    uint64_t totalTxPkts = 0, totalLostPkts = 0;
    double   delaySum = 0;
    uint64_t rxPkts = 0;

    for (auto it = stats.begin(); it != stats.end(); ++it)
    {
        currentTxBytes += it->second.txBytes;
        currentRxBytes += it->second.rxBytes;
        totalTxPkts    += it->second.txPackets;
        totalLostPkts  += it->second.lostPackets;
        delaySum       += it->second.delaySum.GetSeconds();
        rxPkts         += it->second.rxPackets;
    }

    double dt = 0.1; // 采样周期
    TimeSeriesRecord rec;
    rec.time       = Simulator::Now().GetSeconds();
    rec.txThrMbps  = ((currentTxBytes - prevTxBytes) * 8.0) / (dt * 1e6);
    rec.rxThrMbps  = ((currentRxBytes - prevRxBytes) * 8.0) / (dt * 1e6);
    rec.cumLostPkts = totalLostPkts;

    // 本周期瞬时丢包率
    uint64_t periodTxPkts  = totalTxPkts - prevTxPkts;
    uint64_t periodLostPkts = totalLostPkts - prevLostPkts;
    rec.instLossRate = (periodTxPkts > 0)
                       ? (static_cast<double>(periodLostPkts) / periodTxPkts * 100.0)
                       : 0.0;
    rec.avgDelayMs = (rxPkts > 0) ? (delaySum / rxPkts * 1000.0) : 0.0;

    g_tsRecords.push_back(rec);

    prevTxBytes  = currentTxBytes;
    prevRxBytes  = currentRxBytes;
    prevLostPkts = totalLostPkts;
    prevTxPkts   = totalTxPkts;

    Simulator::Schedule(Seconds(dt), &UpdateStats, monitor);
}

// 仿真结束后写入 CSV
void
OutputStats()
{
    std::ofstream f(trFileDir + "timeseries.csv");
    f << "time,txThr_Mbps,rxThr_Mbps,cumLostPkts,instLossRate_pct,avgDelay_ms" << std::endl;
    f << std::fixed << std::setprecision(4);
    for (auto& r : g_tsRecords)
    {
        f << r.time << "," << r.txThrMbps << "," << r.rxThrMbps << ","
          << r.cumLostPkts << "," << r.instLossRate << "," << r.avgDelayMs << std::endl;
    }
    f.close();
    std::cout << "[OUTPUT] traces/timeseries.csv  (" << g_tsRecords.size() << " rows)" << std::endl;
}

///////////////////////////////////////////////Tracers////////////////////////////////////////////////////////
static void
CwndTracer(uint32_t oldval, uint32_t newval)
{
    if (firstCwnd)
    {
        firstCwnd = false;
    }
    *cWndStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    cWndValue = newval;

    if (!firstSshThr)
    {
        *ssThreshStream->GetStream()
            << Simulator::Now().GetSeconds() << " " << ssThreshValue << std::endl;
    }
}

static void
SsThreshTracer(uint32_t oldval, uint32_t newval)
{
    if (firstSshThr)
    {
        firstSshThr = false;
    }
    *ssThreshStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
    ssThreshValue = newval;

    if (!firstCwnd)
    {
        *cWndStream->GetStream() << Simulator::Now().GetSeconds() << " " << cWndValue << std::endl;
    }
}

static void
RttTracer(Time oldval, Time newval)
{
    if (firstRtt)
    {
        firstRtt = false;
    }
    *rttStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval.GetSeconds()
                            << std::endl;
}

static void
RtoTracer(Time oldval, Time newval)
{
    if (firstRto)
    {
        firstRto = false;
    }
    *rtoStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval.GetSeconds()
                            << std::endl;
}

static void
RwndTracer(uint32_t oldval, uint32_t newval)
{
    if (firstRwnd)
    {
        firstRwnd = false;
    }
    *rwndStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

static void
AdvWNDTracer(uint32_t oldval, uint32_t newval)
{
    if (firstAdvWND)
    {
        firstAdvWND = false;
    }
    *advWNDStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

static void
HrackTracer(const SequenceNumber32 oldval, const SequenceNumber32 newval)
{
    if (firstHrack)
    {
        firstHrack = false;
    }
    *hrackStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval << std::endl;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void
TraceCwnd(std::string cwnd_tr_file_name)
{
    AsciiTraceHelper ascii;
    cWndStream = ascii.CreateFileStream((trFileDir + cwnd_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                  MakeCallback(&CwndTracer));
}

static void
TraceSsThresh(std::string ssthresh_tr_file_name)
{
    AsciiTraceHelper ascii;
    ssThreshStream = ascii.CreateFileStream((trFileDir + ssthresh_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
                                  MakeCallback(&SsThreshTracer));
}

static void
TraceRtt(std::string rtt_tr_file_name)
{
    AsciiTraceHelper ascii;
    rttStream = ascii.CreateFileStream((trFileDir + rtt_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/RTT",
                                  MakeCallback(&RttTracer));
}

static void
TraceRto(std::string rto_tr_file_name)
{
    AsciiTraceHelper ascii;
    rtoStream = ascii.CreateFileStream((trFileDir + rto_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/RTO",
                                  MakeCallback(&RtoTracer));
}

static void
TraceRwnd(std::string rwnd_tr_file_name)
{
    AsciiTraceHelper ascii;
    rwndStream = ascii.CreateFileStream((trFileDir + rwnd_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/10/$ns3::TcpL4Protocol/SocketList/0/RWND",
                                  MakeCallback(&RwndTracer));
}

static void
TraceAdvWND(std::string advWND_tr_file_name)
{
    AsciiTraceHelper ascii;
    advWNDStream = ascii.CreateFileStream((trFileDir + advWND_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/10/$ns3::TcpL4Protocol/SocketList/0/AdvWND",
                                  MakeCallback(&AdvWNDTracer));
}

// Highest ack received from peer
static void
TraceHighestRxAck(std::string hrack_tr_file_name)
{
    AsciiTraceHelper ascii;
    hrackStream = ascii.CreateFileStream((trFileDir + hrack_tr_file_name).c_str());
    Config::ConnectWithoutContext("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/HighestRxAck",
                                  MakeCallback(&HrackTracer));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
PrintProgress(Ptr<OutputStreamWrapper> stream_throughput)
{
    Time now_t = Simulator::Now();
    /* The first time the function is called we schedule the next call after
     * biDurationNs ns and then return*/
    if (*(lastReportTime) == MicroSeconds(0))
    {
        AsciiTraceHelper ascii;
        stream_throughput = ascii.CreateFileStream((trFileDir + thrputStream_tr_file_name).c_str());

        Simulator::Schedule(Seconds(0.25), PrintProgress, stream_throughput);
        lastRxMbits.resize(sinkApps.GetN(), 0.0);
        *(lastReportTime) = now_t;
        return;
    }

    *stream_throughput->GetStream() << now_t;
    *stream_throughput->GetStream() << std::fixed << std::setprecision(6);

    for (uint32_t appIdx = 0; appIdx < sinkApps.GetN(); appIdx++)
    {
        uint64_t rx_bytes;
        double cur_rx_Mbits;

        rx_bytes = DynamicCast<PacketSink>(sinkApps.Get(appIdx))
                       ->GetTotalRx(); // the total bytes received in this sink app
        cur_rx_Mbits = (rx_bytes * 8.0) / 1e6;

        *stream_throughput->GetStream() << " "
                                        << (cur_rx_Mbits - lastRxMbits[appIdx]) /
                                               ((now_t - *(lastReportTime)).GetNanoSeconds() / 1e9)
                                        << " ";

        lastRxMbits[appIdx] = cur_rx_Mbits;
    }

    *stream_throughput->GetStream() << std::endl;

    Simulator::Schedule(Seconds(0.25), PrintProgress, stream_throughput);

    *(lastReportTime) = now_t;
}

int
main(int argc, char* argv[])
{
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(false));
    Config::SetDefault("ns3::LteEnbPhy::NoiseFigure", DoubleValue(9.0)); // 增加噪声

    // 打开日志文件
    std::ofstream logFile("socketbase__log.txt");

    // 保存原标准输出流
    std::clog.rdbuf(logFile.rdbuf()); // 重定向到文件

    // 启用日志
    LogComponentEnable("TcpSocketBase", LogLevel(LOG_LEVEL_INFO));
    // LogComponentEnable("A3RsrpHandoverAlgorithm", LogLevel(LOG_LEVEL_INFO));

    // 打开日志文件
    // std::ofstream logFilecwnd("cwnd_log.txt");

    // 保存原标准输出流
    // std::clog.rdbuf(logFilecwnd.rdbuf());  // 重定向到文件

    // 启用日志
    // LogComponentEnable("TcpCongestionOps", LogLevel(LOG_LEVEL_INFO));

    // 使用 LOG_INFO 级别来记录日志
    // LogComponentEnable("TcpSocketBase", LogLevel(LOG_LEVEL_INFO));
    Config::SetDefault(
        "ns3::TcpL4Protocol::SocketType",
        StringValue("ns3::TcpCubic")); // change TcpNewReno to TcpWestwood/TcpVeno/TcpCubic/TcpBic
                                       // to implement Westwood,Veno,Cubic and Bic respectively.

    //============================================================================================================
    //for retrieve number of enbs
    ifstream EnbinFile("scratch/EnbLocations2.txt");
    int enblines; // counting number of lines in this file
    // string words ;               //store the word we are processing on
    if (!EnbinFile.is_open())
    {
        std::cerr << "Error: Could not open EnbLocations2.txt" << std::endl;
        return -1;
    }
    else
    {
        cout << "access txt" << endl;
    }

    enblines = count(istreambuf_iterator<char>(EnbinFile), // count line
                     istreambuf_iterator<char>(),
                     '\n');
    cout << enblines << endl; //----------------------------------------set node number here
    EnbinFile.close();

    ifstream Enbcin("scratch/EnbLocations2.txt");
    //=============================================================================================================

    uint16_t numberOfUes = 1;
    uint16_t numberOfEnbs = enblines;
    uint16_t numBearersPerUe = 1;
    // double distance = 500.0; // m
    // double yForUe = 500.0;   // m
    // double speed = 40;       // m/s
    // double simTime = (double)(numberOfEnbs + 1) * distance / speed; // 1500 m / 20 m/s = 75 secs
    double simTime = (double)30;
    double enbTxPowerDbm = 46.0;

    // Command line arguments
    unsigned int dlinterval = 300, dlpacketsize = 1024, ulinterval = 300, ulpacketsize = 1024;
    CommandLine cmd;
    cmd.AddValue("simTime", "Total duration of the simulation (in seconds)", simTime);
    cmd.AddValue("enbTxPowerDbm", "TX power [dBm] used by HeNBs (default = 46.0)", enbTxPowerDbm);
    cmd.AddValue("dlpacketSize", "DL packet size (bytes)", dlpacketsize);
    cmd.AddValue("dlpacketsInterval", "DL packet interval (ms)", dlinterval);
    cmd.AddValue("ulpacketSize", "UL packet size (bytes)", ulpacketsize);
    cmd.AddValue("ulpacketsInterval", "UL packet interval (ms)", ulinterval);
    // ── 切换感知 TCP 优化参数 ──
    cmd.AddValue("expCase",          "baseline / rwnd-only / ack-only / joint", g_expCase);
    cmd.AddValue("triggerSource",    "position / time / rsrp", g_triggerSource);
    cmd.AddValue("triggerTimesSec",  "逗号分隔绝对触发时刻(s)", g_triggerTimesSec);
    cmd.AddValue("triggerPositionsM","逗号分隔触发位置(m)", g_triggerPositionsM);
    cmd.AddValue("triggerAdvanceMs", "提前量(ms)", g_triggerAdvanceMs);
    cmd.AddValue("ackSplitCount",    "ACK拆分数量", g_ackSplitCount);
    cmd.AddValue("ackSplitK",        "ACK拆分公比系数", g_ackSplitK);
    cmd.AddValue("holdDurationMs",   "Hold阶段时长(ms)", g_holdDurationMs);
    cmd.AddValue("restoreDurationMs","恢复阶段时长(ms)", g_restoreDurationMs);
    cmd.AddValue("rwndAlphaFloor",   "rwnd最小缩放因子", g_rwndAlphaFloor);
    cmd.AddValue("rwndBeta",         "rwnd衰减速率β", g_rwndBeta);
    cmd.AddValue("rwndGamma",        "rwnd恢复速率γ", g_rwndGamma);
    cmd.Parse(argc, argv);

    // ── 根据 expCase 设置 rwnd/ackSplit 开关 ──
    if (g_expCase == "baseline")       { g_enableRwnd = false; g_enableAckSplit = false; }
    else if (g_expCase == "rwnd-only") { g_enableRwnd = true;  g_enableAckSplit = false; }
    else if (g_expCase == "ack-only")  { g_enableRwnd = false; g_enableAckSplit = true;  }
    else /* joint */                   { g_enableRwnd = true;  g_enableAckSplit = true;  }

    // 写入静态参数到 TcpSocketBase
    TcpSocketBase::s_rwndAlphaFloor = g_rwndAlphaFloor;
    TcpSocketBase::s_rwndBeta       = g_rwndBeta;
    TcpSocketBase::s_rwndGamma      = g_rwndGamma;

    std::cout << "=== Experiment: " << g_expCase
              << " | trigger: " << g_triggerSource
              << " | rwnd=" << g_enableRwnd
              << " | ackSplit=" << g_enableAckSplit << " ===" << std::endl;

    // change some default attributes so that they are reasonable for
    // this scenario, but do this before processing command line
    // arguments, so that the user is allowed to override these settings
    Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(500)));
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(4294967295));
    // Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (packetsize));
    // Config::SetDefault ("ns3::UdpClient::Interval", TimeValue(MilliSeconds (interval)));
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(false));
    // Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(enbTxPowerDbm));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    //=========================================================================================================setting
    //fading model
    lteHelper->SetFadingModel("ns3::TraceFadingLossModel");
    lteHelper->SetFadingModelAttribute(
        "TraceFilename",
        StringValue("src/lte/model/fading-traces/fading_trace_Highspeed_300kmph.fad"));
    // lteHelper->SetFadingModelAttribute("TraceLength", TimeValue(Seconds(10.0)));
    // lteHelper->SetFadingModelAttribute("SamplesNum", UintegerValue(1000));
    // lteHelper->SetFadingModelAttribute("WindowSize", TimeValue(Seconds(1.0)));
    // lteHelper->SetFadingModelAttribute("RbNum", UintegerValue(100));

    // 设置信道干扰和噪声
    Config::SetDefault("ns3::LteEnbPhy::NoiseFigure", DoubleValue(9.0)); // 增加噪声

    // 增加路径损耗
    Ptr<LogDistancePropagationLossModel> propagationLossModel =
        CreateObject<LogDistancePropagationLossModel>();
    propagationLossModel->SetReference(1.0, 46.0);
    propagationLossModel->SetPathLossExponent(3.5); // 增加衰减指数

    // Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();

    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");

    lteHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(3.0));
    lteHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(256)));

    // lteHelper->SetHandoverAlgorithmType("ns3::A2A4RsrqHandoverAlgorithm");
    // lteHelper->SetHandoverAlgorithmAttribute ("ServingCellThreshold", UintegerValue (30));
    // lteHelper->SetHandoverAlgorithmAttribute ("NeighbourCellOffset", UintegerValue (1));

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create a single RemoteHost
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer); // 配置服务器位置（例如坐标 (2000, 0, 0)）

    MobilityHelper remoteHostMobility;
    Ptr<ListPositionAllocator> remoteHostPositionAlloc = CreateObject<ListPositionAllocator>();
    remoteHostPositionAlloc->Add(Vector(0.0, 0.0, 0.0)); // 设置服务器坐标

    remoteHostMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    remoteHostMobility.SetPositionAllocator(remoteHostPositionAlloc);
    remoteHostMobility.Install(remoteHost); // 安装到服务器节点

    // Create the Internet
    PointToPointHelper p2ph;

    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("5Mb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.020)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // drop packets rate
    //  获取设备对象并设置错误模型

    // Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    // em->SetAttribute("ErrorRate", DoubleValue(0.01));
    // em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    // internetDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue (em));
    // internetDevices.Get(0)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

    // Routing of the Internet Host (towards the LTE network)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    // interface 0 is localhost, 1 is the p2p device
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    /*
     * Network topology:
     *
     *      |               d                   d                   d
     *      x--------------x--------------x--------------x--------------x--------------x--------------x
     *      |
     *      |                                 eNodeB              eNodeB
     *    y |+
     * ---------------------------------------------------------------------------------------> |UE
     *      |
     *      |
     *      |                                             d = distance
     *      o (0, 0, 0)
     */

    NodeContainer ueNodes;
    NodeContainer enbNodes;
    enbNodes.Create(numberOfEnbs);
    ueNodes.Create(numberOfUes);

    // p2ph.EnablePcap("tcp-high-speed", nodes.Get(1)->GetId(), 0, true); // 接收端
    string words;
    // Install Mobility Model in
    // eNB---------------------------------------------------------------------------------
    Ptr<ListPositionAllocator> enbPositionAlloc = CreateObject<ListPositionAllocator>();
    for (int i = 0; i < enblines; i++)
    {
        string enbtmp_ch, locationx, locationy; // for comparing
        getline(Enbcin, words);
        int xory = 0;
        for (unsigned int j = 0; j < words.length(); j++)
        {
            enbtmp_ch = words[j];

            if (enbtmp_ch == ":")
            {
                xory += 1;
                continue;
            }
            if (enbtmp_ch == " ")
            {
                if (xory != 0)
                {
                    Vector enbPosition(stof(locationx), stof(locationy), 0);
                    enbPositionAlloc->Add(enbPosition);
                    // ueWaypointMobility->AddWaypoint(Waypoint(Seconds(stof(time)),Vector(stof(locationx),stof(locationy),0)));
                    cout << locationx << " " << locationy
                         << endl; //----------------------deploy node in here
                }
                xory += 1;
                continue;
            }
            else if (xory == 1)
            {
                locationx.append(enbtmp_ch);
                // cout<<"location:"<<location<<endl;
            }
            else if (xory == 2)
            {
                locationy.append(enbtmp_ch);
            }
        }

        xory = 0;
        locationx.erase();
        locationy.erase();
    }

    MobilityHelper enbMobility;
    enbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    enbMobility.SetPositionAllocator(enbPositionAlloc);
    enbMobility.Install(enbNodes);

    MobilityHelper ueMobility;
    ueMobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    ueMobility.Install(ueNodes);

    // 设置移动速度为100km/h (100000 mm/s)
    for (uint32_t i = 0; i < ueNodes.GetN(); i++)
    {
        Ptr<ConstantVelocityMobilityModel> mobilityModel =
            DynamicCast<ConstantVelocityMobilityModel>(ueNodes.Get(i)->GetObject<MobilityModel>());
        if (mobilityModel)
        {
            // 设置初始位置为 (0, 300, 0)
            mobilityModel->SetPosition(Vector(0.0, 300.0, 0.0));
            // 设置速度，方向为沿 x轴正方向
            mobilityModel->SetVelocity(
                Vector(83.3, 0, 0)); // 100 km/h 转换成 m/s (100000/3600 ≈ 27.8)
        }
        cout << i << endl;
    }

    // Install LTE Devices in eNB and UEs
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // 确认设备安装成功
    if (enbLteDevs.GetN() > 0 && ueLteDevs.GetN() > 0)
    {
        std::cout << "LTE devices installed successfully." << std::endl;
    }
    else
    {
        std::cerr << "Failed to install LTE devices!" << std::endl;
    }
    p2ph.EnablePcap("high-speed", ueNodes.Get(0)->GetId(), 0, true); // 接收端
    p2ph.EnablePcapAll("tcp-high-speed");

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    NS_LOG_LOGIC("setting up applications");

    for (uint32_t i = 0; i < enbNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = enbNodes.Get(i)->GetObject<MobilityModel>();
        cout << "eNB Position: " << mobilityModel->GetPosition() << endl;
    }
    cout << enbNodes.GetN() << endl;

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<MobilityModel> mobilityModel = ueNodes.Get(i)->GetObject<MobilityModel>();
        cout << "UE Position: " << mobilityModel->GetPosition() << endl;
    }
    cout << ueNodes.GetN() << endl;
    /*
    TrafficControlHelper traffic;
    traffic.SetRootQueueDisc("ns3::RedQueueDisc",
                           "MinTh", DoubleValue(0.1),  // 10% 队列容量
                           "MaxTh", DoubleValue(0.3),
                           "MaxSize", QueueSizeValue(QueueSize("1KB")),
                           "LinkBandwidth", DataRateValue(DataRate("10Mbps")));
    traffic.Install(ueLteDevs.Get(0)); // 将流量控制应用到接收端设备
    */

    // Install and start applications on UEs and remote host
    uint16_t dlPort = 10000;
    uint16_t ulPort = 20000;

    // randomize a bit start times to avoid simulation artifacts
    // (e.g., buffer overflows due to packet transmissions happening
    // exactly at the same time)
    Ptr<UniformRandomVariable> startTimeSeconds = CreateObject<UniformRandomVariable>();
    // TCP needs to be started late enough so that all UEs are connected
    // otherwise TCP SYN packets will get lost
    startTimeSeconds->SetAttribute("Min", DoubleValue(0.100));
    startTimeSeconds->SetAttribute("Max", DoubleValue(0.110));

    lteHelper->AddX2Interface(enbNodes);

    for (uint32_t u = 0; u < numberOfUes; ++u)
    {
        Ptr<Node> ue = ueNodes.Get(u);
        // Set the default gateway for the UE
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

        // 打印静态路由表信息
        std::cout << "Static Routing Table for UE " << u << ":" << std::endl;
        if (ueStaticRouting)
        {
            // 输出静态路由信息
            // 在 NS-3 中静态路由的添加是手动配置的，通常你需要查看具体的条目。
            std::cout << "Default Route: " << ueStaticRouting->GetDefaultRoute() << std::endl;
        }
        else
        {
            std::cout << "No static routing found for this node." << std::endl;
        }

        for (uint32_t b = 0; b < numBearersPerUe; ++b)
        {
            ++dlPort;
            ++ulPort;
            cout << dlPort << endl;
            cout << ulPort << endl;
            NS_LOG_LOGIC("installing TCP DL app for UE " << u);
            BulkSendHelper dlClientHelper("ns3::TcpSocketFactory",
                                          InetSocketAddress(ueIpIfaces.GetAddress(u), dlPort));
            cout << "socket success" << endl;
            dlClientHelper.SetAttribute("SendSize", UintegerValue(1024)); // in bytes
            dlClientHelper.SetAttribute("MaxBytes", UintegerValue(4294967295));
            // OnOffHelper dlClientHelper ("ns3::TcpSocketFactory", InetSocketAddress
            // (ueIpIfaces.GetAddress (u), dlPort)); dlClientHelper.SetAttribute ("OffTime",
            // StringValue ("ns3::ConstantRandomVariable[Constant=0]")); dlClientHelper.SetAttribute
            // ("DataRate", ns3::DataRateValue(7.5*1000000));//in bps dlClientHelper.SetAttribute
            // ("PacketSize", UintegerValue (1024));//in bytes

            sourceApps.Add(dlClientHelper.Install(remoteHost));

            Ptr<PacketSink> source = DynamicCast<PacketSink>(sourceApps.Get(0));
            // source->SetAttribute ("EnableSeqTsSizeHeader",BooleanValue(true));

            PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory",
                                                InetSocketAddress(Ipv4Address::GetAny(), dlPort));

            sinkApps.Add(dlPacketSinkHelper.Install(ue));

            Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkApps.Get(0));
            // packetSink->SetAttribute ("EnableSeqTsSizeHeader",BooleanValue(true));

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

            Simulator::Schedule(Seconds(startTimeSeconds->GetValue() + 0.01),
                                &MyCheckSocketFunction,
                                packetSink);
            // if (source) {
            //   cout << "source success" << endl;
            // source->SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
            //}

            // if (packetSink) {
            //   cout << "sink success" << endl;
            //	packetSink->SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
            //}
            // 绑定回调
            // packetSink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));
            // packetSink->TraceConnectWithoutContext("RxWithAddresses",
            // MakeCallback(&RxWithAddressesCallback));
            // packetSink->TraceConnectWithoutContext("RxWithSeqTsSize",
            // MakeCallback(&RxWithSeqTsSizeCallback));

            Ptr<TcpL4Protocol> tcp = ue->GetObject<TcpL4Protocol>();
            if (tcp)
            {
                cout << "TCP protocol is initialized." << endl;
            }
        }
    }

    // Add X2 interface

    // X2-based Handover
    // lteHelper->HandoverRequest (Seconds (0.100), ueLteDevs.Get (0), enbLteDevs.Get (0),
    // enbLteDevs.Get (1));

    // Uncomment to enable PCAP tracing
    // p2ph.EnablePcapAll("lena-x2-handover-measures");

    // lteHelper->EnablePhyTraces();
    // lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();
    lteHelper->EnablePdcpTraces();
    Ptr<RadioBearerStatsCalculator> rlcStats = lteHelper->GetRlcStats();
    rlcStats->SetAttribute("EpochDuration", TimeValue(Seconds(0.001)));
    Ptr<RadioBearerStatsCalculator> pdcpStats = lteHelper->GetPdcpStats();
    pdcpStats->SetAttribute("EpochDuration", TimeValue(Seconds(0.001)));

    /******************************************
     *Setting up Flow Monitor
     *******************************************/
    Ptr<FlowMonitor> monitor;
    FlowMonitorHelper flowHelper;
    NodeContainer flowEnd2End = ueNodes;
    flowEnd2End.Add(remoteHost);
    monitor = flowHelper.Install(flowEnd2End);
    Simulator::Schedule(Seconds(0.1), &UpdateStats, monitor); // 调度每秒更新吞吐量

    std::cout << "Total nodes in flow monitor: " << flowEnd2End.GetN() << std::endl;
    std::cout << "Number of UE nodes: " << ueNodes.GetN() << std::endl;
    std::cout << "Number of eNB nodes: " << enbNodes.GetN() << std::endl;
    std::cout << "Number of remoteHost nodes: " << remoteHostContainer.GetN() << std::endl;

    lastReportTime = new Time(MicroSeconds(0));
    EnsureDirectoryExists(trFileDir);

    // for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i) {
    //   Ptr<Node> node = NodeList::GetNode(i);
    //   cout << "Node ID: " << i << endl;

    // 打印节点的IP地址
    //  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    //  if (ipv4) {
    //      for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
    //          for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k) {
    //             Ipv4Address addr = ipv4->GetAddress(j, k).GetLocal();
    //              cout << "Node IP Address: " << addr<< endl;
    //          }
    //      }
    //  }
    //}

    // std::string path = "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";
    // std::cout << "Connecting to: " << path << std::endl;
    // Config::Connect(path, MakeCallback(&CwndTracer));
    // Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*", BooleanValue(true));
    //  Get the TcpL4Protocol object from the node

    Simulator::Schedule(Seconds(0.11001), &TraceCwnd, cwnd_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceSsThresh, ssthresh_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceRtt, rtt_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceRto, rto_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceRwnd, rwnd_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceAdvWND, advWND_tr_file_name);
    Simulator::Schedule(Seconds(0.11001), &TraceHighestRxAck, hrack_tr_file_name);
    Simulator::Schedule(Seconds(0.2), &LogOptimTrace);  // 优化机制追踪
    // Simulator::Schedule(Seconds(3), PrintProgress, thrputStream);

    // connect custom trace sinks for RRC connection establishment and handover notification
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
    Config::Connect("/NodeList/*/DeviceList/*/LteUeRrc/ReportUeMeasurement",
                    MakeCallback(&NotifyUeMeasurement));

    // ── 设置 UE 节点 ID 并初始化触发 ──
    TcpSocketBase::s_ueNodeId = ueNodes.Get(0)->GetId();
    std::cout << "UE NodeId = " << TcpSocketBase::s_ueNodeId << std::endl;

    if (g_expCase != "baseline")
    {
        if (g_triggerSource == "position")
        {
            g_triggerPosList  = ParseDoubleList(g_triggerPositionsM);
            g_triggeredFlags.assign(g_triggerPosList.size(), false);
            Simulator::Schedule(Seconds(0.2), &CheckPositionTrigger, ueNodes.Get(0));
            std::cout << "[TRIGGER] position mode, points:";
            for (double p : g_triggerPosList) std::cout << " " << p;
            std::cout << std::endl;
        }
        else if (g_triggerSource == "time")
        {
            std::vector<double> times = ParseDoubleList(g_triggerTimesSec);
            for (double t : times)
            {
                Simulator::Schedule(Seconds(t), []() {
                    std::cout << Simulator::Now().GetSeconds()
                              << "s [TRIGGER] time-based" << std::endl;
                    DoOptimTrigger();
                });
            }
            std::cout << "[TRIGGER] time mode, times:";
            for (double t : times) std::cout << " " << t;
            std::cout << std::endl;
        }
        else if (g_triggerSource == "rsrp")
        {
            std::cout << "[TRIGGER] rsrp mode, threshold=-90dBm" << std::endl;
        }
    }

    Simulator::Stop(Seconds(simTime));

    AnimationInterface anim("lte-ananew.xml");
    anim.SetMaxPktsPerTraceFile(300000);
    anim.EnablePacketMetadata();
    // 只显示 UE 和基站
    for (uint32_t i = 0; i < numberOfUes; ++i)
    {
        anim.UpdateNodeColor(ueNodes.Get(i), 255, 0, 0); // 设置 UE 为红色
        anim.UpdateNodeSize(i, 15, 15);
        // cout << numberOfUes << endl;
    }

    anim.UpdateNodeColor(remoteHost, 0, 0, 255); // 设置 UE 为红色
    anim.UpdateNodeSize(0, 15, 15);
    // cout << numberOfUes << endl;

    for (uint32_t i = 0; i < enbNodes.GetN(); ++i)
    {
        anim.UpdateNodeColor(enbNodes.Get(i), 0, 255, 0); // 设置基站为绿色
        anim.UpdateNodeSize(i, 15, 15);
    }

    Simulator::Run();

    // GtkConfigStore config;
    // config.ConfigureAttributes ();
    // After run

    //	uint32_t numSinkApps = (cfg.downLink==true)?(cfg.dlSinkApps.GetN()):(cfg.ulSinkApps.GetN());
    for (uint32_t appIdx = 0; appIdx < sinkApps.GetN(); appIdx++)
    {
        Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(appIdx));
        // std::cout << " Packet sink " << appIdx << " Rx " << sink1->GetTotalRx () << " bytes" <<
        // std::endl; std::cout << std::endl;
    }

    OutputStats();
    OutputOptimTrace();
    OutputHandoverEvents();

    monitor->CheckForLostPackets();

    // 获取分类器，用于从 FlowId 反查 IP 地址
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        // 1. 获取流的五元组信息 (源IP, 目的IP, 端口等)
        // 这一步非常重要，否则你只知道 FlowId: 1，不知道是谁发给谁的
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "FlowId: " << i->first << " (" << t.sourceAddress << " -> "
                  << t.destinationAddress << ")\n";

        // 2. 基础统计
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";

        // 3. 计算丢包率 (Packet Loss Ratio)
        // lostPackets 是监控器认为丢失的，也可以用 (Tx - Rx) / Tx 来粗略估算
        double packetLossRate = 0.0;
        if (i->second.txPackets > 0)
        {
            packetLossRate = (double)i->second.lostPackets / (double)i->second.txPackets * 100.0;
        }
        std::cout << "  Lost Packets: " << i->second.lostPackets << " (" << packetLossRate
                  << "%)\n";

        // 4. 计算吞吐量 (Throughput) - Mbps
        // 只有接收到了包，计算吞吐量才有意义
        double throughput = 0.0;
        if (i->second.rxPackets > 0)
        {
            // 计算持续时间：最后接收时间 - 第一包发送时间 (也可以用 LastRx - FirstRx)
            Time duration = i->second.timeLastRxPacket - i->second.timeFirstTxPacket;
            if (duration.GetSeconds() > 0)
            {
                throughput = i->second.rxBytes * 8.0 / duration.GetSeconds() / 1024 / 1024;
            }
        }
        std::cout << "  Throughput: " << throughput << " Mbps\n";

        // 5. 计算平均时延 (Average Delay) - ms
        double avgDelay = 0.0;
        if (i->second.rxPackets > 0)
        {
            // delaySum 是 Time 对象，需要转换
            avgDelay = i->second.delaySum.GetSeconds() * 1000 / i->second.rxPackets;
        }
        std::cout << "  Avg Delay:  " << avgDelay << " ms\n";

        // 6. 计算平均抖动 (Average Jitter) - ms
        double avgJitter = 0.0;
        if (i->second.rxPackets > 1)
        { // 至少收到两个包才能算抖动
            avgJitter = i->second.jitterSum.GetSeconds() * 1000 / (i->second.rxPackets - 1);
        }
        std::cout << "  Avg Jitter: " << avgJitter << " ms\n";

        // 7. [进阶] 查看具体的丢包原因
        // 这能帮你定位是队列溢出(Congestion)还是链路错误(PHY error)
        if (i->second.packetsDropped.size() > 0)
        {
            std::cout << "  Drop Reasons:\n";
            for (uint32_t reasonCode = 0; reasonCode < i->second.packetsDropped.size();
                 reasonCode++)
            {
                if (i->second.packetsDropped[reasonCode] > 0)
                {
                    // 将 reasonCode 转换为可读字符串 (NS-3 提供了工具函数，或者手动对应)
                    // 这里简单打印 Code 和 数量
                    std::cout << "    Code " << reasonCode << ": "
                              << i->second.packetsDropped[reasonCode] << " pkts\n";

                    // 常见的 Code (仅供参考，具体取决于安装的 FlowProbe):
                    // Ipv4FlowProbe::DROP_NO_ROUTE = 0
                    // Ipv4FlowProbe::DROP_TTL_EXPIRE = 1
                    // Ipv4FlowProbe::DROP_BAD_CHECKSUM = 2
                    // Ipv4FlowProbe::DROP_QUEUE = 3 (最常见，代表拥塞丢包)
                }
            }
        }

        std::cout << "\n";
    }
    std::cout << "\n";

    monitor->SerializeToXmlFile("lte-grid.xml", true, true);

    lteHelper = 0;
    Simulator::Destroy();
    return 0;
}
