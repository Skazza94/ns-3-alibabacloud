#include <ns3/simulator.h>
#include <ns3/simple-seq-ts-header.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include "ns3/ppp-header.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/pointer.h"
#include "ns3/priority-tag.h"
#include "rdma-hw.h"
#include "ppp-header.h"
#include "qbb-header.h"
#include "cn-header.h"
#ifdef NS3_MTP
#include "ns3/mtp-interface.h"
#endif
#include <iostream>	// debug

namespace ns3{

TypeId RdmaHw::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaHw")
		.SetParent<Object> ()
		.AddAttribute("MinRate",
				"Minimum rate of a throttled flow",
				DataRateValue(DataRate("100Mb/s")),
				MakeDataRateAccessor(&RdmaHw::m_minRate),
				MakeDataRateChecker())
		.AddAttribute("Mtu",
				"Mtu.",
				UintegerValue(1000),
				MakeUintegerAccessor(&RdmaHw::m_mtu),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute ("CcMode",
				"which mode of DCQCN is running",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::m_cc_mode),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("NACKGenerationInterval",
				"The NACK Generation interval",
				DoubleValue(500.0),
				MakeDoubleAccessor(&RdmaHw::m_nack_interval),
				MakeDoubleChecker<double>())
		.AddAttribute("L2ChunkSize",
				"Layer 2 chunk size. Disable chunk mode if equals to 0.",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::m_chunk),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("L2AckInterval",
				"Layer 2 Ack intervals. Disable ack if equals to 0.",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::m_ack_interval),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("L2BackToZero",
				"Layer 2 go back to zero transmission.",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::m_backto0),
				MakeBooleanChecker())
		.AddAttribute("EwmaGain",
				"Control gain parameter which determines the level of rate decrease",
				DoubleValue(1.0 / 16),
				MakeDoubleAccessor(&RdmaHw::m_g),
				MakeDoubleChecker<double>())
		.AddAttribute ("RateOnFirstCnp",
				"the fraction of rate on first CNP",
				DoubleValue(1.0),
				MakeDoubleAccessor(&RdmaHw::m_rateOnFirstCNP),
				MakeDoubleChecker<double> ())
		.AddAttribute("ClampTargetRate",
				"Clamp target rate.",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::m_EcnClampTgtRate),
				MakeBooleanChecker())
		.AddAttribute("RPTimer",
				"The rate increase timer at RP in microseconds",
				DoubleValue(1500.0),
				MakeDoubleAccessor(&RdmaHw::m_rpgTimeReset),
				MakeDoubleChecker<double>())
		.AddAttribute("RateDecreaseInterval",
				"The interval of rate decrease check",
				DoubleValue(4.0),
				MakeDoubleAccessor(&RdmaHw::m_rateDecreaseInterval),
				MakeDoubleChecker<double>())
		.AddAttribute("FastRecoveryTimes",
				"The rate increase timer at RP",
				UintegerValue(5),
				MakeUintegerAccessor(&RdmaHw::m_rpgThreshold),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("AlphaResumInterval",
				"The interval of resuming alpha",
				DoubleValue(55.0),
				MakeDoubleAccessor(&RdmaHw::m_alpha_resume_interval),
				MakeDoubleChecker<double>())
		.AddAttribute("RateAI",
				"Rate increment unit in AI period",
				DataRateValue(DataRate("5Mb/s")),
				MakeDataRateAccessor(&RdmaHw::m_rai),
				MakeDataRateChecker())
		.AddAttribute("RateHAI",
				"Rate increment unit in hyperactive AI period",
				DataRateValue(DataRate("50Mb/s")),
				MakeDataRateAccessor(&RdmaHw::m_rhai),
				MakeDataRateChecker())
		.AddAttribute("VarWin",
				"Use variable window size or not",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::m_var_win),
				MakeBooleanChecker())
		.AddAttribute("FastReact",
				"Fast React to congestion feedback",
				BooleanValue(true),
				MakeBooleanAccessor(&RdmaHw::m_fast_react),
				MakeBooleanChecker())
		.AddAttribute("MiThresh",
				"Threshold of number of consecutive AI before MI",
				UintegerValue(5),
				MakeUintegerAccessor(&RdmaHw::m_miThresh),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("TargetUtil",
				"The Target Utilization of the bottleneck bandwidth, by default 95%",
				DoubleValue(0.95),
				MakeDoubleAccessor(&RdmaHw::m_targetUtil),
				MakeDoubleChecker<double>())
		.AddAttribute("UtilHigh",
				"The upper bound of Target Utilization of the bottleneck bandwidth, by default 98%",
				DoubleValue(0.98),
				MakeDoubleAccessor(&RdmaHw::m_utilHigh),
				MakeDoubleChecker<double>())
		.AddAttribute("RateBound",
				"Bound packet sending by rate, for test only",
				BooleanValue(true),
				MakeBooleanAccessor(&RdmaHw::m_rateBound),
				MakeBooleanChecker())
		.AddAttribute("MultiRate",
				"Maintain multiple rates in HPCC",
				BooleanValue(true),
				MakeBooleanAccessor(&RdmaHw::m_multipleRate),
				MakeBooleanChecker())
		.AddAttribute("SampleFeedback",
				"Whether sample feedback or not",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::m_sampleFeedback),
				MakeBooleanChecker())
		.AddAttribute("TimelyAlpha",
				"Alpha of TIMELY",
				DoubleValue(0.875),
				MakeDoubleAccessor(&RdmaHw::m_tmly_alpha),
				MakeDoubleChecker<double>())
		.AddAttribute("TimelyBeta",
				"Beta of TIMELY",
				DoubleValue(0.8),
				MakeDoubleAccessor(&RdmaHw::m_tmly_beta),
				MakeDoubleChecker<double>())
		.AddAttribute("TimelyTLow",
				"TLow of TIMELY (ns)",
				UintegerValue(50000),
				MakeUintegerAccessor(&RdmaHw::m_tmly_TLow),
				MakeUintegerChecker<uint64_t>())
		.AddAttribute("TimelyTHigh",
				"THigh of TIMELY (ns)",
				UintegerValue(500000),
				MakeUintegerAccessor(&RdmaHw::m_tmly_THigh),
				MakeUintegerChecker<uint64_t>())
		.AddAttribute("TimelyMinRtt",
				"MinRtt of TIMELY (ns)",
				UintegerValue(20000),
				MakeUintegerAccessor(&RdmaHw::m_tmly_minRtt),
				MakeUintegerChecker<uint64_t>())
		.AddAttribute("DctcpRateAI",
				"DCTCP's Rate increment unit in AI period",
				DataRateValue(DataRate("1000Mb/s")),
				MakeDataRateAccessor(&RdmaHw::m_dctcp_rai),
				MakeDataRateChecker())
		.AddAttribute("PintSmplThresh",
				"PINT's sampling threshold in rand()%65536",
				UintegerValue(65536),
				MakeUintegerAccessor(&RdmaHw::pint_smpl_thresh),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("SwiftRange",
				"fs_range of Swift",
				DoubleValue(100000.0),
				MakeDoubleAccessor(&RdmaHw::m_swift_range),
				MakeDoubleChecker<double>())
		.AddAttribute("SwiftMinCwnd",
				"fs_min_cwnd of Swift (in packets)", 
				DoubleValue(0.001),
				MakeDoubleAccessor(&RdmaHw::m_swift_minCwnd),
				MakeDoubleChecker<double>())
		.AddAttribute("SwiftMaxCwnd",
				"fs_max_cwnd of Swift (in packets)",
				DoubleValue(256.0),
				MakeDoubleAccessor(&RdmaHw::m_swift_maxCwnd),
				MakeDoubleChecker<double>())
		.AddAttribute("SwiftPerHopScalingFactor",
				"Per hop scaling for target delay calculation in Swift (in nanoseconds)",
				DoubleValue(100000.0),
				MakeDoubleAccessor(&RdmaHw::m_swift_perHopScalingFactor),
				MakeDoubleChecker<double>())
		.AddAttribute("SwiftAiFactor",
				"AI of Swift (per packet)",
				DoubleValue(1.0),
				MakeDoubleAccessor(&RdmaHw::m_swift_ai),
				MakeDoubleChecker<double>())
		.AddAttribute("SwiftMdGain",
				"Multiplicative decrease gain (beta) for Swift",
				DoubleValue(0.8),
				MakeDoubleAccessor(&RdmaHw::m_swift_mdGain),
				MakeDoubleChecker<double>())
		.AddAttribute("SwiftMaxMdf",
				"Maximum multiplicative decrease factor per event for Swift",
				DoubleValue(0.5),
				MakeDoubleAccessor(&RdmaHw::m_swift_maxMdFactor),
				MakeDoubleChecker<double>())
		.AddAttribute("SwiftRetxResetThreshold",
				"Retransmission count threshold for resetting cwnd to min_cwnd in Swift",
				UintegerValue(5),
				MakeUintegerAccessor(&RdmaHw::m_swift_retxResetThreshold),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("GPUsPerServer",
				"the number of gpus in a server, used for routing",
				UintegerValue(1),
				MakeUintegerAccessor(&RdmaHw::m_gpus_per_server),
				MakeUintegerChecker<uint32_t>())	
		.AddAttribute("TotalPauseTimes",
				"The number of pause times to simulate PFC pause due to PCIe",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::m_total_pause_times),
				MakeUintegerChecker<uint64_t>())
		.AddAttribute("NVLS_enable",
				"NVLS enable info",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::nvls_enable),
				MakeUintegerChecker<uint32_t>())
		/* Taken from: https://www.usenix.org/system/files/atc25-peng-yajuan.pdf */
		.AddAttribute("MinTimeBetweenCnps",
				"Interval for Receiver Responding with CNP (ns)",
				UintegerValue(4000),
				MakeUintegerAccessor(&RdmaHw::m_min_time_between_cnps),
				MakeUintegerChecker<uint64_t>())
		.AddAttribute("RetransmissionTimeout",
				"The retransmission timeout interval in microseconds. 0 is a special value to disable it.",
				DoubleValue(0.0),
				MakeDoubleAccessor(&RdmaHw::m_retransmissionTimeout),
				MakeDoubleChecker<double>())
		.AddAttribute("L2Retransmission",
				"Layer 2 retransmission scheme: 0=go-back, 1=selective-repeat.",
				UintegerValue(0),
				MakeUintegerAccessor(&RdmaHw::SetL2Retransmission, &RdmaHw::GetL2Retransmission),
				MakeUintegerChecker<uint32_t>())
		.AddAttribute("OOOReorderEnable",
				"Enable OOO Packet Reordering.",
				BooleanValue(false),
				MakeBooleanAccessor(&RdmaHw::SetOOOReorderEnable, &RdmaHw::GetOOOReorderEnable),
				MakeBooleanChecker());
	return tid;
}

RdmaHw::RdmaHw(){
}

void RdmaHw::enable_nvls() {
	nvls_enable = 1;
}

