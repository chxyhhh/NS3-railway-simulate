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
#include "ns3/flow-monitor-helper.h" // 引入 FlowMonitor 模块
#include "ns3/netanim-module.h"
//================================-------------------------------------
#include <iostream>
#include <fstream>
#include <algorithm>
#include <string>
#include <iomanip>
#include <ios>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
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
std::string trFileDir = "./traces/";////////////!!!!!!!!!!!!!!!!!!!not complet3

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
//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>.

ApplicationContainer sourceApps;
ApplicationContainer sinkApps;
Time *lastReportTime;
std::vector <double> lastRxMbits;


Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

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
  // lteHelper->Attach (ues, enbs.Get (1));
  std::cout << "attach" << std::endl;
}

static void RxDrop (Ptr<const Packet> p)  //丢包 回调函数
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}

void TraceTcpDupAck (std::string context, uint32_t seqNo, uint32_t ackNo)
{
    std::cout << "Dup ACK: " << "seqNo = " << seqNo << ", ackNo = " << ackNo
              << " at time " << Simulator::Now ().GetSeconds () << "s" << std::endl;
}

void TraceTcpRetransmission (std::string context, uint32_t seqNo, uint32_t ackNo)
{
    std::cout << "TCP Retransmission: " << "seqNo = " << seqNo << ", ackNo = " << ackNo
              << " at time " << Simulator::Now ().GetSeconds () << "s" << std::endl;
}

