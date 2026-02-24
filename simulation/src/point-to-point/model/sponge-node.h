#ifndef SPONGE_NODE_H
#define SPONGE_NODE_H

#include "ns3/node.h"
#include "ns3/callback.h"
#include "ns3/qbb-net-device.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"

enum SpongeMode : uint8_t
{
    FLUSH_ON_SILENCE = 0, // Reset-on-arrival delay
    THROTTLE_ON_LOAD = 1, // Start TX immediately, then slow down when arrivals are high
    PAUSE_WHILE_BUSY = 2  // Stop sending while arrivals are coming, send only in gaps (adaptive)
};

namespace ns3
{
    class SpongeNode : public Node
    {
    public:
        static TypeId GetTypeId();
        SpongeNode();

        void SetPort(Ptr<QbbNetDevice> dev);
        void PrintUtil(FILE *f, std::string ev);

    protected:
        void NotifyConstructionCompleted() override;

    private:
        /* Configuration knobs */
        uint8_t m_mode = FLUSH_ON_SILENCE;
        std::string m_bufSize;

        /* For FLUSH_ON_SILENCE */
        Time m_delay{MicroSeconds(100)};
        /* For THROTTLE_ON_LOAD */
        Time m_step = MicroSeconds(2);
        Time m_maxStep = MicroSeconds(200);
        uint32_t m_burstSize = 64;
        /* For PAUSE_WHILE_BUSY */
        Time m_minGap = MicroSeconds(2);
        Time m_forceAfter = MicroSeconds(500);

        /* Statistics */
        uint64_t m_qPkts = 0;
        uint64_t m_qBytes = 0;
        uint64_t m_rxPkts = 0;
        uint64_t m_rxBytes = 0;
        uint64_t m_dropPkts = 0;
        uint64_t m_dropBytes = 0;
        uint64_t m_txPkts = 0;
        uint64_t m_txBytes = 0;

        /* Runtime variables */
        Ptr<QbbNetDevice> m_dev;
        Ptr<SimpleDropTailQueue> m_queue;
        EventId m_armEvent;
        /* For THROTTLE_ON_LOAD */
        bool m_rxSinceLastTx = false;
        Time m_throttle = MicroSeconds(0);
        bool m_txGateOpen = true;
        uint32_t m_sentInBurst = 0;
        /* For PAUSE_WHILE_BUSY */
        Time m_lastRxTime = Seconds(0);
        Time m_firstEnqTime = Seconds(0);

        /* Trace callbacks */
        TracedCallback<Ptr<const Packet>, Ptr<SpongeNode>> m_spongeRx;
        TracedCallback<Ptr<const Packet>, Ptr<SpongeNode>> m_spongeDrop;
        TracedCallback<Ptr<const Packet>, Ptr<SpongeNode>> m_spongeTx;

        int Receive(Ptr<Packet> p, CustomHeader &ch);

        /* For FLUSH_ON_SILENCE, trigger transmission after the timer expired */
        void StartDrain();
        /* For THROTTLE_ON_LOAD, evaluate the throttling time */
        void EvaluateThrottle();
        /* For THROTTLE_ON_LOAD, transmit after throttle time */
        void TryDrainAfterThrottle();
        /* For PAUSE_WHILE_BUSY, start draining if timer allows */
        void TryStartDrain();

        Ptr<Packet> GetNxtPkt(Ptr<RdmaQueuePair> qp);
    };

}
#endif