void RdmaHw::disable_nvls() {
	nvls_enable = 0;
}

void RdmaHw::SetL2Retransmission(uint32_t v) {
    if (v > 1) {
        v = 1;
    }

    if (v == 1 && m_oooReorderEnable) {
        m_oooReorderEnable = false;
    }

    m_rtx = v;
}

uint32_t RdmaHw::GetL2Retransmission(void) const {
    return m_rtx;
}

void RdmaHw::SetOOOReorderEnable(bool v) {
    if (v && m_rtx == 1) {
        m_rtx = 0;
    }

    m_oooReorderEnable = v;
}

bool RdmaHw::GetOOOReorderEnable(void) const {
    return m_oooReorderEnable;
}

void RdmaHw::add_nvswitch(uint32_t nvswitch_id) {
	nvswitch_set.insert(nvswitch_id);
}

void RdmaHw::SetNode(Ptr<Node> node){
	m_node = node;
}
void RdmaHw::Setup(QpCompleteCallback cb,SendCompleteCallback send_cb){
	tx_bytes.resize(m_nic.size());
	last_tx_bytes.resize(m_nic.size());
	for (uint32_t i = 0; i < m_nic.size(); i++){
		tx_bytes[i] = 0;
		last_tx_bytes[i] = 0;
		Ptr<QbbNetDevice> dev = m_nic[i].dev;
		if (dev == NULL)
			continue;
		// share data with NIC
		dev->m_rdmaEQ->m_qpGrp = m_nic[i].qpGrp;
		// setup callback
		dev->m_rdmaReceiveCb = MakeCallback(&RdmaHw::Receive, this);
        dev->m_rdmaSentCb = MakeCallback(&RdmaHw::SendPacketComplete, this);
        dev->m_rdmaLinkDownCb = MakeCallback(&RdmaHw::SetLinkDown, this);
		dev->m_rdmaPktSent = MakeCallback(&RdmaHw::PktSent, this);
		dev->m_rdmaUpdateTxBytes = MakeCallback(&RdmaHw::UpdateTxBytes, this);
		// config NIC
		dev->m_rdmaEQ->m_rdmaGetNxtPkt = MakeCallback(&RdmaHw::GetNxtPacket, this);
	}
	// setup qp complete callback
	m_qpCompleteCallback = cb;
    m_sendCompleteCallback = send_cb;
}
uint32_t ip_to_node_id(Ipv4Address ip) { return (ip.Get() >> 8) & 0xffff; }
uint32_t RdmaHw::GetNicIdxOfQp(Ptr<RdmaQueuePair> qp){
	uint32_t src = qp->m_src;
	uint32_t dst = qp->m_dest;
	if(src / m_gpus_per_server == dst / m_gpus_per_server || m_rtTable_nxthop_nvswitch.count(qp->dip.Get()) != 0){ // src and dst are in the same server, communicate through nvswitch
		auto &v = m_rtTable_nxthop_nvswitch[qp->dip.Get()];
		if (v.size() > 0){
			return v[qp->GetHash() % v.size()];
		}else{
			NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
		}
	}else{ // src and dst don't in the same server, communicate through swicth
		auto &v = m_rtTable[qp->dip.Get()];
		if (v.size() > 0){
			return v[qp->GetHash() % v.size()];
		}else{
			NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
		}
	}
}
uint64_t RdmaHw::GetQpKey(uint32_t dip, uint16_t sport, uint16_t pg){
	return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg;
}
Ptr<RdmaQueuePair> RdmaHw::GetQp(uint32_t dip, uint16_t sport, uint16_t pg){
	uint64_t key = GetQpKey(dip, sport, pg);
	auto it = m_qpMap.find(key);
	if (it != m_qpMap.end())
		return it->second;
	return NULL;
}
void RdmaHw::AddQueuePair(uint32_t src, uint32_t dest, uint64_t tag, uint64_t size, uint16_t pg, Ipv4Address sip, Ipv4Address dip, uint16_t sport, uint16_t dport, uint32_t win, uint64_t baseRtt, bool isLossy, Callback<void> notifyAppFinish, Callback<void> notifyAppSent){
	// create qp
	Ptr<RdmaQueuePair> qp = CreateObject<RdmaQueuePair>(pg, sip, dip, sport, dport);
	qp->SetSrc(src);
	qp->SetDest(dest);
	qp->SetTag(tag);
	qp->SetSize(size);
	qp->SetInitialSize(size);
	qp->SetWin(win);
	qp->SetBaseRtt(baseRtt);
	qp->SetVarWin(m_var_win);
	qp->SetIsLossy(isLossy);
	qp->SetAppNotifyCallback(notifyAppFinish);
	qp->SetAppSentCallback(notifyAppSent);
	// add qp
	uint32_t nic_idx = GetNicIdxOfQp(qp);

	// std::cout << "src is: " << src << ", dst is: " << dest <<  ", nic_idx: " << nic_idx << ", and the m_nic size is: " << m_nic.size() << std::endl;
	// Assign the qp to specific qbbnetdevice
	m_nic[nic_idx].qpGrp->AddQp(qp);
	uint64_t key = GetQpKey(dip.Get(), sport, pg);
	m_qpMap[key] = qp;
	qp_cnp[key] = 0;
	last_qp_cnp[key] = 0;
	last_qp_rate[key] = 0;

	// set init variables
	DataRate m_bps = m_nic[nic_idx].dev->GetDataRate();
	qp->m_rate = m_bps;
	qp->m_max_rate = m_bps;
	if (m_cc_mode == 1){
		qp->mlx.m_targetRate = m_bps;
	}else if (m_cc_mode == 3){
		qp->hp.m_curRate = m_bps;
		if (m_multipleRate){
			for (uint32_t i = 0; i < IntHeader::maxHop; i++)
				qp->hp.hopState[i].Rc = m_bps;
		}
	}else if (m_cc_mode == 7){
		qp->tmly.m_curRate = m_bps;
	}else if (m_cc_mode == 10){
		qp->hpccPint.m_curRate = m_bps;
	}else if (m_cc_mode == 11){
		m_swift_alpha = 0;
		m_swift_beta = 0;
		qp->swift.m_cwnd_bytes = win;
        qp->swift.m_cwnd_bytes_past = win;
		qp->swift.m_last_decrease = Simulator::Now();
		qp->swift.m_pacing_delay = Time(0);
	}
	// NVLS settings
	if(nvls_enable == 1) qp->nvls_enable = 1;
	else qp->nvls_enable = 0;

	/* OOO Reorder mode */
	qp->m_oooReorderEnable = m_oooReorderEnable;

	// Notify Nic
	m_nic[nic_idx].dev->NewQp(qp);
}

void RdmaHw::DeleteQueuePair(Ptr<RdmaQueuePair> qp){
	// remove qp from the m_qpMap
	uint64_t key = GetQpKey(qp->dip.Get(), qp->sport, qp->m_pg);
	m_qpMap.erase(key);
	qp_cnp.erase(key);
	last_qp_cnp.erase(key);
	last_qp_rate.erase(key);
}

Ptr<RdmaRxQueuePair> RdmaHw::GetRxQp(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, uint16_t pg, bool create){
    uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
    #ifdef NS3_MTP
    MtpInterface::explicitCriticalSection cs;
    #endif
    auto it = m_rxQpMap.find(key);
    if (it != m_rxQpMap.end()){
        #ifdef NS3_MTP
        cs.ExitSection();
        #endif
        return it->second;
    }
    if (create){
        // create new rx qp
        Ptr<RdmaRxQueuePair> q = CreateObject<RdmaRxQueuePair>();
        // init the qp
        q->sip = sip;
        q->dip = dip;
        q->sport = sport;
        q->dport = dport;
        q->m_ecn_source.qIndex = pg;

		/* OOO Reorder mode */
		q->m_oooReorderEnable = m_oooReorderEnable;
        if (q->m_oooReorderEnable) {
            q->m_milestone_rx = m_ack_interval;
			q->m_lastAckedSeq = 0;
        }

        // store in map
        m_rxQpMap[key] = q;
        #ifdef NS3_MTP
        cs.ExitSection();
        #endif
        return q;
    }
    #ifdef NS3_MTP
    cs.ExitSection();
    #endif
    return NULL;
}

uint32_t RdmaHw::GetNicIdxOfRxQp(Ptr<RdmaRxQueuePair> q){
	// BUG就出现在这里了，首先要判断m_rtTable[q->dip]是否存在，若不存在就去判断m_rtTable_nxthop_nvswitch是否存在，如果都不存在，那么就输出错误
	// auto &v = m_rtTable[q->dip];

	if(m_rtTable.count(q->dip) != 0) {
		auto &v = m_rtTable[q->dip];
		if(v.size() > 0)
			return v[q->GetHash() % v.size()];
		else 
			NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
	} else if(m_rtTable_nxthop_nvswitch.count(q->dip) != 0) {
		auto &v = m_rtTable_nxthop_nvswitch[q->dip];
		if(v.size() > 0)
			return v[q->GetHash() % v.size()];
		else 
			NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
	} else {
		NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
	}
	NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
	
}
void RdmaHw::DeleteRxQp(uint32_t dip, uint16_t pg, uint16_t dport){
	uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
	m_rxQpMap.erase(key);
}

int RdmaHw::SendPacketComplete(Ptr<Packet> p, CustomHeader &ch)
{
	uint16_t qIndex = ch.udp.pg;
	uint16_t port = ch.udp.sport;
	uint32_t seq = ch.udp.seq;
	// uint8_t cnp = (ch.flags >> qbbHeader::FLAG_CNP) & 1;
	// int i;
	Ptr<RdmaQueuePair> qp = GetQp(ch.dip, port, qIndex);
	if (qp == NULL)
	{
		return 0;
	}
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
	SendComplete(qp);
	return 1;
}

void RdmaHw::SendComplete(Ptr<RdmaQueuePair> qp)
{
	NS_ASSERT(!m_sendCompleteCallback.IsNull());

	// std::cout
	// 	<< Simulator::Now().GetTimeStep() << " "
	// 	<< "[SENDER] [COMPLETED] [" << Ipv4Address(qp->dip) << "(" << qp->dport 
	// 	<< ") --> " << Ipv4Address(qp->sip) << "(" << qp->sport << ")] (tag " << qp->m_tag << ")" << std::endl;

	m_sendCompleteCallback(qp);
}