void MyCheckSocketFunction(Ptr<PacketSink> packetSink)
{
    Ptr<Socket> socket = packetSink->GetListeningSocket();
    if (socket)
    {
        //std::cout << "m_socket success" << std::endl;
        Ptr<TcpSocket> tcpSocket = DynamicCast<TcpSocket>(socket);
        if (tcpSocket)
        {
            Ptr<TcpSocketBase> tcpSocketBase = DynamicCast<TcpSocketBase>(tcpSocket);
            if(tcpSocketBase){
            	if(isHandoverStart){
                //cout << "!!!handover start!!!" << endl;
            	tcpSocketBase->SetHandoverStatus(isHandoverStart); // 设置为切换中
            	Simulator::Schedule(MilliSeconds(100), [=]() {
                    isHandoverStart = false;
                });
            	//isHandoverStart = false;
            }
            //std::cout << "Successfully casted to TcpSocket" << std::endl;
            //tcpSocket->SetAttribute("RcvBufSize", UintegerValue(32768*4));
            //NS_LOG_INFO("Successfully set RcvBufSize to 32768.");
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

void
NotifyConnectionEstablishedUe (std::string context,
                              uint64_t imsi,
                              uint16_t cellid,
                              uint16_t rnti)
{
  std::cout << "at "<<Simulator::Now ().GetSeconds () << "s "<< context
            << " UE IMSI " << imsi
            << ": connected to CellId " << cellid
            << " with RNTI " << rnti
            << std::endl;
}

void
NotifyHandoverStartUe (std::string context,
                      uint64_t imsi,
                      uint16_t cellid,
                      uint16_t rnti,
                      uint16_t targetCellId)
{
  //isHandoverStart = true;
  std::cout <<Simulator::Now ().GetSeconds () << "s "<< context
            << " UE IMSI " << imsi
            << ": previously connected to CellId " << cellid
            << " with RNTI " << rnti
            << ", doing handover to CellId " << targetCellId
            << std::endl;
  std::cout << BooleanValue(isHandoverStart) << std::endl;
}

void
NotifyHandoverEndOkUe (std::string context,
                      uint64_t imsi,
                      uint16_t cellid,
                      uint16_t rnti)
{
  //isHandoverStart = false;
  std::cout <<Simulator::Now ().GetSeconds () << "s "<< context
            << " UE IMSI " << imsi
            << ": successful handover to CellId " << cellid
            << " with RNTI " << rnti
            << std::endl;
  std::cout << BooleanValue(isHandoverStart) << std::endl;
}

void
NotifyConnectionEstablishedEnb (std::string context,
                                uint64_t imsi,
                                uint16_t cellid,
                                uint16_t rnti)
{
  std::cout <<Simulator::Now ().GetSeconds () << "s "<< context
            << " eNB CellId " << cellid
            << ": successful connection of UE with IMSI " << imsi
            << " RNTI " << rnti
            << std::endl;
}

void
NotifyHandoverStartEnb (std::string context,
                        uint64_t imsi,
                        uint16_t cellid,
                        uint16_t rnti,
                        uint16_t targetCellId)
{
  std::cout <<Simulator::Now ().GetSeconds () << "s "<< context
            << " eNB CellId " << cellid
            << ": start handover of UE with IMSI " << imsi
            << " RNTI " << rnti
            << " to CellId " << targetCellId
            << std::endl;
  std::cout << BooleanValue(isHandoverStart) << std::endl;
}

void
NotifyHandoverEndOkEnb (std::string context,
                        uint64_t imsi,
                        uint16_t cellid,
                        uint16_t rnti)
{
  std::cout <<Simulator::Now ().GetSeconds () << "s "<< context
            << " eNB CellId " << cellid
            << ": completed handover of UE with IMSI " << imsi
            << " RNTI " << rnti
            << std::endl;
}

// 捕获UE的测量报告
void NotifyUeMeasurement(std::string context, uint64_t imsi, uint16_t cellId, uint16_t rnti, LteRrcSap::MeasurementReport report) {
    // 提取服务小区RSRP（单位：dBm）
    //if (!report.empty()) {
        //double rsrp_dBm = EutranMeasurementMapping::RsrpRange2Dbm(report.measResults.measResultPCell.rsrpResult);

        // 添加到预测器
        //g_predictor.AddRsrpMeasurement(rsrp_dBm);

        // 打印实时数据（调试用）
//        std::cout << "11111111111" << std::endl;
        double rsrpResults = (double)EutranMeasurementMapping::RsrpRange2Dbm(report.measResults.measResultPCell.rsrpResult);
        std::cout << Simulator::Now().GetSeconds () << "s " << " UE IMSI=" << imsi << " 服务小区RSRP: " <<
                                        rsrpResults << " dBm" << std::endl;
        if(rsrpResults < -90){
            isHandoverStart = true;
            //std::cout << "==========rsrp lower===========" << std::endl;

        }
    //}
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
std::vector<double> rttData; // RTT 数据
std::vector<double> packetLossData; // 丢包率数据

double prevTxBytes = 0; // 上次统计的发送字节数
double prevRxBytes = 0;

// 定义每个数据包接收时更新 RTT 的回调函数
void UpdateRTT(Ptr<const Packet> packet, const Address &from, const Address &to) {
    // 获取当前时间
    Time currentTime = Simulator::Now();

    // 计算 RTT（这里只是示意，实际可以根据你发送的包的时间戳进行计算）
    // 如果你能够获得包的时间戳信息，应该通过当前时间减去包的发送时间
    double rtt = currentTime.GetSeconds() * 1000; // 转换为毫秒

    // 存储 RTT 值
    rttData.push_back(rtt);
}

// 定义更新吞吐量和丢包率的回调函数
void UpdateStats(Ptr<FlowMonitor> monitor) {
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double currentTxBytes = 0;
    double currentRxBytes = 0;
    double totalPacketsSent = 0;
    double totalPacketsReceived = 0;
    double totalPacketsLost = 0;

    // 遍历所有流量统计信息
    for (auto it = stats.begin(); it != stats.end(); ++it) {
        currentTxBytes += it->second.txBytes;
        currentRxBytes += it->second.rxBytes;
        totalPacketsSent += it->second.txPackets;
        totalPacketsReceived += it->second.rxPackets;
        totalPacketsLost += it->second.lostPackets;
    }

    // 计算吞吐量（以 Mbps 为单位）
    double txThroughputBytes = currentTxBytes - prevTxBytes; // 当前时间窗口的发送字节增量
    double txThroughputMbps = (txThroughputBytes * 8) / (0.1 * 1e6); // 转换为 Mbps

    double rxThroughputBytes = currentRxBytes - prevRxBytes; // 当前时间窗口的发送字节增量
    double rxThroughputMbps = (rxThroughputBytes * 8) / (0.1 * 1e6); // 转换为 Mbps

    txThroughputData.push_back(txThroughputMbps); // 保存吞吐量数据
    rxThroughputData.push_back(rxThroughputMbps); // 保存吞吐量数据
    prevTxBytes = currentTxBytes;
    prevRxBytes = currentRxBytes;

    // 计算丢包率
    double packetLossRate = 0;
    if (totalPacketsSent > 0) {
        packetLossRate = (totalPacketsLost / totalPacketsSent) * 100; // 丢包率百分比
    }
    packetLossData.push_back(packetLossRate);

    // 每 0.1 秒更新一次
    Simulator::Schedule(Seconds(0.1), &UpdateStats, monitor);
}

// 输出吞吐量、RTT 和丢包率到文件
void OutputStats() {
    std::ofstream txThroughputFile("txThroughput_data.txt");
    for (double throughput : txThroughputData) {
        txThroughputFile << throughput << std::endl; // 每行输出一个吞吐量值
    }
    txThroughputFile.close();

    std::ofstream rxThroughputFile("rxThroughput_data.txt");
    for (double throughput : rxThroughputData) {
        rxThroughputFile << throughput << std::endl; // 每行输出一个吞吐量值
    }
    rxThroughputFile.close();

    std::ofstream packetLossFile("packet_loss_data.txt");
    for (double lossRate : packetLossData) {
        packetLossFile << lossRate << std::endl; // 每行输出一个丢包率值
    }
    packetLossFile.close();
}
///////////////////////////////////////////////Tracers////////////////////////////////////////////////////////
	static void
CwndTracer (uint32_t oldval, uint32_t newval)
{
	if (firstCwnd)
	{
		firstCwnd = false;
	}
	*cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
	cWndValue = newval;

	if (!firstSshThr)
	{
		*ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << ssThreshValue << std::endl;
	}
}

	static void
SsThreshTracer (uint32_t oldval, uint32_t newval)
{
	if (firstSshThr)
	{
		firstSshThr = false;
	}
	*ssThreshStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
	ssThreshValue = newval;

	if (!firstCwnd)
	{
		*cWndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << cWndValue << std::endl;
	}
}

	static void
RttTracer (Time oldval, Time newval)
{
	if (firstRtt)
	{
		firstRtt = false;
	}
	*rttStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

	static void
RtoTracer (Time oldval, Time newval)
{
	if (firstRto)
	{
		firstRto = false;
	}
	*rtoStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval.GetSeconds () << std::endl;
}

	static void
RwndTracer (uint32_t oldval, uint32_t newval)
{
	if (firstRwnd)
	{
		firstRwnd = false;
	}
	*rwndStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
}

	static void
AdvWNDTracer (uint32_t oldval, uint32_t newval)
{
	if (firstAdvWND)
	{
		firstAdvWND = false;
	}
	*advWNDStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
}


	static void
HrackTracer (const SequenceNumber32 oldval, const SequenceNumber32 newval)
{
	if (firstHrack)
	{
		firstHrack = false;
	}
	*hrackStream->GetStream () << Simulator::Now ().GetSeconds () << " " << newval << std::endl;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	static void
TraceCwnd (std::string cwnd_tr_file_name)
{
	AsciiTraceHelper ascii;
	cWndStream = ascii.CreateFileStream ((trFileDir + cwnd_tr_file_name).c_str ());
	Config::ConnectWithoutContext ("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback (&CwndTracer));

}

	static void
TraceSsThresh (std::string ssthresh_tr_file_name)
{
	AsciiTraceHelper ascii;
	ssThreshStream = ascii.CreateFileStream ((trFileDir + ssthresh_tr_file_name).c_str ());
	Config::ConnectWithoutContext ("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold", MakeCallback (&SsThreshTracer));
}

	static void
TraceRtt (std::string rtt_tr_file_name)
{
	AsciiTraceHelper ascii;
	rttStream = ascii.CreateFileStream (rtt_tr_file_name.c_str ());
	Config::ConnectWithoutContext ("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/RTT", MakeCallback (&RttTracer));
}

	static void
TraceRto (std::string rto_tr_file_name)
{
	AsciiTraceHelper ascii;
	rtoStream = ascii.CreateFileStream ((trFileDir + rto_tr_file_name).c_str ());
	Config::ConnectWithoutContext ("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/RTO", MakeCallback (&RtoTracer));
}

	static void
TraceRwnd (std::string rwnd_tr_file_name)
{
	AsciiTraceHelper ascii;
	rwndStream = ascii.CreateFileStream ((trFileDir + rwnd_tr_file_name).c_str ());
	Config::ConnectWithoutContext ("/NodeList/11/$ns3::TcpL4Protocol/SocketList/0/RWND", MakeCallback (&RwndTracer));
}

	static void
TraceAdvWND (std::string advWND_tr_file_name)
{
	AsciiTraceHelper ascii;
	advWNDStream = ascii.CreateFileStream ((trFileDir + advWND_tr_file_name).c_str ());
	Config::ConnectWithoutContext ("/NodeList/11/$ns3::TcpL4Protocol/SocketList/0/AdvWND", MakeCallback (&AdvWNDTracer));
}

// Highest ack received from peer
	static void
TraceHighestRxAck (std::string hrack_tr_file_name)
{
	AsciiTraceHelper ascii;
	hrackStream = ascii.CreateFileStream ((trFileDir + hrack_tr_file_name).c_str ());
	Config::ConnectWithoutContext ("/NodeList/3/$ns3::TcpL4Protocol/SocketList/0/HighestRxAck", MakeCallback (&HrackTracer));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PrintProgress(Ptr<OutputStreamWrapper> stream_throughput)
{
	Time now_t = Simulator::Now ();
	/* The first time the function is called we schedule the next call after
	 * biDurationNs ns and then return*/
	if (*(lastReportTime) == MicroSeconds(0)) {
		AsciiTraceHelper ascii;
		stream_throughput = ascii.CreateFileStream ((trFileDir+ thrputStream_tr_file_name).c_str ());

		Simulator::Schedule(Seconds(0.25), PrintProgress, stream_throughput);
		lastRxMbits.resize(sinkApps.GetN(), 0.0);
		*(lastReportTime) = now_t;
		return;
	}

	*stream_throughput->GetStream() << now_t;
	*stream_throughput->GetStream() << std::fixed << std::setprecision(6);

	for (uint32_t appIdx = 0; appIdx < sinkApps.GetN(); appIdx++ ){
		uint64_t rx_bytes;
		double cur_rx_Mbits;

		rx_bytes = DynamicCast<PacketSink>(sinkApps.Get(appIdx))->GetTotalRx ();//the total bytes received in this sink app
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

    // 打开日志文件
    std::ofstream logFile("handover_predic_log.txt");

    // 保存原标准输出流
    std::clog.rdbuf(logFile.rdbuf());  // 重定向到文件

    // 启用日志
//    LogComponentEnable("TcpSocketBase", LogLevel(LOG_LEVEL_INFO));
    LogComponentEnable("HandoverPredictor", LogLevel(LOG_LEVEL_INFO));
    //LogComponentEnable("A3RsrpHandoverAlgorithm", LogLevel(LOG_LEVEL_INFO));

    // 打开日志文件
    //std::ofstream logFilecwnd("cwnd_log.txt");

    // 保存原标准输出流
    //std::clog.rdbuf(logFilecwnd.rdbuf());  // 重定向到文件

    // 启用日志
    //LogComponentEnable("TcpCongestionOps", LogLevel(LOG_LEVEL_INFO));

    // 使用 LOG_INFO 级别来记录日志
    //LogComponentEnable("TcpSocketBase", LogLevel(LOG_LEVEL_INFO));
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpCubic")); //change TcpNewReno to TcpWestwood/TcpVeno/TcpCubic/TcpBic to implement Westwood,Veno,Cubic and Bic respectively.

  //============================================================================================================ for retrieve number of enbs
  ifstream EnbinFile("scratch/EnbLocations2.txt");
  int enblines; // counting number of lines in this file
  // string words ;               //store the word we are processing on
  if (!EnbinFile.is_open()) {
  	std::cerr << "Error: Could not open EnbLocations2.txt" << std::endl;
	return -1;
  }else{
	cout << "access txt" << endl;
  }

  enblines = count(istreambuf_iterator<char>(EnbinFile), // count line
                   istreambuf_iterator<char>(), '\n');
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
  // cmd.AddValue ("speed", "Speed of the UE (default = 20 m/s)", speed);
  cmd.AddValue("enbTxPowerDbm", "TX power [dBm] used by HeNBs (default = 46.0)", enbTxPowerDbm);
  cmd.AddValue("dlpacketSize", "Size (bytes) of packets generated (default = 1024 bytes). The minimum packet size is 12 bytes which is the size of the header carrying the sequence number and the time stamp.", dlpacketsize);
  cmd.AddValue("dlpacketsInterval", "The time (ms) wait between packets (default = 500 ms)", dlinterval);
  cmd.AddValue("ulpacketSize", "Size (bytes) of packets generated (default = 1024 bytes). The minimum packet size is 12 bytes which is the size of the header carrying the sequence number and the time stamp.", ulpacketsize);
  cmd.AddValue("ulpacketsInterval", "The time (ms) wait between packets (default = 500 ms)", ulinterval);
  cmd.Parse(argc, argv);

  // change some default attributes so that they are reasonable for
  // this scenario, but do this before processing command line
  // arguments, so that the user is allowed to override these settings
  Config::SetDefault("ns3::UdpClient::Interval", TimeValue(MilliSeconds(500)));
  Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(4294967295));
  // Config::SetDefault ("ns3::UdpClient::PacketSize", UintegerValue (packetsize));
  // Config::SetDefault ("ns3::UdpClient::Interval", TimeValue(MilliSeconds (interval)));
  Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(false));
  //Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(enbTxPowerDbm));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

  //=========================================================================================================setting fading model
  lteHelper->SetFadingModel("ns3::TraceFadingLossModel");
  lteHelper->SetFadingModelAttribute("TraceFilename", StringValue("src/lte/model/fading-traces/fading_trace_Highspeed_300kmph.fad"));
  //lteHelper->SetFadingModelAttribute("TraceLength", TimeValue(Seconds(10.0)));
  //lteHelper->SetFadingModelAttribute("SamplesNum", UintegerValue(1000));
  //lteHelper->SetFadingModelAttribute("WindowSize", TimeValue(Seconds(1.0)));
  //lteHelper->SetFadingModelAttribute("RbNum", UintegerValue(100));

  // 设置信道干扰和噪声
  Config::SetDefault("ns3::LteEnbPhy::NoiseFigure", DoubleValue(9.0));  // 增加噪声

  // 增加路径损耗
  Ptr<LogDistancePropagationLossModel> propagationLossModel = CreateObject<LogDistancePropagationLossModel>();
  propagationLossModel->SetReference(1.0, 46.0);
  propagationLossModel->SetPathLossExponent(3.5);  // 增加衰减指数

  // Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();

  lteHelper->SetEpcHelper(epcHelper);
  lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

  lteHelper->SetHandoverAlgorithmType("ns3::A3RsrpHandoverAlgorithm");

  lteHelper->SetHandoverAlgorithmAttribute("Hysteresis", DoubleValue(3.0));
  lteHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(256)));

  //lteHelper->SetHandoverAlgorithmType("ns3::A2A4RsrqHandoverAlgorithm");
  //lteHelper->SetHandoverAlgorithmAttribute ("ServingCellThreshold", UintegerValue (30));
  //lteHelper->SetHandoverAlgorithmAttribute ("NeighbourCellOffset", UintegerValue (1));

  Ptr<Node> pgw = epcHelper->GetPgwNode();


  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create(1);
  Ptr<Node> remoteHost = remoteHostContainer.Get(0);
  InternetStackHelper internet;
  internet.Install(remoteHostContainer);// 配置服务器位置（例如坐标 (2000, 0, 0)）

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

  //drop packets rate
  // 获取设备对象并设置错误模型

  //Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    //em->SetAttribute("ErrorRate", DoubleValue(0.01));
    //em->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    //internetDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue (em));
    //internetDevices.Get(0)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));

  // Routing of the Internet Host (towards the LTE network)
  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
  // interface 0 is localhost, 1 is the p2p device
  remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

  /*
   * Network topology:
   *
   *      |               d                   d                   d
   *      x--------------x--------------x--------------x--------------x--------------x--------------x
   *      |
   *      |                                 eNodeB              eNodeB
   *    y |+ --------------------------------------------------------------------------------------->
   *      |UE
   *      |
   *      |
   *      |                                             d = distance
   *      o (0, 0, 0)
   */

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create(numberOfEnbs);
  ueNodes.Create(numberOfUes);

  //p2ph.EnablePcap("tcp-high-speed", nodes.Get(1)->GetId(), 0, true); // 接收端
  string words ;
  // Install Mobility Model in eNB---------------------------------------------------------------------------------
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
          cout << locationx << " " << locationy << endl; //----------------------deploy node in here
        }
        xory += 1;
        continue;
      }
      else if (xory == 1)
      {
        locationx.append(enbtmp_ch);
        //cout<<"location:"<<location<<endl;
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
    Ptr<ConstantVelocityMobilityModel> mobilityModel = DynamicCast<ConstantVelocityMobilityModel>(ueNodes.Get(i)->GetObject<MobilityModel>());
    if (mobilityModel)
    {
      // 设置初始位置为 (0, 300, 0)
      mobilityModel->SetPosition(Vector(0.0, 300.0, 0.0));
      // 设置速度，方向为沿 x轴正方向
      mobilityModel->SetVelocity(Vector(83.3, 0, 0)); // 100 km/h 转换成 m/s (100000/3600 ≈ 27.8)
    }
    cout << i << endl;
  }

  // Install LTE Devices in eNB and UEs
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

  // 确认设备安装成功
  if (enbLteDevs.GetN() > 0 && ueLteDevs.GetN() > 0) {
      std::cout << "LTE devices installed successfully." << std::endl;
  } else {
      std::cerr << "Failed to install LTE devices!" << std::endl;
  }
  p2ph.EnablePcap("high-speed", ueNodes.Get(0)->GetId(), 0, true); // 接收端
  p2ph.EnablePcapAll("tcp-high-speed");

  // Install the IP stack on the UEs
  internet.Install(ueNodes);
  Ipv4InterfaceContainer ueIpIfaces;
  ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

  lteHelper->Attach (ueLteDevs.Get (0), enbLteDevs.Get (0));

  NS_LOG_LOGIC("setting up applications");

  for (uint32_t i = 0; i < enbNodes.GetN(); ++i){
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
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // 打印静态路由表信息
    std::cout << "Static Routing Table for UE " << u << ":" << std::endl;
    if (ueStaticRouting) {
        // 输出静态路由信息
        // 在 NS-3 中静态路由的添加是手动配置的，通常你需要查看具体的条目。
        std::cout << "Default Route: " << ueStaticRouting->GetDefaultRoute() << std::endl;
    } else {
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
      dlClientHelper.SetAttribute ("MaxBytes", UintegerValue (4294967295));
      //OnOffHelper dlClientHelper ("ns3::TcpSocketFactory", InetSocketAddress (ueIpIfaces.GetAddress (u), dlPort));
      //dlClientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
      //dlClientHelper.SetAttribute ("DataRate", ns3::DataRateValue(7.5*1000000));//in bps
      //dlClientHelper.SetAttribute ("PacketSize", UintegerValue (1024));//in bytes

      sourceApps.Add(dlClientHelper.Install(remoteHost));

      Ptr<PacketSink> source = DynamicCast<PacketSink>(sourceApps.Get(0));
      //source->SetAttribute ("EnableSeqTsSizeHeader",BooleanValue(true));

      PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));

      sinkApps.Add(dlPacketSinkHelper.Install(ue));

      Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkApps.Get(0));
      //packetSink->SetAttribute ("EnableSeqTsSizeHeader",BooleanValue(true));

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
      //if (source) {
      //  cout << "source success" << endl;
    	//source->SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
	//}

      //if (packetSink) {
      //  cout << "sink success" << endl;
    //	packetSink->SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
	//}
      // 绑定回调
      //packetSink->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));
      //packetSink->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxWithAddressesCallback));
      //packetSink->TraceConnectWithoutContext("RxWithSeqTsSize", MakeCallback(&RxWithSeqTsSizeCallback));

      Ptr<TcpL4Protocol> tcp = ue->GetObject<TcpL4Protocol>();
      if (tcp) {
         cout << "TCP protocol is initialized." << endl;
      }


    }
  }

  // Add X2 interface

  // X2-based Handover
  // lteHelper->HandoverRequest (Seconds (0.100), ueLteDevs.Get (0), enbLteDevs.Get (0), enbLteDevs.Get (1));

  // Uncomment to enable PCAP tracing
  // p2ph.EnablePcapAll("lena-x2-handover-measures");

  //lteHelper->EnablePhyTraces();
  //lteHelper->EnableMacTraces();
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

  //for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i) {
  //  Ptr<Node> node = NodeList::GetNode(i);
  //  cout << "Node ID: " << i << endl;

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

  //std::string path = "/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow";
  //std::cout << "Connecting to: " << path << std::endl;
  //Config::Connect(path, MakeCallback(&CwndTracer));
  //Config::Set("/NodeList/0/$ns3::TcpL4Protocol/SocketList/*", BooleanValue(true));
  // Get the TcpL4Protocol object from the node


  Simulator::Schedule(Seconds(0.11001), &TraceCwnd, cwnd_tr_file_name);
  Simulator::Schedule(Seconds(0.11001), &TraceSsThresh, ssthresh_tr_file_name);
  Simulator::Schedule(Seconds(0.11001), &TraceRtt, rtt_tr_file_name);
  Simulator::Schedule(Seconds(0.11001), &TraceRto, rto_tr_file_name);
  Simulator::Schedule(Seconds(0.11001), &TraceRwnd, rwnd_tr_file_name);
  Simulator::Schedule(Seconds(0.11001), &TraceAdvWND, advWND_tr_file_name);
  Simulator::Schedule(Seconds(0.11001), &TraceHighestRxAck, hrack_tr_file_name);
  //Simulator::Schedule(Seconds(3), PrintProgress, thrputStream);

  // connect custom trace sinks for RRC connection establishment and handover notification
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/ConnectionEstablished",
                  MakeCallback (&NotifyConnectionEstablishedEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ConnectionEstablished",
                  MakeCallback (&NotifyConnectionEstablishedUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverStart",
                  MakeCallback (&NotifyHandoverStartEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverStart",
                  MakeCallback (&NotifyHandoverStartUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteEnbRrc/HandoverEndOk",
                  MakeCallback (&NotifyHandoverEndOkEnb));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/HandoverEndOk",
                  MakeCallback (&NotifyHandoverEndOkUe));
  Config::Connect ("/NodeList/*/DeviceList/*/LteUeRrc/ReportUeMeasurement",
                  MakeCallback (&NotifyUeMeasurement));

  //Config::ListPaths();
  //logFile.close();




  Simulator::Stop(Seconds(60.0));

  AnimationInterface anim("lte-ananew.xml");
  anim.SetMaxPktsPerTraceFile(300000);
  anim.EnablePacketMetadata();
  // 只显示 UE 和基站
for (uint32_t i = 0; i < numberOfUes; ++i) {
    anim.UpdateNodeColor(ueNodes.Get(i), 255, 0, 0);  // 设置 UE 为红色
    anim.UpdateNodeSize (i, 15, 15);
    //cout << numberOfUes << endl;

}


    anim.UpdateNodeColor(remoteHost, 0, 0, 255);  // 设置 UE 为红色
    anim.UpdateNodeSize (0, 15, 15);
    //cout << numberOfUes << endl;


for (uint32_t i = 0; i < enbNodes.GetN(); ++i) {
    anim.UpdateNodeColor(enbNodes.Get(i), 0, 255, 0);  // 设置基站为绿色
    anim.UpdateNodeSize (i, 15, 15);
}

  Simulator::Run();

  // GtkConfigStore config;
  // config.ConfigureAttributes ();
  // After run

  //	uint32_t numSinkApps = (cfg.downLink==true)?(cfg.dlSinkApps.GetN()):(cfg.ulSinkApps.GetN());
  for (uint32_t appIdx = 0; appIdx < sinkApps.GetN(); appIdx++)
  {

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(appIdx));
    // std::cout << " Packet sink " << appIdx << " Rx " << sink1->GetTotalRx () << " bytes" << std::endl;
    // std::cout << std::endl;
  }

  OutputStats();
  monitor->TraceConnectWithoutContext("RxPacket", MakeCallback(&UpdateRTT));

  monitor->CheckForLostPackets();
  double throughput = 0.0;

  //Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
  {
    std::cout << "FlowId:" << i->first << "\n";
    std::cout << "TxBytes:" << i->second.txBytes << "\t\t";
    std::cout << "RxBytes:" << i->second.rxBytes << "\t\t";
    std::cout << "lostPackets:" << i->second.lostPackets << "\t\t";
    //			std::cout << " bytesDropped " << i->second.bytesDropped	 << "\t";

    throughput = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024;
    std::cout << "throughput:" << throughput << " \n\n";

  }
  std::cout << "\n";

  monitor->SerializeToXmlFile("lte-grid.xml", true, true);

  lteHelper = 0;
  Simulator::Destroy();
  return 0;
}

