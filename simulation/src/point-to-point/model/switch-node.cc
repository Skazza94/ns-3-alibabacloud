#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/qbb-header.h"
#include "ns3/pause-header.h"
#include "ns3/flow-id-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "switch-node.h"
#include "qbb-net-device.h"
#include "ppp-header.h"
#include "ns3/int-header.h"
#include "ns3/simulator.h"
#include "ns3/priority-tag.h"
#include "ns3/simple-seq-ts-header.h"

#include <cmath>

namespace ns3 {

TypeId SwitchNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SwitchNode")
    .SetParent<Node> ()
    .AddConstructor<SwitchNode> ()
	.AddAttribute("EcnEnabled",
			"Enable ECN marking.",
			BooleanValue(false),
			MakeBooleanAccessor(&SwitchNode::m_ecnEnabled),
			MakeBooleanChecker())
	.AddAttribute("CcMode",
			"CC mode.",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ccMode),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("AckHighPrio",
			"Set high priority for ACK/NACK or not",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("MaxRtt",
			"Max Rtt of the network",
			UintegerValue(9000),
			MakeUintegerAccessor(&SwitchNode::m_maxRtt),
			MakeUintegerChecker<uint32_t>())
    .AddAttribute("PktEcmpEnable",
			"Enable per-packet ECMP.",
			BooleanValue(false),
			MakeBooleanAccessor(&SwitchNode::m_pktEcmpEnable),
			MakeBooleanChecker())
    .AddAttribute("PktEcmpQlenThresholdBytes",
			"Queue length threshold (bytes) above which per-packet ECMP may deflect packets.",
			UintegerValue(16384),
			MakeUintegerAccessor(&SwitchNode::m_pktEcmpQlenThresholdBytes),
			MakeUintegerChecker<uint64_t>())
    .AddAttribute("PktEcmpHysteresisBytes",
        	"Minimum queue-length advantage (bytes) a candidate port must have to override base ECMP.",
			UintegerValue(4096),
			MakeUintegerAccessor(&SwitchNode::m_pktEcmpHysteresisBytes),
			MakeUintegerChecker<uint64_t>())
	.AddAttribute("FastNackEnable",
			"Enable the generation of a NACK instead of dropping a packet (DCP-like).",
			BooleanValue(false),
			MakeBooleanAccessor(&SwitchNode::m_fastNackEnable),
			MakeBooleanChecker())
	.AddAttribute("SpongeEnable",
			"Enable the deflection instead of dropping a packet.",
			BooleanValue(false),
			MakeBooleanAccessor(&SwitchNode::m_spongeEnable),
			MakeBooleanChecker())
  ;
  return tid;
}

SwitchNode::SwitchNode(){
	m_ecmpSeed = m_id;
	m_node_type = 1;
	m_mmu = CreateObject<SwitchMmu>();
	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < pCnt; j++)
			for (uint32_t k = 0; k < qCnt; k++)
				m_bytes[i][j][k] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_txBytes[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_lastPktSize[i] = m_lastPktTs[i] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_u[i] = 0;
}

uint64_t SwitchNode::GetPortQlenBytes(uint32_t outDev) const {
    if (!m_mmu) {
        return 0;
    }
    if (outDev >= pCnt) {
        return 0;
    }

    uint64_t port_len = 0;
    for (uint32_t j = 0; j < qCnt; ++j) {
        port_len += m_mmu->egress_bytes[outDev][j];
    }

    return port_len;
}

int SwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch) {
	if (!m_pktEcmpEnable) {
		/* Default, do per-flow ECMP */
        return GetOutDevFlowEcmp(p, ch);
    }
	/* Do per-packet ECMP */
    return GetOutDevPktEcmp(p, ch);
}

int SwitchNode::GetOutDevFlowEcmp(Ptr<const Packet> p, CustomHeader &ch) {
	// look up entries
	auto entry = m_rtTable.find(ch.dip);

	// no matching entry
	if (entry == m_rtTable.end())
		return -1;

	// entry found
	auto &nexthops = entry->second;

	// pick one next hop based on hash
	union {
		uint8_t u8[4+4+2+2];
		uint32_t u32[3];
	} buf;
	buf.u32[0] = ch.sip;
	buf.u32[1] = ch.dip;
	if (ch.l3Prot == 0x6)
		buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
	else if (ch.l3Prot == 0x11)
		buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
	else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
		buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);

	uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % nexthops.size();
	return nexthops[idx];
}