int RdmaHw::ReceiveUdp(Ptr<Packet> p, CustomHeader &ch){
	uint8_t ecnbits = ch.GetIpv4EcnBits();

	uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();
	// Find corresponding rx queue pair
	Ptr<RdmaRxQueuePair> rxQp = GetRxQp(ch.dip, ch.sip, ch.udp.dport, ch.udp.sport, ch.udp.pg, true);
	if (ecnbits != 0){
		rxQp->m_ecn_source.ecnbits |= ecnbits;
		rxQp->m_ecn_source.qfb++;
	}
	rxQp->m_ecn_source.total++;
	/* Normal mode, reset milestone each time. */
    /* In OOO Reorder mode, we maintain m_milestone_rx across packets. */
    if (!rxQp->m_oooReorderEnable) {
        rxQp->m_milestone_rx = m_ack_interval;
    }

	// std::cout 	
	// 	<< Simulator::Now().GetTimeStep() << " " 
	// 	<< "[RECEIVER] "
	// 	<< "[UDP] "
	// 	<< "[" << Ipv4Address(rxQp->dip) << "(" << rxQp->dport 
	// 	<< ") --> "  << Ipv4Address(rxQp->sip) << "(" << rxQp->sport << ")] "
	// 	<< "[Seq " << ch.udp.seq << "] "
	// 	<< "[Size " << payload_size << "] " << std::endl;

	uint32_t hops = 0;
	if (m_cc_mode == 11) { // Swift
		SwiftHopTag hop;
		if (p->PeekPacketTag(hop)) {
			hops = hop.GetHops();
		}
	}

	/* OOO Reorder Enabled, use different logic */
	int x, toAck, toNack;
    if (rxQp->m_oooReorderEnable) {
        std::tie(x, toAck, toNack) = OOOReorderCheck(rxQp, ch.udp.seq, payload_size);
    } else {
		std::tie(x, toAck, toNack) = ReceiverCheckSeq(ch.udp.seq, rxQp, payload_size);
	}
	if (x == 1 || x == 2){ //generate ACK or NACK
		qbbHeader seqh;
		seqh.SetSeq((x == 1) ? toAck : toNack);
		seqh.SetPG(ch.udp.pg);
		seqh.SetSport(ch.udp.dport);
		seqh.SetDport(ch.udp.sport);
		seqh.SetIntHeader(ch.udp.ih);
		if (ecnbits) {
			if (Simulator::Now() >= m_last_cnp_time) {
				seqh.SetCnp();
				m_last_cnp_time = Simulator::Now() + NanoSeconds(m_min_time_between_cnps);
			} else {
				ecnbits = 0;
			}
		}

		Ptr<Packet> newp = Create<Packet>(std::max(60-14-20-(int)seqh.GetSerializedSize(), 0));
		newp->AddHeader(seqh);

		Ipv4Header head;	// Prepare IPv4 header
		head.SetDestination(Ipv4Address(ch.sip));
		head.SetSource(Ipv4Address(ch.dip));
		head.SetProtocol(x == 1 ? 0xFC : 0xFD); //ack=0xFC nack=0xFD
		head.SetTtl(64);
		head.SetPayloadSize(newp->GetSize());
		head.SetIdentification(rxQp->m_ipid++);
		// GPU receives the packet and generate ACK with NVLS tag
		if(ch.m_tos == 4) head.SetTos(4);

		newp->AddHeader(head);
		AddHeader(newp, 0x800);	// Attach PPP header

		if (m_cc_mode == 11) { // Swift
			SwiftHopTag hop_tag;
			hop_tag.SetHops(hops);
      		newp->AddPacketTag(hop_tag);
		}

		uint32_t sip = ch.sip;
		uint32_t sid = (sip >> 8) & 0xffff;
		uint32_t dip = ch.dip;
		uint32_t did = (dip >> 8) & 0xffff;
		// send
		uint32_t nic_idx = GetNicIdxOfRxQp(rxQp);
		m_nic[nic_idx].dev->RdmaEnqueueHighPrioQ(newp);
		// 发送给目标 NVSwitch 的报文
		if(did == m_node->GetId() && m_node->GetNodeType() == 2 && ch.m_tos == 4) m_nic[nic_idx].dev->SwitchAsHostSend();
		else m_nic[nic_idx].dev->TriggerTransmit();
	} else if (x == 6) {
		// Send ACK for last acked
		qbbHeader seqh;
		seqh.SetSeq(toAck);
		seqh.SetPG(ch.udp.pg);
		seqh.SetSport(ch.udp.dport);
		seqh.SetDport(ch.udp.sport);
		seqh.SetIntHeader(ch.udp.ih);
		if (ecnbits) {
			if (Simulator::Now() >= m_last_cnp_time) {
				seqh.SetCnp();
				m_last_cnp_time = Simulator::Now() + NanoSeconds(m_min_time_between_cnps);
			} else {
				ecnbits = 0;
			}
		}

		Ptr<Packet> newp = Create<Packet>(std::max(60-14-20-(int)seqh.GetSerializedSize(), 0));
		newp->AddHeader(seqh);

		Ipv4Header head;
		head.SetDestination(Ipv4Address(ch.sip));
		head.SetSource(Ipv4Address(ch.dip));
		head.SetProtocol(0xFC);
		head.SetTtl(64);
		head.SetPayloadSize(newp->GetSize());
		head.SetIdentification(rxQp->m_ipid++);
		if(ch.m_tos == 4) head.SetTos(4);

		newp->AddHeader(head);
		AddHeader(newp, 0x800);

		if (m_cc_mode == 11) { // Swift
			SwiftHopTag hop_tag;
			hop_tag.SetHops(hops);
      		newp->AddPacketTag(hop_tag);
		}

		uint32_t sip = ch.sip;
		uint32_t sid = (sip >> 8) & 0xffff;
		uint32_t dip = ch.dip;
		uint32_t did = (dip >> 8) & 0xffff;
		uint32_t nic_idx = GetNicIdxOfRxQp(rxQp);
		m_nic[nic_idx].dev->RdmaEnqueueHighPrioQ(newp);

		// Send selective NACK
		qbbHeader seqh1;
		seqh1.SetSeq(toNack);
		seqh1.SetPG(ch.udp.pg);
		seqh1.SetSport(ch.udp.dport);
		seqh1.SetDport(ch.udp.sport);
		seqh1.SetIntHeader(ch.udp.ih);

		Ptr<Packet> newp1 = Create<Packet>(std::max(60-14-20-(int)seqh1.GetSerializedSize(), 0));
		newp1->AddHeader(seqh1);

		Ipv4Header head1;
		head1.SetDestination(Ipv4Address(ch.sip));
		head1.SetSource(Ipv4Address(ch.dip));
		head1.SetProtocol(0xFD);
		head1.SetTtl(64);
		head1.SetPayloadSize(newp->GetSize());
		head1.SetIdentification(rxQp->m_ipid++);
		if(ch.m_tos == 4) head1.SetTos(4);

		newp1->AddHeader(head1);
		AddHeader(newp1, 0x800);

		if (m_cc_mode == 11) { // Swift
			SwiftHopTag hop_tag;
			hop_tag.SetHops(hops);
      		newp1->AddPacketTag(hop_tag);
		}

		m_nic[nic_idx].dev->RdmaEnqueueHighPrioQ(newp1);

		if(did == m_node->GetId() && m_node->GetNodeType() == 2 && ch.m_tos == 4) m_nic[nic_idx].dev->SwitchAsHostSend();
		else m_nic[nic_idx].dev->TriggerTransmit();
	} else if (x == 4 && ecnbits) {
		/* We should not ACK/NACK but send a pure CNP! */
		if (Simulator::Now() >= m_last_cnp_time) {
			/* It is time to send a CNP */
			CnHeader cn_hdr;
			cn_hdr.SetSeq(ch.udp.seq);
			cn_hdr.SetQindex(ch.udp.pg);
			cn_hdr.SetFlow(ch.udp.sport);
	
			Ptr<Packet> cnp = Create<Packet>(std::max(64-14-20-16, 0));
			cnp->AddHeader(cn_hdr);
	
			Ipv4Header ipv4;
			ipv4.SetDestination(Ipv4Address(ch.sip));
			ipv4.SetSource(Ipv4Address(ch.dip));
			ipv4.SetProtocol(0xFF);
			ipv4.SetTtl(64);
			ipv4.SetPayloadSize(cnp->GetSize());
			ipv4.SetIdentification(rxQp->m_ipid++);
			if(ch.m_tos == 4) ipv4.SetTos(4);
	
			cnp->AddHeader(ipv4);
			AddHeader(cnp, 0x800);
	
			uint32_t dip = ch.dip;
			uint32_t did = (dip >> 8) & 0xffff;
			uint32_t nic_idx = GetNicIdxOfRxQp(rxQp);
			m_nic[nic_idx].dev->RdmaEnqueueHighPrioQ(cnp);
			if(did == m_node->GetId() && m_node->GetNodeType() == 2 && ch.m_tos == 4) m_nic[nic_idx].dev->SwitchAsHostSend();
			else m_nic[nic_idx].dev->TriggerTransmit();
			
			m_last_cnp_time = Simulator::Now() + NanoSeconds(m_min_time_between_cnps);
		} else {
			/* Not time to send a CNP */
			ecnbits = 0;
		}
	}

	// std::cout 	
	// 	<< Simulator::Now().GetTimeStep() << " " 
	// 	<< "[RECEIVER] "
	// 	<< "[UDP] "
	// 	<< "[" << Ipv4Address(rxQp->dip) << "(" << rxQp->dport 
	// 	<< ") --> "  << Ipv4Address(rxQp->sip) << "(" << rxQp->sport << ")] "
	// 	<< "[Response ";
	// if (x == 1){
	// 	std::cout << "ACK" << "(" << rxQp->ReceiverNextExpectedSeq << ")";
	// }else if (x == 2){
	// 	std::cout << "NACK" << "(" << rxQp->ReceiverNextExpectedSeq << ")";
	// }else if (x == 6) {
	// 	std::cout << "ACK" << "(" << toAck << ") and NACK" << "(" << toNack << ")";
	// } else if (x == 4 && ecnbits) {
	// 	std::cout << "CNP";
	// } else
	// 	std::cout << "NONE";

	// if (x != 4 && ecnbits != 0) {
	// 	std::cout << " + CNP";
	// }
	// std::cout << "] " << x << std::endl;

	return 0;
}

int RdmaHw::ReceiveCnp(Ptr<Packet> p, CustomHeader &ch){
	// QCN on NIC
	// This is a Congestion signal
	// Then, extract data from the congestion packet.
	// We assume, without verify, the packet is destinated to me
	uint32_t qIndex = ch.cnp.qIndex;
	if (qIndex == 1){		//DCTCP
		return 0;
	}
	uint16_t udpport = ch.cnp.fid; // corresponds to the sport
	uint8_t ecnbits = ch.cnp.ecnBits;
	uint16_t qfb = ch.cnp.qfb;
	uint16_t total = ch.cnp.total;

	uint32_t i;
	// get qp
	Ptr<RdmaQueuePair> qp = GetQp(ch.sip, udpport, qIndex);
	if (qp == NULL) {
		/* QP completed when the CNP is received, return */
		// std::cout << "ERROR: QCN NIC cannot find the flow\n";
		return 0;
	}
	// get nic
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;

	if (qp->m_rate == 0)			//lazy initialization	
	{
		qp->m_rate = dev->GetDataRate();
		if (m_cc_mode == 1){
			qp->mlx.m_targetRate = dev->GetDataRate();
		}else if (m_cc_mode == 3){
			qp->hp.m_curRate = dev->GetDataRate();
			if (m_multipleRate){
				for (uint32_t i = 0; i < IntHeader::maxHop; i++)
					qp->hp.hopState[i].Rc = dev->GetDataRate();
			}
		}else if (m_cc_mode == 7){
			qp->tmly.m_curRate = dev->GetDataRate();
		}else if (m_cc_mode == 10){
			qp->hpccPint.m_curRate = dev->GetDataRate();
		}
	}

	// Handle simple CNP packets here for DCQCN, not piggybacked on ACK/NACK
	if (m_cc_mode == 1) {
		uint64_t key = GetQpKey(qp->dip.Get(), qp->sport, qp->m_pg);
		qp_cnp[key]++; // update for the number of cnp this qp has received
		cnp_received_mlx(qp);
	}

	return 0;
}

