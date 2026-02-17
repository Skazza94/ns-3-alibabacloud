#include "sponge-node.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/simulator.h"
#include "ns3/point-to-point-net-device.h"

namespace ns3
{
    TypeId
    SpongeNode::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::SpongeNode")
                                .SetParent<Node>()
                                .AddConstructor<SpongeNode>()
                                .AddAttribute("BufferSize",
                                              "The size of the buffer.",
                                              StringValue("4096MB"),
                                              MakeStringAccessor(&SpongeNode::m_bufSize),
                                              MakeStringChecker());
        return tid;
    }

    SpongeNode::SpongeNode()
    {
        m_node_type = 3;
    }

    void SpongeNode::NotifyConstructionCompleted()
    {
        Node::NotifyConstructionCompleted();

        m_queue = CreateObject<SimpleDropTailQueue>();
        m_queue->SetAttribute("MaxBytes", UintegerValue(QueueSize(m_bufSize).GetValue()));
    }

    void SpongeNode::SetPort(Ptr<QbbNetDevice> dev)
    {
        m_dev = dev;

        m_dev->SetNode(this);

        m_dev->m_rdmaReceiveCb = MakeCallback(&SpongeNode::Receive, this);
        /* Use this without a QP, but to have a callback to invoke :) */
        m_dev->m_rdmaEQ->m_rdmaGetNxtPkt = MakeCallback(&SpongeNode::GetNxtPkt, this);
    }

    int SpongeNode::Receive(Ptr<Packet> p, CustomHeader &ch)
    {
        bool success = m_queue->Enqueue(p);

        if (success)
        {
            if (m_bounceTimer.IsRunning())
            {
                m_bounceTimer.Cancel();
            }

            m_bounceTimer = Simulator::Schedule(m_delay, &SpongeNode::Bounce, this);
        }
    }

    void SpongeNode::Bounce()
    {
        m_dev->TriggerTransmit();
    }

    Ptr<Packet> SpongeNode::GetNxtPkt(Ptr<RdmaQueuePair> qp)
    {
        if (m_bounceTimer.IsRunning())
        {
            return nullptr;
        }

        Ptr<Packet> p = m_queue->Dequeue();
        if (p == 0)
        {
            return nullptr;
        }

        /* Revert original packet info before sending */
        CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
        p->RemoveHeader(ch);

        ch.sip = ch.udp.sh.m_osip;
        ch.dip = ch.udp.sh.m_odip;

        ch.udp.sport = ch.udp.sh.m_osport;
        ch.udp.dport = ch.udp.sh.m_odport;

        ch.udp.sh.m_osip = 0;
        ch.udp.sh.m_odip = 0;
        ch.udp.sh.m_osport = 0;
        ch.udp.sh.m_odport = 0;

        p->AddHeader(ch);

        CustomHeader tch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
        p->PeekHeader(tch);
        std::cout << Simulator::Now().GetTimeStep() << " [SPONGE] [" << Ipv4Address(tch.sip) << "(" << tch.udp.sport << ") --> " << Ipv4Address(tch.dip) << "(" << tch.udp.dport << ")] [Seq " << tch.udp.seq << "]" << std::endl;

        return p;
    }
}
