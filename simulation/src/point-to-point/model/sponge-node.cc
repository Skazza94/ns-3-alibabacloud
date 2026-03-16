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
                                .AddAttribute("PacketsPerBurst",
                                              "Packets to bounce for each burst (for ThrottleOnLoad and PauseWhileBusy).",
                                              UintegerValue(64),
                                              MakeUintegerAccessor(&SpongeNode::m_burstSize),
                                              MakeUintegerChecker<uint32_t>())
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
                                .AddAttribute("PauseMaxBackoffGap",
                                              "For PauseWhileBusy, max quiet-gap after exponential backoff.",
                                              TimeValue(Time("200us")),
                                              MakeTimeAccessor(&SpongeNode::m_maxBackoffGap),
                                              MakeTimeChecker())
                                .AddAttribute("PauseForceRounds",
                                              "For PauseWhileBusy, number of failed probe/wait rounds before force mode is allowed.",
                                              UintegerValue(3),
                                              MakeUintegerAccessor(&SpongeNode::m_forceRounds),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("PauseMaxBackoffShift",
                                              "For PauseWhileBusy, max number of exponential backoff doublings.",
                                              UintegerValue(6),
                                              MakeUintegerAccessor(&SpongeNode::m_maxBackoffShift),
                                              MakeUintegerChecker<uint32_t>())
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
            Time now = Simulator::Now();
            m_lastRxTime = now;

            /* Queue just became non-empty: start a new pause epoch */
            if (m_queue->GetNPackets() == 1)
            {
                m_forceMode = false;
                m_sentInBurst = 0;
                m_txGateOpen = true;
                m_pauseRound = 0;
                m_failedRounds = 1;
                m_goodRounds = 0;

                if (m_armEvent.IsRunning())
                {
                    m_armEvent.Cancel();
                }
                m_armEvent = Simulator::Schedule(GetBackoffGap(), &SpongeNode::TryStartDrain, this, true);
                if (m_forceEvent.IsRunning())
                {
                    m_forceEvent.Cancel();
                }
                m_forceEvent = Simulator::Schedule(m_forceAfter, &SpongeNode::ForceStartDrain, this);
            }
            else
            {
                if (m_txGateOpen)
                {
                    if (m_armEvent.IsRunning())
                    {
                        m_armEvent.Cancel();
                    }
                    m_armEvent = Simulator::Schedule(GetBackoffGap(), &SpongeNode::TryStartDrain, this, true);
                }
            }
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

    /* For PAUSE_WHILE_BUSY, compute what is the gap timer to use considering backoff */
    Time SpongeNode::GetBackoffGap() const
    {
        uint32_t shift = std::min(m_pauseRound, m_maxBackoffShift);
        Time gap = m_minGap;

        for (uint32_t i = 0; i < shift; ++i)
        {
            gap = gap + gap;
            if (gap >= m_maxBackoffGap)
            {
                return m_maxBackoffGap;
            }
        }

        return std::min(gap, m_maxBackoffGap);
    }

    /* For PAUSE_WHILE_BUSY, start draining if timer allows */
    void SpongeNode::TryStartDrain(bool makeProbe)
    {
        if (m_queue->IsEmpty())
            return;

        Time now = Simulator::Now();
        Time gap = GetBackoffGap();
        if (now - m_lastRxTime >= gap)
        {
            m_txGateOpen = false;
            m_sentInBurst = 0;
            m_probeBurst = makeProbe;

            if (m_forceEvent.IsRunning())
            {
                m_forceEvent.Cancel();
            }

            m_dev->TriggerTransmit();
        }
        else
        {
            if (m_armEvent.IsRunning())
            {
                m_armEvent.Cancel();
            }
            Time left = gap - (now - m_lastRxTime);
            m_armEvent = Simulator::Schedule(left, &SpongeNode::TryStartDrain, this, makeProbe);
        }
    }

    /* For PAUSE_WHILE_BUSY, start force draining */
    void SpongeNode::ForceStartDrain()
    {
        if (m_queue->IsEmpty())
            return;

        if (m_failedRounds < m_forceRounds)
        {
            /* Not enough failed waiting phases yet: keep waiting, but back off more */
            m_pauseRound++;
            m_failedRounds++;

            if (m_armEvent.IsRunning())
            {
                m_armEvent.Cancel();
            }
            m_armEvent = Simulator::Schedule(GetBackoffGap(), &SpongeNode::TryStartDrain, this, true);
            if (m_forceEvent.IsRunning())
            {
                m_forceEvent.Cancel();
            }
            m_forceEvent = Simulator::Schedule(m_forceAfter, &SpongeNode::ForceStartDrain, this);
            return;
        }

        m_forceMode = true;
        m_txGateOpen = false;
        m_sentInBurst = 0;
        m_probeBurst = true;

        if (m_armEvent.IsRunning())
        {
            m_armEvent.Cancel();
        }

        m_dev->TriggerTransmit();
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

        if (m_mode == PAUSE_WHILE_BUSY)
        {
            Time now = Simulator::Now();
            /* Before force timeout expires, pause again if arrivals are still recent */
            Time gap = GetBackoffGap();
            if (!m_forceMode && (now - m_lastRxTime < gap))
            {
                m_txGateOpen = true;
                m_sentInBurst = 0;
                m_goodRounds = 0;
                m_pauseRound++;
                m_failedRounds++;

                if (m_armEvent.IsRunning())
                {
                    m_armEvent.Cancel();
                }
                Time left = gap - (now - m_lastRxTime);
                m_armEvent = Simulator::Schedule(left, &SpongeNode::TryStartDrain, this, true);
                if (m_forceEvent.IsRunning())
                {
                    m_forceEvent.Cancel();
                }
                m_forceEvent = Simulator::Schedule(m_forceAfter, &SpongeNode::ForceStartDrain, this);
                return nullptr;
            }

            uint32_t burstLimit = m_probeBurst ? 1 : ((m_goodRounds < 2) ? (m_burstSize / 2) : m_burstSize);
            if (m_sentInBurst >= burstLimit)
            {
                bool wasProbeBurst = m_probeBurst;

                m_txGateOpen = true;
                m_sentInBurst = 0;

                if (m_armEvent.IsRunning())
                {
                    m_armEvent.Cancel();
                }
                if (m_forceEvent.IsRunning())
                {
                    m_forceEvent.Cancel();
                }

                if (m_forceMode)
                {
                    /* Force mode only buys one burst, then fall back to normal logic */
                    m_forceMode = false;
                }
                else if (!wasProbeBurst)
                {
                    /* A completed non-probe burst is partial success, so relax backoff a bit */
                    if (m_pauseRound > 0)
                    {
                        m_pauseRound--;
                    }
                }

                m_probeBurst = false;

                Time gap = GetBackoffGap();
                Time now = Simulator::Now();
                Time sinceLastRx = now - m_lastRxTime;

                if (wasProbeBurst)
                {
                    /* Probe got sent just now. Even if nothing has arrived yet, that does NOT mean the path is clear; the probe may not have had time to bounce back yet. So always wait a fresh full gap. */
                    m_armEvent = Simulator::Schedule(gap, &SpongeNode::TryStartDrain, this, false);
                    m_forceEvent = Simulator::Schedule(m_forceAfter, &SpongeNode::ForceStartDrain, this);

                    /* A good probe is only weak evidence. */
                    m_goodRounds = 1;
                }
                else
                {
                    m_goodRounds++;

                    /* For normal bursts, accelerate if the path has already been quiet long enough; otherwise wait only the remaining gap. */
                    if (m_goodRounds >= 2 && sinceLastRx >= gap)
                    {
                        /* Only accelerate after 2 consecutive good rounds. */
                        m_txGateOpen = false;
                        m_dev->TriggerTransmit();
                    }
                    else
                    {
                        Time left = (sinceLastRx >= gap) ? Time(0) : (gap - sinceLastRx);
                        m_armEvent = Simulator::Schedule(left, &SpongeNode::TryStartDrain, this, false);
                        m_forceEvent = Simulator::Schedule(m_forceAfter, &SpongeNode::ForceStartDrain, this);
                    }
                }

                return nullptr;
            }
        }

        Ptr<Packet> p = m_queue->Dequeue();
        if (p == 0)
        {
            if (m_mode == THROTTLE_ON_LOAD)
            {
                m_sentInBurst = 0;
                EvaluateThrottle();
            }
            else if (m_mode == PAUSE_WHILE_BUSY)
            {
                m_txGateOpen = true;
                m_forceMode = false;
                m_sentInBurst = 0;
                m_pauseRound = 0;
                m_failedRounds = 0;
                m_goodRounds = 0;
                m_probeBurst = false;

                if (m_armEvent.IsRunning())
                {
                    m_armEvent.Cancel();
                }
                if (m_forceEvent.IsRunning())
                {
                    m_forceEvent.Cancel();
                }
            }

            return nullptr;
        }

        m_sentInBurst++;

        m_qPkts -= 1;
        m_qBytes -= p->GetSize();

        /* Revert original packet info before sending */
        CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
        p->RemoveHeader(ch);

        /* Read and remove the SpongeTag */
        SpongeTag st;
        if (!p->RemovePacketTag(st))
        {
            /* Tag cannot be found, something weird happened, return. */
            std::cout << "Sponge cannot forward a packet without SpongeTag!" << std::endl;
            return nullptr;
        }

        ch.sip = st.GetOriginalSrcIp();
        ch.dip = st.GetOriginalDstIp();
        ch.udp.sport = st.GetOriginalSrcPort();
        ch.udp.dport = st.GetOriginalDstPort();
        ch.udp.pg = st.GetOriginalPG();

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

    void SpongeNode::PrintSpongeBW(FILE *bw_output, uint32_t bw_mon_interval)
    {
        if (m_txBytes == m_lastTxBytes)
        {
            return;
        }

        double bw = (m_txBytes - m_lastTxBytes) * 8 * 1e6 / (bw_mon_interval);
        bw = bw * 1.0 / 1e9;
        fprintf(bw_output, "%lu, %u, %u, %f\n", Simulator::Now().GetTimeStep(), GetId(), 1, bw);
        fflush(bw_output);
        m_lastTxBytes = m_txBytes;
    }
}