int RdmaHw::ReceiveAck(Ptr<Packet> p, CustomHeader &ch){
	uint16_t qIndex = ch.ack.pg;
	uint16_t port = ch.ack.dport;
	uint64_t seq = ch.ack.seq;
	uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;

	int i;
	Ptr<RdmaQueuePair> qp = GetQp(ch.sip, port, qIndex);
	if (qp == NULL){
		return 0;
	}

	uint32_t nic_idx = GetNicIdxOfQp(qp);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;

	// std::cout
	// 	<< Simulator::Now().GetTimeStep() << " "
	// 	<< "[SENDER]  " << ((ch.l3Prot == 0xFD ? "[NACK]" : " [ACK]")) << " "
	// 	<< "[" << Ipv4Address(qp->dip) << "(" << qp->dport 
	// 	<< ") --> " << Ipv4Address(qp->sip) << "(" << qp->sport << ")] (tag " << qp->m_tag << ")"
	// 	<< (ch.l3Prot == 0xFD ? "NACK" : "ACK") << "(" << seq << ")"
	// 	<< (cnp ? "+CNP" : "") << std::endl;

	uint32_t bytes_acked = 0;
	if (m_ack_interval == 0)
		std::cout << "ERROR: shouldn't receive ack\n";
	else {
        uint64_t prev_una = qp->snd_una;
		if (!m_backto0){
			qp->Acknowledge(seq);
		}else {
			uint64_t goback_seq = seq / m_chunk * m_chunk;
			qp->Acknowledge(goback_seq);
		}
        bytes_acked = (qp->snd_una > prev_una) ? (qp->snd_una - prev_una) : 0;
		
		if (qp->IsFinished()){
			SendComplete(qp);
			QpComplete(qp);
		}
	}

	if (ch.l3Prot == 0xFC) { // ACK
		if (m_rtx == 1 || m_oooReorderEnable) {
			/* Delete below the received PSN */
			qp->DeleteTxBufferBelowPsn(seq);
		} 
		
		if (m_retransmissionTimeout > 0) {
			/* If RTX are enabled, clear the TX timestamps */
			qp->ClearTxTimesBelowPsn(seq);
		}
	}

	if (m_ack_interval > 0) {
		// Restart sender timer on ACK/NACK reception
		RestartSenderTimer(qp);
	}
	
	if (ch.l3Prot == 0xFD) { // NACK
		if (m_rtx == 1) {
			/* add the NACK'd sequence to retransmission list */
			if (seq >= qp->snd_una && qp->m_retransmissionBuffer.count(seq) == 0) {
				qp->PopulateRetransmissionBuffer(seq);
			}
		} else if (m_oooReorderEnable) {
			/* Only Fast NACKs from the switches can be received when in OOO Reorder. This is safe then :) */
			/* In this case, we do not check if it is >= snd_una, it could happen to receive any type of NACK */
			if (qp->m_retransmissionBuffer.count(seq) == 0) {
				qp->PopulateRetransmissionBuffer(seq);
			}
		} else {
			// restart from the last unacknowledged sequence
			RecoverQueue(qp);
		}
	}

	// handle cnp
	if (cnp){
		uint64_t key = GetQpKey(qp->dip.Get(), qp->sport, qp->m_pg);
		qp_cnp[key]++; // update for the number of cnp this qp has received
		if (m_cc_mode == 1){ // mlx version
			cnp_received_mlx(qp);
		} 
	}

	if (m_cc_mode == 3){
		HandleAckHp(qp, p, ch);
	}else if (m_cc_mode == 7){
		HandleAckTimely(qp, p, ch);
	}else if (m_cc_mode == 8){
		HandleAckDctcp(qp, p, ch);
	}else if (m_cc_mode == 10){
		HandleAckHpPint(qp, p, ch);
	}else if (m_cc_mode == 11){
		HandleAckSwift(qp, p, ch, bytes_acked);
	}
	uint32_t sip = ch.sip;
	uint32_t sid = (sip >> 8) & 0xffff;
	uint32_t dip = ch.dip;
	uint32_t did = (dip >> 8) & 0xffff;
	// ACK may advance the on-the-fly window, allowing more packets to send
	if(did == m_node->GetId() && m_node->GetNodeType() == 2) m_nic[nic_idx].dev->SwitchAsHostSend();
	else m_nic[nic_idx].dev->TriggerTransmit();
	//std:://cout << "ack triggere transmitted\n";
	return 0;
}

int RdmaHw::Receive(Ptr<Packet> p, CustomHeader &ch){
	if (ch.l3Prot == 0x11){ // UDP
		ReceiveUdp(p, ch);
	}else if (ch.l3Prot == 0xFF){ // CNP
		ReceiveCnp(p, ch);
	}else if (ch.l3Prot == 0xFD){ // NACK
		ReceiveAck(p, ch);
	}else if (ch.l3Prot == 0xFC){ // ACK
		ReceiveAck(p, ch);
	}
	return 0;
}

std::tuple<int, uint64_t, uint64_t> RdmaHw::ReceiverCheckSeq(uint64_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size) {
	auto logAndReturn = [&](int code, uint64_t ack, uint64_t nack) {
		// std::cout
		// 	<< Simulator::Now().GetTimeStep() << " "
		// 	<< "[RECEIVER] [INFO]"
		// 	<< "[" << Ipv4Address(q->sip) << "(" << q->sport
		// 	<< ") --> " << Ipv4Address(q->dip) << "(" << q->dport << ")] "
		// 	<< "[ReceiverCheckSeq] "
		// 	<< "code=" << code
		// 	<< " seq=" << seq
		// 	<< " size=" << size
		// 	<< " ackSeq=" << ack;
		// 	if (code == 2 || code == 6) 
		// 		std::cout << " nackSeq=" << nack;
		// 	else
		// 		std::cout << " nackSeq=NULL";
		// 	std::cout 
		// 	<< " expected=" << q->ReceiverNextExpectedSeq
		// 	<< " last=" << q->ReceiverLastExpectedSeq
		// 	<< " highest=" << q->HighestSeqno
		// 	<< " buffering=" << q->m_rxSrBuffering
		// 	<< std::endl;

        // if (q->m_rxSrBuffering) {
        //     std::cout
        //         << Simulator::Now().GetTimeStep() << " "
        //         << "[RECEIVER] [INFO]"
        //         << "[" << Ipv4Address(q->sip) << "(" << q->sport
        //         << ") --> " << Ipv4Address(q->dip) << "(" << q->dport << ")] "
        //         << "[SR RX buffer] ";

        //     for (auto it = q->m_rxBuffer.begin(); it != q->m_rxBuffer.end(); ++it) {
        //         std::cout << it->first << "/" << it->second << " ";
        //     }
        //     std::cout << std::endl;
        // }

        return std::make_tuple(code, ack, nack);
    };

    uint64_t expected = q->ReceiverNextExpectedSeq;

    if (m_rtx == 0) {
        if (seq == expected) {
            q->ReceiverNextExpectedSeq += size;
            if (q->ReceiverNextExpectedSeq >= q->m_milestone_rx) {
                q->m_milestone_rx = q->ReceiverNextExpectedSeq + m_ack_interval;
                return logAndReturn(1, q->ReceiverNextExpectedSeq, 0);
            }
            return logAndReturn(4, 0, 0);
        } else if (seq > expected) {
            return logAndReturn(2, 0, expected);
        } else {
            return logAndReturn(1, q->ReceiverNextExpectedSeq, 0);
        }
    } else {
		q->HighestSeqno = std::max(q->HighestSeqno, seq + size);
	
		if (q->CheckPsnExists(seq)) {
			/* Duplicate, ACK again */
			return logAndReturn(1, q->ReceiverNextExpectedSeq, 0);
		}
	
		/* First time we see this psn */
		q->AddPsnToList(seq, size);
	
		if (seq == expected) {
			RdmaRxQueuePair::SrPsnResult res = q->AdvancePsnContiguous();
	
			if (res.advanced) {
				q->m_milestone_rx = std::max((uint64_t)q->m_milestone_rx, q->ReceiverNextExpectedSeq);
			}
	
			if (!q->m_rxSrBuffering) { /* We were not buffering before */	
				if (res.hasHole) { /* There is a hole */
					q->m_rxSrBuffering = true;
					q->m_nackTimer = Simulator::Now() + MicroSeconds(m_nack_interval);
					q->m_lastNACK = res.firstHole;
	
					/* ACK up to cumulative point and NACK the first hole */
					return logAndReturn(6, q->ReceiverNextExpectedSeq, res.firstHole);
				} else { /* No holes, contiguous advance */
					if (q->ReceiverNextExpectedSeq >= q->m_milestone_rx) {
						q->m_milestone_rx = q->ReceiverNextExpectedSeq + m_ack_interval;
						return logAndReturn(1, q->ReceiverNextExpectedSeq, 0);
					}
					/* Do nothing */
					return logAndReturn(4, 0, 0);
				}
			} else {
				/* We are already buffering (we know we have holes) */
				if (res.hasHole) {
					q->m_lastNACK = res.firstHole;
					return logAndReturn(6, q->ReceiverNextExpectedSeq, res.firstHole);
				} else {
					/* Holes are gone, contiguous again */
					q->m_rxSrBuffering = false;
					if (q->ReceiverNextExpectedSeq >= q->m_milestone_rx) {
						q->m_milestone_rx = q->ReceiverNextExpectedSeq + m_ack_interval;
						return logAndReturn(1, q->ReceiverNextExpectedSeq, 0);
					}
					/* Do nothing */
					return logAndReturn(4, 0, 0);
				}
			}
		} else if (seq > expected) {
			/* There is at least one hole at "expected" */
			if (!q->m_rxSrBuffering) {
				q->m_rxSrBuffering = true;
				q->m_nackTimer = Simulator::Now() + MicroSeconds(m_nack_interval);
				q->m_lastNACK = expected;
	
				/* ACK cumulative point and NACK the first missing packet */
				return (expected > 0) ? logAndReturn(6, q->ReceiverNextExpectedSeq, expected) : logAndReturn(2, 0, expected);
			}
	
			/* Already buffering, do nothing */
			return logAndReturn(4, 0, 0);
		}
	
		/* Duplicate, ACK again */
		return logAndReturn(1, q->ReceiverNextExpectedSeq, 0);
	}
}

