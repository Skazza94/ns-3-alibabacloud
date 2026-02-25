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
                                .AddAttribute("SpongeMode",
                                              "0=FlushOnSilence, 1=ThrottleOnLoad, 2=PauseWhileBusy",
                                              UintegerValue(SpongeMode::FLUSH_ON_SILENCE),
                                              MakeUintegerAccessor(&SpongeNode::m_mode),
                                              MakeUintegerChecker<uint8_t>())
                                .AddAttribute("BufferSize",
                                              "The size of the buffer.",
                                              StringValue("4096MB"),
                                              MakeStringAccessor(&SpongeNode::m_bufSize),
                                              MakeStringChecker())
                                .AddAttribute("BounceDelay",
                                              "For FlushOnSilence, time to wait before starting bounce packets.",
                                              TimeValue(Time("100us")),
                                              MakeTimeAccessor(&SpongeNode::m_delay),
                                              MakeTimeChecker())
                                .AddAttribute("ThrottleStep",
                                              "For ThrottleOnLoad, throttle increase step.",
                                              TimeValue(Time("2us")),
                                              MakeTimeAccessor(&SpongeNode::m_step),
                                              MakeTimeChecker())
                                .AddAttribute("MaxThrottleStep",
                                              "For ThrottleOnLoad, max throttle increase.",
                                              TimeValue(Time("200us")),
                                              MakeTimeAccessor(&SpongeNode::m_maxStep),
                                              MakeTimeChecker())
                                .AddAttribute("PacketsPerBurst",
                                              "For ThrottleOnLoad, packets to bounce for each burst.",
                                              UintegerValue(64),
                                              MakeUintegerAccessor(&SpongeNode::m_burstSize),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("PauseGap",
                                              "For PauseWhileBusy, gap to wait before starting bounce packets.",
                                              TimeValue(Time("2us")),
                                              MakeTimeAccessor(&SpongeNode::m_minGap),
                                              MakeTimeChecker())
                                .AddAttribute("PauseForceAfter",
                                              "For PauseWhileBusy, max time to wait without gaps before bouncing packets.",
                                              TimeValue(Time("500us")),
                                              MakeTimeAccessor(&SpongeNode::m_forceAfter),
                                              MakeTimeChecker())
                                .AddTraceSource("SpongeRx",
                                                "Packet received by sponge.",
                                                MakeTraceSourceAccessor(&SpongeNode::m_spongeRx),
                                                "ns3::TracedCallback")
                                .AddTraceSource("SpongeDrop",
                                                "Packet dropped by sponge due to buffer full.",
                                                MakeTraceSourceAccessor(&SpongeNode::m_spongeDrop),
                                                "ns3::TracedCallback")
                                .AddTraceSource("SpongeTx",
                                                "Packet transmitted by sponge.",
                                                MakeTraceSourceAccessor(&SpongeNode::m_spongeTx),
                                                "ns3::TracedCallback");
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
        /* Use this without a QP, but to have a callback to invoke for tx :) */
        m_dev->m_rdmaEQ->m_rdmaGetNxtPkt = MakeCallback(&SpongeNode::GetNxtPkt, this);
    }

    int SpongeNode::Receive(Ptr<Packet> p, CustomHeader &ch)
    {
        m_rxPkts++;
        m_rxBytes += p->GetSize();
        m_spongeRx(p, this);

        bool success = m_queue->Enqueue(p);
        if (!success)
        {
            m_dropPkts++;
            m_dropBytes += p->GetSize();
            m_spongeDrop(p, this);
            return 1;
        }

        m_qPkts += 1;
        m_qBytes += p->GetSize();

        switch (m_mode)
        {
        case FLUSH_ON_SILENCE:
            if (m_armEvent.IsRunning())
            {
                m_armEvent.Cancel();
            }
            m_armEvent = Simulator::Schedule(m_delay, &SpongeNode::StartDrain, this);
            break;
        case THROTTLE_ON_LOAD:
            if (m_txGateOpen)
            {
                /* Start immediately if no TX is happening */
                m_txGateOpen = false;
                m_rxSinceLastTx = false;
                m_dev->TriggerTransmit();
            }
            else
            {
                /* Flag that we received packets after starting TX */
                m_rxSinceLastTx = true;
            }
            break;
        case PAUSE_WHILE_BUSY:
            m_lastRxTime = Simulator::Now();
            if (m_queue->GetNPackets() == 1)
            {
                m_firstEnqTime = Simulator::Now();
            }

            if (m_armEvent.IsRunning())
            {
                m_armEvent.Cancel();
            }
            m_armEvent = Simulator::Schedule(m_minGap, &SpongeNode::TryStartDrain, this);
            break;
        }

        return 0;
    }

    /* For FLUSH_ON_SILENCE, trigger transmission after the timer expired */
    void SpongeNode::StartDrain()
    {
        if (m_queue->IsEmpty())
            return;

        m_dev->TriggerTransmit();
    }

    /* For THROTTLE_ON_LOAD, evaluate the throttling time */
    void SpongeNode::EvaluateThrottle()
    {
        if (m_rxSinceLastTx)
        {
            /* Received packets in the meantime, increase throttle */
            m_throttle = std::min(m_throttle + m_step, m_maxStep);
        }
        else
        {
            /* No packet received in the meantime, decrease throttle */
            m_throttle = (m_throttle > m_step) ? (m_throttle - m_step) : MicroSeconds(0);
        }

        m_rxSinceLastTx = false;

        /* We try to transmit after the throttle time */
        if (m_armEvent.IsRunning())
        {
            m_armEvent.Cancel();
        }
        m_armEvent = Simulator::Schedule(m_throttle, &SpongeNode::TryDrainAfterThrottle, this);
    }

    /* For THROTTLE_ON_LOAD, transmit after throttle time */
    void SpongeNode::TryDrainAfterThrottle()
    {
        m_txGateOpen = true;

        if (!m_queue->IsEmpty())
        {
            m_txGateOpen = false;
            m_rxSinceLastTx = false;
            m_dev->TriggerTransmit();
        }
    }

    /* For PAUSE_WHILE_BUSY, start draining if timer allows */
    void SpongeNode::TryStartDrain()
    {
        if (m_queue->IsEmpty())
            return;

        Time now = Simulator::Now();
        /* Force flush if queue has been non-empty too long */
        if (now - m_firstEnqTime >= m_forceAfter)
        {
            m_dev->TriggerTransmit();
            return;
        }

        if (now - m_lastRxTime >= m_minGap)
        {
            /* Gap time has passed (no packet RX in the meantime), we can transmit */
            m_dev->TriggerTransmit();
        }
        else
        {
            /* Still busy, check again after remaining gap */
            Time left = m_minGap - (now - m_lastRxTime);
            m_armEvent = Simulator::Schedule(left, &SpongeNode::TryStartDrain, this);
        }
    }

    Ptr<Packet> SpongeNode::GetNxtPkt(Ptr<RdmaQueuePair> qp)
    {
        if (m_mode == FLUSH_ON_SILENCE && m_armEvent.IsRunning())
        {
            /* Not silent anymore, stop */
            return nullptr;
        }

        if (m_mode == THROTTLE_ON_LOAD && m_sentInBurst >= m_burstSize)
        {
            /* Stop burst */
            m_sentInBurst = 0;
            EvaluateThrottle();
            return nullptr;
        }

        Ptr<Packet> p = m_queue->Dequeue();
        if (p == 0)
        {
            if (m_mode == THROTTLE_ON_LOAD)
            {
                m_sentInBurst = 0;
                EvaluateThrottle();
            }

            return nullptr;
        }

        m_sentInBurst++;

        m_qPkts -= 1;
        m_qBytes -= p->GetSize();

        /* Revert original packet info before sending */
        CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
        p->RemoveHeader(ch);

        ch.sip = ch.udp.sh.m_osip;
        ch.dip = ch.udp.sh.m_odip;
        ch.udp.sport = ch.udp.sh.m_osport;
        ch.udp.dport = ch.udp.sh.m_odport;
        ch.udp.pg = ch.udp.sh.m_opg;

        ch.udp.sh.m_enabled = 0;
        ch.udp.sh.m_osip = 0;
        ch.udp.sh.m_odip = 0;
        ch.udp.sh.m_osport = 0;
        ch.udp.sh.m_odport = 0;
        ch.udp.sh.m_opg = 0;

        p->AddHeader(ch);

        m_txPkts++;
        m_txBytes += p->GetSize();
        m_spongeTx(p, this);

        return p;
    }

    void SpongeNode::PrintUtil(FILE *f, std::string ev)
    {
        fprintf(f, "%lu, %u, %s, %lu, %lu, %lu, %lu, %lu, %lu, %lu, %lu\n", Simulator::Now().GetTimeStep(), GetId(), ev.c_str(), m_qPkts, m_qBytes, m_rxPkts, m_rxBytes, m_dropPkts, m_dropBytes, m_txPkts, m_txBytes);
        fflush(f);
    }
}
