#ifndef SWITCH_NODE_H
#define SWITCH_NODE_H

#include <unordered_map>
#include <ns3/node.h>
#include <ns3/random-variable.h>
#include <ns3/themis-loop-tag.h>
#include <ns3/swift-hop-tag.h>
#include "qbb-net-device.h"
#include "switch-mmu.h"
#include "pint.h"

/* =============
Improved SwitchNode class to support Nvidia-like per-packet ECMP Adaptive Routing and Fast NACKs (DCP-like) by Mariano Scazzariello.
Sponge Deflection implementation by Mariano Scazzariello, PD-Quantile was implemented by Alexandra Udrescu.
Themis implementation was imported/polished from original artifact: https://github.com/Networked-System-and-Security-Group/Themis/
================ */

namespace ns3 {

class Packet;

struct CnpKey {
	uint32_t a, b;
	uint16_t pg, dport, sport;

  	CnpKey(uint32_t a_=0, uint32_t b_=0, uint16_t pg_=0, uint16_t dport_=0, uint16_t sport_=0)
		: a(a_), b(b_), pg(pg_), dport(dport_), sport(sport_) {}

	bool operator<(const CnpKey &o) const {
		if (a != o.a) return a < o.a;
		if (b != o.b) return b < o.b;
		if (pg != o.pg) return pg < o.pg;
		if (dport != o.dport) return dport < o.dport;
		return sport < o.sport;
	}
};

struct CnpHandler {
	uint32_t cnp_num = 0;
	uint32_t alpha = 1;
	uint32_t loop_num = 0;
	uint32_t biggest = 0;
	Time rec_time = Seconds(0);
	Time set_last_loop = Seconds(0);
	std::map<int32_t, int32_t> recover;
	uint32_t recovered = 0;
};

class SwitchNode : public Node{
	static const uint32_t pCnt = 1025;	// Number of ports used
	static const uint32_t qCnt = 8;	// Number of queues/priorities used
	uint32_t m_ecmpSeed;
	std::unordered_map<uint32_t, std::vector<int> > m_rtTable; // map from ip address (u32) to possible ECMP port (index of dev)
	std::set<uint32_t> active_ports;	// record active ports in switch

	// monitor of PFC
	uint32_t m_bytes[pCnt][pCnt][qCnt]; // m_bytes[inDev][outDev][qidx] is the bytes from inDev enqueued for outDev at qidx
	
	uint64_t m_txBytes[pCnt]; // counter of tx bytes

	uint32_t m_lastPktSize[pCnt];
	uint64_t m_lastPktTs[pCnt]; // ns
	double m_u[pCnt];

	/* PD-Quantile: per-egress histogram of remaining bytes for UDP packets */
	std::map<uint64_t, uint32_t> m_bytesLeftDist[pCnt][qCnt];
	uint32_t m_totalUdpPkts[pCnt][qCnt];

protected:
	bool m_ecnEnabled;
	uint32_t m_ccMode;
	uint64_t m_maxRtt;

	uint32_t m_ackHighPrio; // set high priority for ACK/NACK

	/* Per-Packet ECMP knobs */
	bool m_pktEcmpEnable;
    uint64_t m_pktEcmpQlenThresholdBytes;
    uint64_t m_pktEcmpHysteresisBytes;

	/* Fast NACK knob */
	bool m_fastNackEnable;

	/* Sponge knobs */
	bool m_spongeEnable;
	std::vector<Ipv4Address> m_spongeIps;

	/* PD-Quantile knobs */
	bool m_pdQuantileDeflectionEnable;
	double m_pdQuantileAlphaThreshold;
	bool m_pdDeflectionCandidatesInit = false;
	std::vector<int> m_pdDeflectionCandidates;
	UniformVariable m_randomVariable;
	TracedCallback<uint8_t, uint32_t, Ptr<const Packet>, int, int, uint32_t> m_pdDeflectCb;

	/* Themis Knobs */
	bool m_themisEnable;
	Time m_cnpMinGap = NanoSeconds(5);
	Time m_cnpRecoverWindow = MicroSeconds(500);
	Time m_cnpSendRateLimit = MicroSeconds(5);
	std::map<CnpKey, CnpHandler> m_cnp_handler;
	std::map<CnpKey, Time> m_ecn_detector;
 	int m_recirculationIndex = pCnt;

private:
    int GetOutDev(Ptr<const Packet>, CustomHeader &ch);
	/* Per-Flow ECMP */
	int GetOutDevFlowEcmp(Ptr<const Packet>, CustomHeader &ch);
    /* Per-Packet ECMP */
	int GetOutDevPktEcmp(Ptr<const Packet>, CustomHeader &ch);
	/* Helper to get Port Bytes */
    uint64_t GetPortQlenBytes(uint32_t outDev) const;

	void SendToDev(Ptr<Packet>p, CustomHeader &ch);
	static uint32_t EcmpHash(const uint8_t* key, size_t len, uint32_t seed);
	void CheckAndSendPfc(uint32_t inDev, uint32_t qIndex);
	void CheckAndSendResume(uint32_t inDev, uint32_t qIndex);

	/* Fast NACK generation */
	Ptr<Packet> GenFastNack(Ptr<Packet> p, CustomHeader &ch);
	/* Sponge Packet generation */
	Ptr<Packet> DoSpongePacket(Ptr<Packet> ori_pkt, CustomHeader &ch);
	/* PD-Quantile */
	double PDComputeQuantile(Ptr<Packet> p, uint32_t dev, uint32_t qIndex);
	bool PDShouldDeflect(Ptr<Packet> p, uint32_t dev, uint32_t qIndex);
	/* Themis */
  	int ReceiveCnp(Ptr<Packet> p, CustomHeader &ch);
public:
	Ptr<SwitchMmu> m_mmu;

	static TypeId GetTypeId (void);
	SwitchNode();
	void SetEcmpSeed(uint32_t seed);
	void AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx);
	void ClearTable();
	bool SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch);
	void SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p);

	// for approximate calc in PINT
	int logres_shift(int b, int l);
	int log2apprx(int x, int b, int m, int l); // given x of at most b bits, use most significant m bits of x, calc the result in l bits
	
	// for monitor
	uint64_t last_txBytes[pCnt]; // last sampling of the counter of tx bytes
	uint64_t last_port_qlen[pCnt]; // last sampling of the port length
	
	/**
	 * outoput format:
	 * time, sw_id, port_id, q_id, qlen, port_len
	 */
	void PrintSwitchQlen(FILE* qlen_output);
	/**
	 * outoput format:
	 * time, sw_id, port_id, txBytes
	 */
	void PrintSwitchBw(FILE* bw_output, uint32_t bw_mon_interval);

	/* Themis */
	void SetRecirculationPort(int idx) { m_recirculationIndex = idx; }

	/* Sponge */
	void SetSpongeIps(std::vector<Ipv4Address> spongeIps) { m_spongeIps = spongeIps; }
};

} /* namespace ns3 */

#endif /* SWITCH_NODE_H */