void RdmaHw::AddHeader (Ptr<Packet> p, uint16_t protocolNumber){
	PppHeader ppp;
	ppp.SetProtocol (EtherToPpp (protocolNumber));
	p->AddHeader (ppp);
}
uint16_t RdmaHw::EtherToPpp (uint16_t proto){
	switch(proto){
		case 0x0800: return 0x0021;   //IPv4
		case 0x86DD: return 0x0057;   //IPv6
		default: NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
	}
	return 0;
}

void RdmaHw::RecoverQueue(Ptr<RdmaQueuePair> qp){
	qp->snd_nxt = qp->snd_una;
}

void RdmaHw::QpComplete(Ptr<RdmaQueuePair> qp){
	NS_ASSERT(!m_qpCompleteCallback.IsNull());
	if (m_cc_mode == 1){
		Simulator::Cancel(qp->mlx.m_eventUpdateAlpha);
		Simulator::Cancel(qp->mlx.m_eventDecreaseRate);
		Simulator::Cancel(qp->mlx.m_rpTimer);
	}

	// Cancel sender timer when QP ends
	CancelSenderTimer(qp);

	// This callback will log info
	// It may also delete the rxQp on the receiver
	m_qpCompleteCallback(qp);

	qp->m_notifyAppFinish();

	// delete the qp
	DeleteQueuePair(qp);
}

void RdmaHw::SetLinkDown(Ptr<QbbNetDevice> dev){
	printf("RdmaHw: node:%u a link down\n", m_node->GetId());
}

void RdmaHw::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx, bool is_nvswitch){
	uint32_t dip = dstAddr.Get();
	if(is_nvswitch == false)
		m_rtTable[dip].push_back(intf_idx);
	else {
		m_rtTable_nxthop_nvswitch[dip].push_back(intf_idx);
	}
}

void RdmaHw::ClearTable(){
	m_rtTable.clear();
	m_rtTable_nxthop_nvswitch.clear();
}

void RdmaHw::RedistributeQp(){
	// clear old qpGrp
	for (uint32_t i = 0; i < m_nic.size(); i++){
		if (m_nic[i].dev == NULL)
			continue;
		m_nic[i].qpGrp->Clear();
	}

	// redistribute qp
	for (auto &it : m_qpMap){
		Ptr<RdmaQueuePair> qp = it.second;
		uint32_t nic_idx = GetNicIdxOfQp(qp);
		m_nic[nic_idx].qpGrp->AddQp(qp);
		// Notify Nic
		m_nic[nic_idx].dev->ReassignedQp(qp);
	}
}

Ptr<Packet> RdmaHw::GetNxtPacket(Ptr<RdmaQueuePair> qp){
	uint64_t seq_to_send = 0;
	uint64_t payload_size = 0;

	if (!qp->m_retransmissionBuffer.empty()) {
		// Send from the retransmission list
		auto it = qp->m_retransmissionBuffer.begin();
		auto value = *it;
		qp->m_retransmissionBuffer.erase(it);

		seq_to_send = value.first;
		payload_size = value.second;
	} else {
		// Send the next packet
		seq_to_send = qp->snd_nxt;
		payload_size = qp->GetBytesLeft();
		if ((uint64_t)m_mtu < payload_size)
			payload_size = m_mtu;
	}

	uint64_t sent_end = seq_to_send + payload_size;
	uint64_t bytes_left_after = (sent_end >= qp->m_size) ? 0 : (qp->m_size - sent_end);

	// std::cout 	
	// 	<< Simulator::Now().GetTimeStep() << " " 
	// 	<< "[SENDER] "
	// 	<< "[UDP] "
	// 	<< "[" << Ipv4Address(qp->sip) << "(" << qp->sport 
	// 	<< ") --> "  << Ipv4Address(qp->dip) << "(" << qp->dport << ")] (tag " << qp->m_tag << ")"
	// 	<< "[Seq " << seq_to_send << "] "
	// 	<< "[Size " << payload_size << "] " 
	// 	<< "[BytesLeft " << bytes_left_after << "] " << std::endl;

	Ptr<Packet> p = Create<Packet> ((uint32_t)payload_size);
	// add SimpleSeqTsHeader
	SimpleSeqTsHeader seqTs;
	seqTs.SetSeq (seq_to_send);
	seqTs.SetPG (qp->m_pg);
	seqTs.SetBytesLeft (bytes_left_after);
	p->AddHeader (seqTs);
	// add udp header
	UdpHeader udpHeader;
	udpHeader.SetDestinationPort (qp->dport);
	udpHeader.SetSourcePort (qp->sport);
	p->AddHeader (udpHeader);
	// add ipv4 header
	Ipv4Header ipHeader;
	ipHeader.SetSource (qp->sip);
	ipHeader.SetDestination (qp->dip);
	ipHeader.SetProtocol (0x11);
	ipHeader.SetPayloadSize (p->GetSize());
	ipHeader.SetTtl (64);
	// nvls <-> ToS, ToS = 1 -> NVLS enable
	if(qp->nvls_enable == 1) ipHeader.SetTos (4);
	else ipHeader.SetTos (0);
	ipHeader.SetIdentification (qp->m_ipid);
	p->AddHeader(ipHeader);
	// add ppp header
	PppHeader ppp;
	ppp.SetProtocol (0x0021); // EtherToPpp(0x800), see point-to-point-net-device.cc
	p->AddHeader (ppp);

	// If lossy, add the custom PriorityTag
	if (qp->m_isLossy) {
		PriorityTag prio_tag;
      	prio_tag.SetPriority(qp->m_pg);
      	p->AddPacketTag(prio_tag);
	}

	// update state
	if (seq_to_send == qp->snd_nxt) {
		qp->snd_nxt += payload_size;
	}
	// std::cout << "current snd_nxt is: " << qp->snd_nxt << ", the window is: " << qp->m_win << std::endl;
	qp->m_ipid++;

	if (m_rtx == 1 || m_oooReorderEnable) {
		qp->PopulateTxBuffer(seq_to_send, payload_size);
	}

	if (m_retransmissionTimeout > 0) {
		qp->NoteTxTime(seq_to_send, Simulator::Now());
	}
	
	if (seq_to_send == 0 && !qp->m_senderTimer.IsRunning()) { // this is the first packet
		RestartSenderTimer(qp);
	}

	if (m_cc_mode == 11) {
		SwiftHopTag hop_tag;
		hop_tag.SetHops(0);
      	p->AddPacketTag(hop_tag);
	}

	// return
	return p;
}

void RdmaHw::PktSent(Ptr<RdmaQueuePair> qp, Ptr<Packet> pkt, Time interframeGap){
	qp->m_last_sent = Simulator::Now();
	qp->lastPktSize = pkt->GetSize();
	UpdateNextAvail(qp, interframeGap, pkt->GetSize());
}

void RdmaHw::UpdateNextAvail(Ptr<RdmaQueuePair> qp, Time interframeGap, uint32_t pkt_size){
	Time sendingTime;
	if (m_rateBound)
		sendingTime = interframeGap + qp->m_rate.CalculateBytesTxTime(pkt_size);
	else
		sendingTime = interframeGap + qp->m_max_rate.CalculateBytesTxTime(pkt_size);
	qp->m_nextAvail = Simulator::Now() + sendingTime;
}

void RdmaHw::ChangeRate(Ptr<RdmaQueuePair> qp, DataRate new_rate){
	#if 1
	Time sendingTime = qp->m_rate.CalculateBytesTxTime(qp->lastPktSize);
	Time new_sendintTime = new_rate.CalculateBytesTxTime(qp->lastPktSize);
	qp->m_nextAvail = qp->m_nextAvail + new_sendintTime - sendingTime;
	// update nic's next avail event
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	m_nic[nic_idx].dev->UpdateNextAvail(qp->m_nextAvail);
	#endif

	// change to new rate
	qp->m_rate = new_rate;
}
/**
 * when nic send a packet, update the bytes it has sent
*/
void RdmaHw::UpdateTxBytes(uint32_t port_id, uint64_t bytes){
	tx_bytes[port_id] += bytes;
}
/**
 * output format:
 * time, host_id, port_id, bandwidth
*/
void RdmaHw::PrintHostBW(FILE* bw_output, uint32_t bw_mon_interval){
	for(int i = 0; i < m_nic.size(); ++i){
		if(tx_bytes[i] == last_tx_bytes[i]){
			continue;
		}
		double bw = (tx_bytes[i] - last_tx_bytes[i]) * 8 * 1e6 / (bw_mon_interval); // bit/s
		bw = bw*1.0 / 1e9; // Gbps
		fprintf(bw_output, "%lu, %u, %u, %f\n", Simulator::Now().GetTimeStep(), m_node->GetId(), i, bw);
		fflush(bw_output);
		last_tx_bytes[i] = tx_bytes[i];
	}
}
/**
 * output format:
 * time, src, dst, sport, dport, size, rate
*/
void RdmaHw::PrintQPRate(FILE* rate_output){
	std::unordered_map<uint64_t, Ptr<RdmaQueuePair>>::iterator it = m_qpMap.begin();
	for(; it != m_qpMap.end(); it++){
		Ptr<RdmaQueuePair> qp = it->second;
		uint64_t key = it->first;
		if(qp->m_rate.GetBitRate() == last_qp_rate[key]){
			continue;
		}
		fprintf(rate_output, "%lu, %u, %u, %u, %u, %u, %lu\n", Simulator::Now().GetTimeStep(), qp->m_src, qp->m_dest, qp->sport, qp->dport, qp->m_size, qp->m_rate.GetBitRate());
		fflush(rate_output);
		last_qp_rate[key] = qp->m_rate.GetBitRate();
	}
}
/**
 * output format:
 * time, src, dst, sport, dport, size, cnp_number
*/
void RdmaHw::PrintQPCnpNumber(FILE* cnp_output){
	std::unordered_map<uint64_t, Ptr<RdmaQueuePair>>::iterator it = m_qpMap.begin();
	for(; it != m_qpMap.end(); it++){
		Ptr<RdmaQueuePair> qp = it->second;
		uint64_t key = it->first;
		if(qp_cnp[key] != last_qp_cnp[key]){
			fprintf(cnp_output, "%lu, %u, %u, %u, %u, %u, %u\n", Simulator::Now().GetTimeStep(), qp->m_src, qp->m_dest, qp->sport, qp->dport, qp->m_size, qp_cnp[key]);
			fflush(cnp_output);
			last_qp_cnp[key] = qp_cnp[key];
		}
	}
}
#define PRINT_LOG 0
/******************************
 * Mellanox's version of DCQCN
 *****************************/