int SwitchNode::GetOutDevPktEcmp(Ptr<const Packet> p, CustomHeader &ch) {
    auto entry = m_rtTable.find(ch.dip);
    if (entry == m_rtTable.end()) {
        return -1;
    }

    auto &nexthops = entry->second;
    if (nexthops.empty()) {
        return -1;
    }	
    if (nexthops.size() == 1) {
        /* Single path: no point in doing per-packet */
        return nexthops[0];
    }

	/* Get default per-flow ECMP port */
	int basePort = GetOutDevFlowEcmp(p, ch);

    /* Apply per-packet only on data packets, so if it is a control packet, return per-flow ECMP */
    if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || ch.l3Prot == 0xFD || ch.l3Prot == 0xFC) {
        return basePort;
    }

	/* Get congestion of per-flow ECMP port */
    uint64_t baseCost = GetPortQlenBytes(basePort);
	
	/* Still ok, do not deflect */
    if (baseCost <= m_pktEcmpQlenThresholdBytes) {
        return basePort;
    }

    /* Congested, scan candidates for a significantly better port */
    uint32_t bestPort = basePort;
    uint64_t bestCost = baseCost;
    for (uint32_t i = 0; i < nexthops.size(); ++i) {
        uint32_t candPort = nexthops[i];
        uint64_t candCost = GetPortQlenBytes(candPort);
        if (candCost < bestCost) {
            bestCost = candCost;
            bestPort = candPort;
        }
    }

    /* No better port, return */
    if (bestPort == basePort) {
        return basePort;
    }

    /* Change only if cost is much more better (also considering the hysteresis)! */
    if (baseCost > bestCost + m_pktEcmpHysteresisBytes) {
        return bestPort;
    } else {
        return basePort;
    }
}

Ptr<Packet> SwitchNode::GenFastNack(CustomHeader &ch) {
	qbbHeader seqh;
	seqh.SetSeq(ch.udp.seq);
	seqh.SetPG(ch.udp.pg);
	seqh.SetSport(ch.udp.dport);
	seqh.SetDport(ch.udp.sport);
	seqh.SetIntHeader(ch.udp.ih);
	seqh.SetCnp();	/* Always piggyback a CNP since we want to reduce the rate! */

	Ptr<Packet> newp = Create<Packet>(std::max(60-14-20-(int)seqh.GetSerializedSize(), 0));
	newp->AddHeader(seqh);

	Ipv4Header ipv4;
	ipv4.SetDestination(Ipv4Address(ch.sip));
	ipv4.SetSource(Ipv4Address(ch.dip));
	ipv4.SetProtocol(0xFD); // nack=0xFD
	ipv4.SetTtl(64);
	ipv4.SetPayloadSize(newp->GetSize());
	ipv4.SetIdentification(ch.ipid);
	newp->AddHeader(ipv4);

	PppHeader ppp;
	ppp.SetProtocol(0x0021);
	newp->AddHeader(ppp);

    return newp;
}