void RdmaHw::UpdateAlphaMlx(Ptr<RdmaQueuePair> q){
	#if PRINT_LOG
	//printf("%lu alpha update: %08x %08x %u %u %.6lf->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_alpha);
	#endif
	if (q->mlx.m_alpha_cnp_arrived){
		q->mlx.m_alpha = (1 - m_g)*q->mlx.m_alpha + m_g; 	//binary feedback
	}else {
		q->mlx.m_alpha = (1 - m_g)*q->mlx.m_alpha; 	//binary feedback
	}
	#if PRINT_LOG
	//printf("%.6lf\n", q->mlx.m_alpha);
	#endif
	q->mlx.m_alpha_cnp_arrived = false; // clear the CNP_arrived bit
	ScheduleUpdateAlphaMlx(q);
}
void RdmaHw::ScheduleUpdateAlphaMlx(Ptr<RdmaQueuePair> q){
	q->mlx.m_eventUpdateAlpha = Simulator::Schedule(MicroSeconds(m_alpha_resume_interval), &RdmaHw::UpdateAlphaMlx, this, q);
}

void RdmaHw::cnp_received_mlx(Ptr<RdmaQueuePair> q){
	q->mlx.m_alpha_cnp_arrived = true; // set CNP_arrived bit for alpha update
	q->mlx.m_decrease_cnp_arrived = true; // set CNP_arrived bit for rate decrease
	if (q->mlx.m_first_cnp){
		// init alpha
		q->mlx.m_alpha = 1;
		q->mlx.m_alpha_cnp_arrived = false;
		// schedule alpha update
		ScheduleUpdateAlphaMlx(q);
		// schedule rate decrease
		ScheduleDecreaseRateMlx(q, 1); // add 1 ns to make sure rate decrease is after alpha update
		// set rate on first CNP
		q->mlx.m_targetRate = q->m_rate = m_rateOnFirstCNP * q->m_rate;
		q->mlx.m_first_cnp = false;
	}
}

void RdmaHw::CheckRateDecreaseMlx(Ptr<RdmaQueuePair> q){
	ScheduleDecreaseRateMlx(q, 0);
	if (q->mlx.m_decrease_cnp_arrived){
		#if PRINT_LOG
		printf("%lu rate dec: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
		#endif
		bool clamp = true;
		if (!m_EcnClampTgtRate){
			if (q->mlx.m_rpTimeStage == 0)
				clamp = false;
		}
		if (clamp)
			q->mlx.m_targetRate = q->m_rate;
		q->m_rate = std::max(m_minRate, q->m_rate * (1 - q->mlx.m_alpha / 2));
		// reset rate increase related things
		q->mlx.m_rpTimeStage = 0;
		q->mlx.m_decrease_cnp_arrived = false;
		Simulator::Cancel(q->mlx.m_rpTimer);
		q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
		#if PRINT_LOG
		printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
		#endif
	}
}
void RdmaHw::ScheduleDecreaseRateMlx(Ptr<RdmaQueuePair> q, uint32_t delta){
	q->mlx.m_eventDecreaseRate = Simulator::Schedule(MicroSeconds(m_rateDecreaseInterval) + NanoSeconds(delta), &RdmaHw::CheckRateDecreaseMlx, this, q);
}

void RdmaHw::RateIncEventTimerMlx(Ptr<RdmaQueuePair> q){
	q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
	RateIncEventMlx(q);
	q->mlx.m_rpTimeStage++;
}
void RdmaHw::RateIncEventMlx(Ptr<RdmaQueuePair> q){
	// check which increase phase: fast recovery, active increase, hyper increase
	if (q->mlx.m_rpTimeStage < m_rpgThreshold){ // fast recovery
		FastRecoveryMlx(q);
	}else if (q->mlx.m_rpTimeStage == m_rpgThreshold){ // active increase
		ActiveIncreaseMlx(q);
	}else { // hyper increase
		HyperIncreaseMlx(q);
	}
}

void RdmaHw::FastRecoveryMlx(Ptr<RdmaQueuePair> q){
	#if PRINT_LOG
	printf("%lu fast recovery: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
	#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
}
void RdmaHw::ActiveIncreaseMlx(Ptr<RdmaQueuePair> q){
	#if PRINT_LOG
	printf("%lu active inc: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
	// get NIC
	uint32_t nic_idx = GetNicIdxOfQp(q);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
	// increate rate
	q->mlx.m_targetRate += m_rai;
	if (q->mlx.m_targetRate > dev->GetDataRate())
		q->mlx.m_targetRate = dev->GetDataRate();
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
	#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
}
void RdmaHw::HyperIncreaseMlx(Ptr<RdmaQueuePair> q){
	#if PRINT_LOG
	printf("%lu hyper inc: %08x %08x %u %u (%0.3lf %.3lf)->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport, q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
	// get NIC
	uint32_t nic_idx = GetNicIdxOfQp(q);
	Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
	// increate rate
	q->mlx.m_targetRate += m_rhai;
	if (q->mlx.m_targetRate > dev->GetDataRate())
		q->mlx.m_targetRate = dev->GetDataRate();
	q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
	#if PRINT_LOG
	printf("(%.3lf %.3lf)\n", q->mlx.m_targetRate.GetBitRate() * 1e-9, q->m_rate.GetBitRate() * 1e-9);
	#endif
}

/***********************
 * High Precision CC
 ***********************/
void RdmaHw::HandleAckHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch){
	uint64_t ack_seq = ch.ack.seq;
	// update rate
	if (ack_seq > qp->hp.m_lastUpdateSeq){ // if full RTT feedback is ready, do full update
		UpdateRateHp(qp, p, ch, false);
	}else{ // do fast react
		FastReactHp(qp, p, ch);
	}
}

void RdmaHw::UpdateRateHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react){
	uint64_t next_seq = qp->snd_nxt;
	bool print = !fast_react || true;
	if (qp->hp.m_lastUpdateSeq == 0){ // first RTT
		qp->hp.m_lastUpdateSeq = next_seq;
		// store INT
		IntHeader &ih = ch.ack.ih;
		NS_ASSERT(ih.nhop <= IntHeader::maxHop);
		for (uint32_t i = 0; i < ih.nhop; i++)
			qp->hp.hop[i] = ih.hop[i];
		#if PRINT_LOG
		if (print){
			printf("%lu %s %08x %08x %u %u [%u,%u,%u]", Simulator::Now().GetTimeStep(), fast_react? "fast" : "update", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->hp.m_lastUpdateSeq, ch.ack.seq, next_seq);
			for (uint32_t i = 0; i < ih.nhop; i++)
				printf(" %u %lu %lu", ih.hop[i].GetQlen(), ih.hop[i].GetBytes(), ih.hop[i].GetTime());
			printf("\n");
		}
		#endif
	}else {
		// check packet INT
		IntHeader &ih = ch.ack.ih;
		if (ih.nhop <= IntHeader::maxHop){
			double max_c = 0;
			bool inStable = false;
			#if PRINT_LOG
			if (print)
				printf("%lu %s %08x %08x %u %u [%u,%u,%u]", Simulator::Now().GetTimeStep(), fast_react? "fast" : "update", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->hp.m_lastUpdateSeq, ch.ack.seq, next_seq);
			#endif
			// check each hop
			double U = 0;
			uint64_t dt = 0;
			bool updated[IntHeader::maxHop] = {false}, updated_any = false;
			NS_ASSERT(ih.nhop <= IntHeader::maxHop);
			for (uint32_t i = 0; i < ih.nhop; i++){
				if (m_sampleFeedback){
					if (ih.hop[i].GetQlen() == 0 && fast_react)
						continue;
				}
				updated[i] = updated_any = true;
				#if PRINT_LOG
				if (print)
					printf(" %u(%u) %lu(%lu) %lu(%lu)", ih.hop[i].GetQlen(), qp->hp.hop[i].GetQlen(), ih.hop[i].GetBytes(), qp->hp.hop[i].GetBytes(), ih.hop[i].GetTime(), qp->hp.hop[i].GetTime());
				#endif
				uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);;
				double duration = tau * 1e-9;
				double txRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8 / duration;
				double u = txRate / ih.hop[i].GetLineRate() + (double)std::min(ih.hop[i].GetQlen(), qp->hp.hop[i].GetQlen()) * qp->m_max_rate.GetBitRate() / ih.hop[i].GetLineRate() /qp->m_win;
				#if PRINT_LOG
				if (print)
					printf(" %.3lf %.3lf", txRate, u);
				#endif
				if (!m_multipleRate){
					// for aggregate (single R)
					if (u > U){
						U = u;
						dt = tau;
					}
				}else {
					// for per hop (per hop R)
					if (tau > qp->m_baseRtt)
						tau = qp->m_baseRtt;
					qp->hp.hopState[i].u = (qp->hp.hopState[i].u * (qp->m_baseRtt - tau) + u * tau) / double(qp->m_baseRtt);
				}
				qp->hp.hop[i] = ih.hop[i];
			}

			DataRate new_rate;
			int32_t new_incStage;
			DataRate new_rate_per_hop[IntHeader::maxHop];
			int32_t new_incStage_per_hop[IntHeader::maxHop];
			if (!m_multipleRate){
				// for aggregate (single R)
				if (updated_any){
					if (dt > qp->m_baseRtt)
						dt = qp->m_baseRtt;
					qp->hp.u = (qp->hp.u * (qp->m_baseRtt - dt) + U * dt) / double(qp->m_baseRtt);
					max_c = qp->hp.u / m_targetUtil;

					if (max_c >= 1 || qp->hp.m_incStage >= m_miThresh){
						new_rate = qp->hp.m_curRate / max_c + m_rai;
						new_incStage = 0;
					}else{
						new_rate = qp->hp.m_curRate + m_rai;
						new_incStage = qp->hp.m_incStage+1;
					}
					if (new_rate < m_minRate)
						new_rate = m_minRate;
					if (new_rate > qp->m_max_rate)
						new_rate = qp->m_max_rate;
					#if PRINT_LOG
					if (print)
						printf(" u=%.6lf U=%.3lf dt=%u max_c=%.3lf", qp->hp.u, U, dt, max_c);
					#endif
					#if PRINT_LOG
					if (print)
						printf(" rate:%.3lf->%.3lf\n", qp->hp.m_curRate.GetBitRate()*1e-9, new_rate.GetBitRate()*1e-9);
					#endif
				}
			}else{
				// for per hop (per hop R)
				new_rate = qp->m_max_rate;
				for (uint32_t i = 0; i < ih.nhop; i++){
					if (updated[i]){
						double c = qp->hp.hopState[i].u / m_targetUtil;
						if (c >= 1 || qp->hp.hopState[i].incStage >= m_miThresh){
							new_rate_per_hop[i] = qp->hp.hopState[i].Rc / c + m_rai;
							new_incStage_per_hop[i] = 0;
						}else{
							new_rate_per_hop[i] = qp->hp.hopState[i].Rc + m_rai;
							new_incStage_per_hop[i] = qp->hp.hopState[i].incStage+1;
						}
						// bound rate
						if (new_rate_per_hop[i] < m_minRate)
							new_rate_per_hop[i] = m_minRate;
						if (new_rate_per_hop[i] > qp->m_max_rate)
							new_rate_per_hop[i] = qp->m_max_rate;
						// find min new_rate
						if (new_rate_per_hop[i] < new_rate)
							new_rate = new_rate_per_hop[i];
						#if PRINT_LOG
						if (print)
							printf(" [%u]u=%.6lf c=%.3lf", i, qp->hp.hopState[i].u, c);
						#endif
						#if PRINT_LOG
						if (print)
							printf(" %.3lf->%.3lf", qp->hp.hopState[i].Rc.GetBitRate()*1e-9, new_rate.GetBitRate()*1e-9);
						#endif
					}else{
						if (qp->hp.hopState[i].Rc < new_rate)
							new_rate = qp->hp.hopState[i].Rc;
					}
				}
				#if PRINT_LOG
				printf("\n");
				#endif
			}
			if (updated_any)
				ChangeRate(qp, new_rate);
			if (!fast_react){
				if (updated_any){
					qp->hp.m_curRate = new_rate;
					qp->hp.m_incStage = new_incStage;
				}
				if (m_multipleRate){
					// for per hop (per hop R)
					for (uint32_t i = 0; i < ih.nhop; i++){
						if (updated[i]){
							qp->hp.hopState[i].Rc = new_rate_per_hop[i];
							qp->hp.hopState[i].incStage = new_incStage_per_hop[i];
						}
					}
				}
			}
		}
		if (!fast_react){
			if (next_seq > qp->hp.m_lastUpdateSeq)
				qp->hp.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
		}
	}
}

void RdmaHw::FastReactHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch){
	if (m_fast_react)
		UpdateRateHp(qp, p, ch, true);
}

/**********************
 * TIMELY
 *********************/
void RdmaHw::HandleAckTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch){
	uint64_t ack_seq = ch.ack.seq;
	// update rate
	if (ack_seq > qp->tmly.m_lastUpdateSeq){ // if full RTT feedback is ready, do full update
		UpdateRateTimely(qp, p, ch, false);
	}else{ // do fast react
		FastReactTimely(qp, p, ch);
	}
}
void RdmaHw::UpdateRateTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool us){
	uint64_t next_seq = qp->snd_nxt;
	uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;
	bool print = !us;
	if (qp->tmly.m_lastUpdateSeq != 0){ // not first RTT
		int64_t new_rtt_diff = (int64_t)rtt - (int64_t)qp->tmly.lastRtt;
		double rtt_diff = (1 - m_tmly_alpha) * qp->tmly.rttDiff + m_tmly_alpha * new_rtt_diff;
		double gradient = rtt_diff / m_tmly_minRtt;
		bool inc = false;
		double c = 0;
		#if PRINT_LOG
		if (print)
			printf("%lu node:%u rtt:%lu rttDiff:%.0lf gradient:%.3lf rate:%.3lf", Simulator::Now().GetTimeStep(), m_node->GetId(), rtt, rtt_diff, gradient, qp->tmly.m_curRate.GetBitRate() * 1e-9);
		#endif
		if (rtt < m_tmly_TLow){
			inc = true;
		}else if (rtt > m_tmly_THigh){
			c = 1 - m_tmly_beta * (1 - (double)m_tmly_THigh / rtt);
			inc = false;
		}else if (gradient <= 0){
			inc = true;
		}else{
			c = 1 - m_tmly_beta * gradient;
			if (c < 0)
				c = 0;
			inc = false;
		}
		if (inc){
			if (qp->tmly.m_incStage < 5){
				qp->m_rate = qp->tmly.m_curRate + m_rai;
			}else{
				qp->m_rate = qp->tmly.m_curRate + m_rhai;
			}
			if (qp->m_rate > qp->m_max_rate)
				qp->m_rate = qp->m_max_rate;
			if (!us){
				qp->tmly.m_curRate = qp->m_rate;
				qp->tmly.m_incStage++;
				qp->tmly.rttDiff = rtt_diff;
			}
		}else{
			qp->m_rate = std::max(m_minRate, qp->tmly.m_curRate * c);
			if (!us){
				qp->tmly.m_curRate = qp->m_rate;
				qp->tmly.m_incStage = 0;
				qp->tmly.rttDiff = rtt_diff;
			}
		}
		#if PRINT_LOG
		if (print){
			printf(" %c %.3lf\n", inc? '^':'v', qp->m_rate.GetBitRate() * 1e-9);
		}
		#endif
	}
	if (!us && next_seq > qp->tmly.m_lastUpdateSeq){
		qp->tmly.m_lastUpdateSeq = next_seq;
		// update
		qp->tmly.lastRtt = rtt;
	}
}
void RdmaHw::FastReactTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch){
}

/**********************
 * DCTCP
 *********************/
void RdmaHw::HandleAckDctcp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch){
	uint64_t ack_seq = ch.ack.seq;
	uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
	bool new_batch = false;

	// update alpha
	qp->dctcp.m_ecnCnt += (cnp > 0);
	if (ack_seq > qp->dctcp.m_lastUpdateSeq){ // if full RTT feedback is ready, do alpha update
		#if PRINT_LOG
		printf("%lu %s %08x %08x %u %u [%u,%u,%u] %.3lf->", Simulator::Now().GetTimeStep(), "alpha", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->dctcp.m_lastUpdateSeq, ch.ack.seq, qp->snd_nxt, qp->dctcp.m_alpha);
		#endif
		new_batch = true;
		if (qp->dctcp.m_lastUpdateSeq == 0){ // first RTT
			qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
			qp->dctcp.m_batchSizeOfAlpha = qp->snd_nxt / m_mtu + 1;
		}else {
			double frac = std::min(1.0, double(qp->dctcp.m_ecnCnt) / qp->dctcp.m_batchSizeOfAlpha);
			qp->dctcp.m_alpha = (1 - m_g) * qp->dctcp.m_alpha + m_g * frac;
			qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
			qp->dctcp.m_ecnCnt = 0;
			qp->dctcp.m_batchSizeOfAlpha = (qp->snd_nxt - ack_seq) / m_mtu + 1;
			#if PRINT_LOG
			printf("%.3lf F:%.3lf", qp->dctcp.m_alpha, frac);
			#endif
		}
		#if PRINT_LOG
		printf("\n");
		#endif
	}

	// check cwr exit
	if (qp->dctcp.m_caState == 1){
		if (ack_seq > qp->dctcp.m_highSeq)
			qp->dctcp.m_caState = 0;
	}

	// check if need to reduce rate: ECN and not in CWR
	if (cnp && qp->dctcp.m_caState == 0){
		#if PRINT_LOG
		printf("%lu %s %08x %08x %u %u %.3lf->", Simulator::Now().GetTimeStep(), "rate", qp->sip.Get(), qp->dip.Get(), qp->sport, qp->dport, qp->m_rate.GetBitRate()*1e-9);
		#endif
		qp->m_rate = std::max(m_minRate, qp->m_rate * (1 - qp->dctcp.m_alpha / 2));
		#if PRINT_LOG
		printf("%.3lf\n", qp->m_rate.GetBitRate() * 1e-9);
		#endif
		qp->dctcp.m_caState = 1;
		qp->dctcp.m_highSeq = qp->snd_nxt;
	}

	// additive inc
	if (qp->dctcp.m_caState == 0 && new_batch)
		qp->m_rate = std::min(qp->m_max_rate, qp->m_rate + m_dctcp_rai);
}

/*********************
 * HPCC-PINT
 ********************/
void RdmaHw::SetPintSmplThresh(double p){
       pint_smpl_thresh = (uint32_t)(65536 * p);
}
void RdmaHw::HandleAckHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch){
       uint64_t ack_seq = ch.ack.seq;
       if (rand() % 65536 >= pint_smpl_thresh)
               return;
       // update rate
       if (ack_seq > qp->hpccPint.m_lastUpdateSeq){ // if full RTT feedback is ready, do full update
               UpdateRateHpPint(qp, p, ch, false);
       }else{ // do fast react
               UpdateRateHpPint(qp, p, ch, true);
       }
}

void RdmaHw::UpdateRateHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, bool fast_react){
       uint64_t next_seq = qp->snd_nxt;
       if (qp->hpccPint.m_lastUpdateSeq == 0){ // first RTT
               qp->hpccPint.m_lastUpdateSeq = next_seq;
       }else {
               // check packet INT
               IntHeader &ih = ch.ack.ih;
               double U = Pint::decode_u(ih.GetPower());

               DataRate new_rate;
               int32_t new_incStage;
               double max_c = U / m_targetUtil;

               if (max_c >= 1 || qp->hpccPint.m_incStage >= m_miThresh){
                       new_rate = qp->hpccPint.m_curRate / max_c + m_rai;
                       new_incStage = 0;
               }else{
                       new_rate = qp->hpccPint.m_curRate + m_rai;
                       new_incStage = qp->hpccPint.m_incStage+1;
               }
               if (new_rate < m_minRate)
                       new_rate = m_minRate;
               if (new_rate > qp->m_max_rate)
                       new_rate = qp->m_max_rate;
               ChangeRate(qp, new_rate);
               if (!fast_react){
                       qp->hpccPint.m_curRate = new_rate;
                       qp->hpccPint.m_incStage = new_incStage;
               }
               if (!fast_react){
                       if (next_seq > qp->hpccPint.m_lastUpdateSeq)
                               qp->hpccPint.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
               }
       }
}

/*********************
 * Swift
 ********************/
double RdmaHw::SwiftAlpha(Ptr<RdmaQueuePair> qp) {
	if (m_swift_alpha != 0)
		return m_swift_alpha;

	double sqrt1 = 1.0 / std::sqrt(m_swift_maxCwnd);
	double sqrt2 = 1.0 / std::sqrt(m_swift_minCwnd);
	double result = m_swift_range / (sqrt1 - sqrt2);
	m_swift_alpha = result;
	return result;
}

double RdmaHw::SwiftBeta(Ptr<RdmaQueuePair> qp) {
	if (m_swift_beta != 0)
		return m_swift_beta;

	double result = -SwiftAlpha(qp) / m_swift_maxCwnd;
	m_swift_beta = result;
	return result;
}

double RdmaHw::SwiftGetTargetDelay(Ptr<Packet> p, Ptr<RdmaQueuePair> qp, CustomHeader &ch) {
	uint64_t number_of_hops = 1;
	SwiftHopTag hop;
	if (p->PeekPacketTag(hop)) {
		number_of_hops = hop.GetHops();
	}
	
	uint64_t base_rtt = qp->m_baseRtt;
    double cwnd_pkts = qp->swift.m_cwnd_bytes / (double)m_mtu;
    double result = SwiftAlpha(qp) / cwnd_pkts + SwiftBeta(qp);
	result = std::min(result, m_swift_range);
	result = std::max(result, 0.0);
	return base_rtt + m_swift_perHopScalingFactor * number_of_hops + result;
}