Ptr<Packet> SwitchNode::DoSpongePacket(Ptr<Packet> ori_pkt, CustomHeader &ch) {
	/* Perform ECMP and pick a sponge, very naive selection */
	union {
		uint8_t u8[4+4+2+2];
		uint32_t u32[3];
	} buf;
	buf.u32[0] = ch.sip;
	buf.u32[1] = ch.dip;
	buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
	uint32_t idx = EcmpHash(buf.u8, 12, m_ecmpSeed) % m_spongeIps.size();

	uint32_t payload_size = ori_pkt->GetSize() - ch.GetSerializedSize();
	Ptr<Packet> newp = Create<Packet>(payload_size);

	SpongeHeader sh;
	sh.m_enabled = 1;
	sh.m_osip = ch.sip;
	sh.m_odip = ch.dip;
	sh.m_osport = ch.udp.sport;
	sh.m_odport = ch.udp.dport;
	sh.m_opg = ch.udp.pg;

	SimpleSeqTsHeader seqTs;
	seqTs.SetSeq(ch.udp.seq);
	seqTs.SetPG(4); // Force to lossy, we store the original PG in the SpongeHeader
	seqTs.ih = ch.udp.ih;
	seqTs.sh = sh;
	newp->AddHeader(seqTs);

	UdpHeader udpHeader;
	udpHeader.SetDestinationPort(0xD3F1);
	udpHeader.SetSourcePort((1024 + GetId()) % (1<<16));
	newp->AddHeader(udpHeader);

	Ipv4Header ipv4;
	ipv4.SetDestination(m_spongeIps[idx]);
	ipv4.SetSource(Ipv4Address(0xc8ffff01));
	ipv4.SetProtocol(0x11);
	ipv4.SetTtl(64);
	ipv4.SetPayloadSize(newp->GetSize());
	ipv4.SetIdentification(ch.ipid);
	newp->AddHeader(ipv4);

	PppHeader ppp;
	ppp.SetProtocol(0x0021);
	newp->AddHeader(ppp);

	/* Copy tags */
	ByteTagIterator it = ori_pkt->GetByteTagIterator();
	while (it.HasNext())
	{
		ByteTagIterator::Item tag_item = it.Next();
		Callback<ObjectBase*> constructor = tag_item.GetTypeId().GetConstructor();

		Tag* tag = dynamic_cast<Tag*>(constructor());
		NS_ASSERT(tag != nullptr);
		tag_item.GetTag(*tag);

		newp->AddByteTag(*tag);
	}

	PacketTagIterator pit = ori_pkt->GetPacketTagIterator();
	while (pit.HasNext())
	{
		PacketTagIterator::Item tag_item = pit.Next();
		Callback<ObjectBase*> constructor = tag_item.GetTypeId().GetConstructor();

		Tag* tag = dynamic_cast<Tag*>(constructor());
		NS_ASSERT(tag != nullptr);
		tag_item.GetTag(*tag);

		newp->AddPacketTag(*tag);
	}

    return newp;
}

void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldPause(inDev, qIndex)){
		device->SendPfc(qIndex, 0);
		m_mmu->SetPause(inDev, qIndex);
	}
}
void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldResume(inDev, qIndex)){
		device->SendPfc(qIndex, 1);
		m_mmu->SetResume(inDev, qIndex);
	}
}

void SwitchNode::SendToDev(Ptr<Packet>p, CustomHeader &ch){
	int idx = GetOutDev(p, ch);
	if (idx >= 0){
		NS_ASSERT_MSG(m_devices[idx]->IsLinkUp(), "The routing table look up should return link that is up");

		// determine the qIndex
		uint32_t qIndex;
		PriorityTag prio_tag;
		// IMPORTANT: PriorityTag should only be attached by lossy traffic. This tag indicates the qIndex but also indicates that it is "lossy". Never attach PriorityTag on lossless traffic.
		int type = (int) p->PeekPacketTag(prio_tag);

		if (ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC))){  //QCN or PFC or NACK, go highest priority
			qIndex = 0;
		}
		else if (type == LOSSY) {
			qIndex = prio_tag.GetPriority();
		}
		else {
			qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // if TCP, put to queue 1
		}
		// std::cout << "qIndex is: " << qIndex << std::endl;

		// admission control
		FlowIdTag t;
		p->PeekPacketTag(t);
		uint32_t inDev = t.GetFlowId();
		if (qIndex != 0){ //not highest priority
			if (m_mmu->CheckIngressAdmission(inDev, qIndex, p->GetSize(), type) && m_mmu->CheckEgressAdmission(idx, qIndex, p->GetSize(), type)){			// Admission control
				m_mmu->UpdateIngressAdmission(inDev, qIndex, p->GetSize(), type);
				m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize(), type);
			}else{
				if (m_fastNackEnable && ch.l3Prot == 0x11) {
					/* Fast NACK enable and packet is UDP */
					Ptr<Packet> nack = GenFastNack(ch);
					CustomHeader nack_ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
					nack->PeekHeader(nack_ch);
					/* Send NACK on highest priority queue */
					m_devices[inDev]->SwitchSend(0, nack, nack_ch);
				}

				if (m_spongeEnable && ch.l3Prot == 0x11 && ch.udp.sh.m_enabled == 0) {
					/* Generate the packet for the sponge */
					p = DoSpongePacket(p, ch);
					/* Peek again ch (it changed) */
					p->PeekHeader(ch);

					/* Re-do ECMP towards the sponge */
					idx = GetOutDev(p, ch);

					/* Repeat below since we are prematurely returning */
					m_bytes[inDev][idx][qIndex] += p->GetSize();
					m_devices[idx]->SwitchSend(qIndex, p, ch);
				}

				/* Drop the original packet */
				return;
			}
			CheckAndSendPfc(inDev, qIndex);
			m_bytes[inDev][idx][qIndex] += p->GetSize();
		}
		m_devices[idx]->SwitchSend(qIndex, p, ch);
	}else
	{
		return; // Drop
	}
}

uint32_t SwitchNode::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h += (h << 2) + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

void SwitchNode::SetEcmpSeed(uint32_t seed){
	m_ecmpSeed = seed;
}

void SwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx){
	uint32_t dip = dstAddr.Get();
	m_rtTable[dip].push_back(intf_idx);
}

void SwitchNode::ClearTable(){
	m_rtTable.clear();
}

// This function can only be called in switch mode
bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch){
	SendToDev(packet, ch);
	return true;
}

void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t qIndex, Ptr<Packet> p){
	PriorityTag prio_tag;
	int type = (int) p->PeekPacketTag(prio_tag);

	FlowIdTag t;
	p->PeekPacketTag(t);
	if (qIndex != 0){
		uint32_t inDev = t.GetFlowId();
		m_mmu->RemoveFromIngressAdmission(inDev, qIndex, p->GetSize(), type);
		m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize(), type);
		m_bytes[inDev][ifIndex][qIndex] -= p->GetSize();
		if (m_ecnEnabled){
			bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
			if (egressCongested){
				PppHeader ppp;
				Ipv4Header h;
				p->RemoveHeader(ppp);
				p->RemoveHeader(h);
				h.SetEcn((Ipv4Header::EcnType)0x03);
				p->AddHeader(h);
				p->AddHeader(ppp);
			}
		}
		//CheckAndSendPfc(inDev, qIndex);
		CheckAndSendResume(inDev, qIndex);
	}
	if (1){
		uint8_t* buf = p->GetBuffer();
		if (buf[PppHeader::GetStaticSize() + 9] == 0x11){ // udp packet
			IntHeader *ih = (IntHeader*)&buf[PppHeader::GetStaticSize() + 20 + 8 + 6]; // ppp, ip, udp, SeqTs, INT
			Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
			if (m_ccMode == 3){ // HPCC
				ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
			}else if (m_ccMode == 10){ // HPCC-PINT
				uint64_t t = Simulator::Now().GetTimeStep();
				uint64_t dt = t - m_lastPktTs[ifIndex];
				if (dt > m_maxRtt)
					dt = m_maxRtt;
				uint64_t B = dev->GetDataRate().GetBitRate() / 8; //Bps
				uint64_t qlen = dev->GetQueue()->GetNBytesTotal();
				double newU;

				/**************************
				 * approximate calc
				 *************************/
				int b = 20, m = 16, l = 20; // see log2apprx's paremeters
				int sft = logres_shift(b,l);
				double fct = 1<<sft; // (multiplication factor corresponding to sft)
				double log_T = log2(m_maxRtt)*fct; // log2(T)*fct
				double log_B = log2(B)*fct; // log2(B)*fct
				double log_1e9 = log2(1e9)*fct; // log2(1e9)*fct
				double qterm = 0;
				double byteTerm = 0;
				double uTerm = 0;
				if ((qlen >> 8) > 0){
					int log_dt = log2apprx(dt, b, m, l); // ~log2(dt)*fct
					int log_qlen = log2apprx(qlen >> 8, b, m, l); // ~log2(qlen / 256)*fct
					qterm = pow(2, (
								log_dt + log_qlen + log_1e9 - log_B - 2*log_T
								)/fct
							) * 256;
					// 2^((log2(dt)*fct+log2(qlen/256)*fct+log2(1e9)*fct-log2(B)*fct-2*log2(T)*fct)/fct)*256 ~= dt*qlen*1e9/(B*T^2)
				}
				if (m_lastPktSize[ifIndex] > 0){
					int byte = m_lastPktSize[ifIndex];
					int log_byte = log2apprx(byte, b, m, l);
					byteTerm = pow(2, (
								log_byte + log_1e9 - log_B - log_T
								)/fct
							);
					// 2^((log2(byte)*fct+log2(1e9)*fct-log2(B)*fct-log2(T)*fct)/fct) ~= byte*1e9 / (B*T)
				}
				if (m_maxRtt > dt && m_u[ifIndex] > 0){
					int log_T_dt = log2apprx(m_maxRtt - dt, b, m, l); // ~log2(T-dt)*fct
					int log_u = log2apprx(int(round(m_u[ifIndex] * 8192)), b, m, l); // ~log2(u*512)*fct
					uTerm = pow(2, (
								log_T_dt + log_u - log_T
								)/fct
							) / 8192;
					// 2^((log2(T-dt)*fct+log2(u*512)*fct-log2(T)*fct)/fct)/512 = (T-dt)*u/T
				}
				newU = qterm+byteTerm+uTerm;

				#if 0
				/**************************
				 * accurate calc
				 *************************/
				double weight_ewma = double(dt) / m_maxRtt;
				double u;
				if (m_lastPktSize[ifIndex] == 0)
					u = 0;
				else{
					double txRate = m_lastPktSize[ifIndex] / double(dt); // B/ns
					u = (qlen / m_maxRtt + txRate) * 1e9 / B;
				}
				newU = m_u[ifIndex] * (1 - weight_ewma) + u * weight_ewma;
				printf(" %lf\n", newU);
				#endif

				/************************
				 * update PINT header
				 ***********************/
				uint16_t power = Pint::encode_u(newU);
				if (power > ih->GetPower())
					ih->SetPower(power);

				m_u[ifIndex] = newU;
			}
		}
	}
	m_txBytes[ifIndex] += p->GetSize();
	m_lastPktSize[ifIndex] = p->GetSize();
	m_lastPktTs[ifIndex] = Simulator::Now().GetTimeStep();
}