bool RdmaHw::SwiftCanDecrease(Ptr<RdmaQueuePair> qp) {
	return Simulator::Now().GetTimeStep() - qp->swift.m_last_decrease.GetTimeStep() >= qp->m_baseRtt;
}

void RdmaHw::HandleAckSwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader &ch, uint64_t num_bytes_acked) {
	if (ch.l3Prot == 0xFD) {
		qp->swift.retransmit_cnt = 0;
		
		if (SwiftCanDecrease(qp)) {
			qp->swift.m_cwnd_bytes_past = qp->swift.m_cwnd_bytes;
			qp->swift.m_cwnd_bytes = (1.0 - m_swift_maxMdFactor) * qp->swift.m_cwnd_bytes;
			SwiftEnforceUpperBound(qp);
		}

		return;
	}
	
	qp->swift.m_cwnd_bytes_past = qp->swift.m_cwnd_bytes;

	double target_delay = SwiftGetTargetDelay(p, qp, ch);
	double delay = (double)(Simulator::Now().GetTimeStep() - ch.ack.ih.ts);

    // convert to packets acked for AI calculation
    double pkts_acked = (double)num_bytes_acked / (double)m_mtu;
    if (delay < target_delay) {
        if (qp->swift.m_cwnd_bytes >= m_mtu) {
            // AI: delta_cwnd_bytes = ai * (packets_acked / cwnd_pkts)
            double cwnd_pkts = qp->swift.m_cwnd_bytes / (double)m_mtu;
            qp->swift.m_cwnd_bytes += m_swift_ai * (pkts_acked / cwnd_pkts) * m_mtu;
        } else {
            // below 1 packet: grow linearly by ai per packet acked, in bytes
            qp->swift.m_cwnd_bytes += m_swift_ai * pkts_acked * m_mtu;
        }
	} else if (SwiftCanDecrease(qp)) {
		// Compute proportional multiplicative decrease and cap it
		double md = m_swift_mdGain * (delay - target_delay) / delay;
		if (md > m_swift_maxMdFactor) 
			md = m_swift_maxMdFactor;
		qp->swift.m_cwnd_bytes = (1.0 - md) * qp->swift.m_cwnd_bytes;
	}
    
	// enforce bounds, update t_last_decrease, and compute pacing + window
    SwiftEnforceUpperBound(qp);
}

void RdmaHw::SwiftEnforceUpperBound(Ptr<RdmaQueuePair> qp) {
    // clamp cwnd in bytes using min/max in packets
    double min_bytes = m_swift_minCwnd * (double)m_mtu;
    double max_bytes = m_swift_maxCwnd * (double)m_mtu;
    if (qp->swift.m_cwnd_bytes > max_bytes)
        qp->swift.m_cwnd_bytes = max_bytes;
    if (qp->swift.m_cwnd_bytes < min_bytes)
        qp->swift.m_cwnd_bytes = min_bytes;
    if(qp->swift.m_cwnd_bytes < qp->swift.m_cwnd_bytes_past)
		qp->swift.m_last_decrease = Simulator::Now();
    
    if (qp->swift.m_cwnd_bytes < m_mtu) {
		qp->swift.m_pacing_delay = qp->m_last_sent + NanoSeconds(qp->m_baseRtt / qp->swift.m_cwnd_bytes * (double)m_mtu);
		qp->SetWin(1); 
    } else {
        qp->swift.m_pacing_delay = Time(0);
        qp->SetWin((uint32_t)qp->swift.m_cwnd_bytes);
    }
}

/****************************
 * Retransmission Management
 ****************************/
void RdmaHw::RestartSenderTimer(Ptr<RdmaQueuePair> qp) {
	/* We only schedule retransmissions if the timeout is > 0 and the QP is lossy (lossless does not need it) */
	if (m_retransmissionTimeout > 0 && qp->m_isLossy) {
		/* Cancel existing timer if running */
		CancelSenderTimer(qp);

		Time delta;
		bool has = qp->GetEarliestTimeout(MicroSeconds(m_retransmissionTimeout), delta);
		if (!has) {
			return;
		} 

		/* Add a 20% of skewing to the time so we do not end up syncing! */
		const double skewFrac = 0.2;
		double u = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
		double factor = 1.0 + skewFrac * u;
		Time skewedDelta = NanoSeconds(delta.GetNanoSeconds() * factor);
    	qp->m_senderTimer = Simulator::Schedule(skewedDelta, &RdmaHw::SenderTimeoutHandler, this, qp);
	}
}

void RdmaHw::CancelSenderTimer(Ptr<RdmaQueuePair> qp) {
	// std::cout << Simulator::Now().GetTimeStep() << " "
	// 			  << "[SENDER]  [TIME] "
	// 			  << "[" << Ipv4Address(qp->sip) << "(" << qp->sport 
	// 			  << ") --> " << Ipv4Address(qp->dip) << "(" << qp->dport << ")] "
	// 			  << "Cancel sender timer" << std::endl;

	if (qp->m_senderTimer.IsRunning()) {
		Simulator::Cancel(qp->m_senderTimer);
	}
}

void RdmaHw::SenderTimeoutHandler(Ptr<RdmaQueuePair> qp) {
	// std::cout << Simulator::Now().GetTimeStep() << " "
	// 			<< "[SENDER]  [TIME] "
	// 			<< "[" << Ipv4Address(qp->dip) << "(" << qp->dport 
	// 			<< ") --> " << Ipv4Address(qp->sip) << "(" << qp->sport << ")] "
	// 			<< "Timeout sender timer" << std::endl;

	Time now = Simulator::Now();
    Time rto = MicroSeconds(m_retransmissionTimeout);

	if (m_cc_mode == 11) { // Swift
		qp->swift.retransmit_cnt++;
		
		if (qp->swift.retransmit_cnt >= m_swift_retxResetThreshold) {
			qp->swift.m_cwnd_bytes_past = qp->swift.m_cwnd_bytes;
			qp->swift.m_cwnd_bytes = m_swift_minCwnd * (double)m_mtu;
			SwiftEnforceUpperBound(qp);
		} else if (SwiftCanDecrease(qp)) {
			qp->swift.m_cwnd_bytes_past = qp->swift.m_cwnd_bytes;
			qp->swift.m_cwnd_bytes = (1.0 - m_swift_maxMdFactor) * qp->swift.m_cwnd_bytes;
			SwiftEnforceUpperBound(qp);
		}
	}

	if (m_rtx == 0 && !m_oooReorderEnable) {
        /* GBN Retransmission */
        if (!qp->m_txSendTime.empty()) {
            /* Find timestamp for snd_una or the first >= snd_una */
            auto it = qp->m_txSendTime.lower_bound(qp->snd_una);
            if (it != qp->m_txSendTime.end()) {
                Time sent = it->second;
                if (now - sent >= rto) {
                    RecoverQueue(qp);
                }
            }
        }
    } else {
		/* Selective Repeat or OOO Reorder */
		/* For each outstanding PSN, check if its own timer expired */
		for (auto &kv : qp->m_txSendTime) {
			uint64_t seq = kv.first;
			Time sent = kv.second;

			if (seq < qp->snd_una) { /* Only consider unacked */
				continue;
			}

			if (now - sent >= rto) {
				/* Mark for retransmission if not already queued */
				if (qp->m_retransmissionBuffer.count(seq) == 0) {
					qp->PopulateRetransmissionBuffer(seq);
				}

				/* Slide PSN timestamp forward so we do not hammer it with immediate timeouts */
				kv.second = now;
			}
		}
	}

	RestartSenderTimer(qp);

	/* Trigger transmission to send retransmitted packets */
	uint32_t nic_idx = GetNicIdxOfQp(qp);
	m_nic[nic_idx].dev->TriggerTransmit();
}

/******************************
 * OOO Reorder Management
 ******************************/
std::tuple<int, uint64_t, uint64_t> RdmaHw::OOOReorderCheck(Ptr<RdmaRxQueuePair> rxQp, uint64_t seq, uint32_t payload_size) {
	auto logAndReturn = [&](int code, uint64_t ack, uint64_t nack) {
		// std::cout
		// 	<< Simulator::Now().GetTimeStep() << " "
		// 	<< "[RECEIVER] [INFO]"
		// 	<< "[" << Ipv4Address(rxQp->sip) << "(" << rxQp->sport
		// 	<< ") --> " << Ipv4Address(rxQp->dip) << "(" << rxQp->dport << ")] "
		// 	<< "[OOOReorderCheck] "
		// 	<< "code=" << code
		// 	<< " ackSeq=" << ack
		// 	<< " nackSeq=NULL"
		// 	<< " expected=" << rxQp->ReceiverNextExpectedSeq
		// 	<< " last=" << rxQp->ReceiverLastExpectedSeq
		// 	<< " highest=" << rxQp->HighestSeqno
		// 	<< std::endl;

        return std::make_tuple(code, ack, nack);
    };

    if (payload_size == 0) {
		return logAndReturn(4, 0, 0);
    }
	
	uint64_t oldNext = rxQp->ReceiverNextExpectedSeq;
    uint64_t lastAck = rxQp->m_lastAckedSeq;

	/* Check if we already had out-of-order data buffered beyond oldNext. */
	/* This should trigger an ACK even if < m_ack_interval */
    bool hadAheadBefore = false;
    auto itAhead = rxQp->m_rxBuffer.upper_bound(oldNext);
    if (itAhead != rxQp->m_rxBuffer.end()) {
        hadAheadBefore = true;
    }
	
	/* First time we receive this packet, place in RX buffer */
    if (!rxQp->CheckPsnExists(seq)) {
        rxQp->AddPsnToList(seq, payload_size);
    }

    /* Try to advance cumulative "delivered" pointer */
    uint64_t delivered = oldNext;
    while (true) {
        auto it = rxQp->m_rxBuffer.find(delivered);
        if (it == rxQp->m_rxBuffer.end()) {
            break;  /* Hole or end of known range */
        }
        uint32_t segSize = it->second;
        delivered += segSize;
    }

    bool advanced = (delivered > oldNext);
    if (!advanced) {
        return logAndReturn(1, oldNext, 0);
    }

    rxQp->ReceiverLastExpectedSeq = oldNext;
    rxQp->ReceiverNextExpectedSeq = delivered;
    rxQp->DeleteBelowPsn(rxQp->ReceiverNextExpectedSeq);

	uint64_t newNext = rxQp->ReceiverNextExpectedSeq;

    if (newNext <= lastAck) {
		/* Send duplicate ACK */
    	return logAndReturn(1, lastAck, 0);
    }

	rxQp->m_lastAckedSeq = newNext;

	return logAndReturn(1, newNext, 0);
}

}