int SwitchNode::logres_shift(int b, int l){
	static int data[] = {0,0,1,2,2,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5};
	return l - data[b];
}

int SwitchNode::log2apprx(int x, int b, int m, int l){
	int x0 = x;
	int msb = int(log2(x)) + 1;
	if (msb > m){
		x = (x >> (msb - m) << (msb - m));
		#if 0
		x += + (1 << (msb - m - 1));
		#else
		int mask = (1 << (msb-m)) - 1;
		if ((x0 & mask) > (rand() & mask))
			x += 1<<(msb-m);
		#endif
	}
	return int(log2(x) * (1<<logres_shift(b, l)));
}

// for monitor
/**
 * outoput format:
 * time, sw_id, port_id, q_id, qlen, port_len
*/
void SwitchNode::PrintSwitchQlen(FILE* qlen_output){
	uint32_t n_dev = this->GetNDevices();
	for(uint32_t i = 1; i < n_dev; ++i){
		uint64_t port_len = 0;
		for(uint32_t j=0; j < qCnt; ++j){
			port_len += m_mmu->egress_bytes[i][j];
		}
		if(port_len == last_port_qlen[i]){
			continue;
		}
		for(uint32_t j=0; j < qCnt; ++j){
			fprintf(qlen_output, "%lu, %u, %u, %u, %u, %lu\n", Simulator::Now().GetTimeStep(), m_id, i, j, m_mmu->egress_bytes[i][j], port_len);
			fflush(qlen_output);
		}
		last_port_qlen[i] = port_len;
	}		
}

/**
 * outoput format:
 * time, sw_id, port_id, bandwidth
*/
void SwitchNode::PrintSwitchBw(FILE* bw_output, uint32_t bw_mon_interval){
	uint32_t n_dev = this->GetNDevices();
	for(uint32_t i = 1; i < n_dev; ++i){
		if(last_txBytes[i] == m_txBytes[i]){
			continue;
		}
		double bw = (m_txBytes[i] - last_txBytes[i]) * 8 * 1e6 / bw_mon_interval; // bit/s
		bw = bw*1.0 / 1e9; // Gbps
		fprintf(bw_output, "%lu, %u, %u, %f\n", Simulator::Now().GetTimeStep(), m_id, i, bw);
		fflush(bw_output);
		last_txBytes[i] = m_txBytes[i];
	}	
}

} /* namespace ns3 */
