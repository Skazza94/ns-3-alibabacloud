#ifndef SPONGE_NODE_H
#define SPONGE_NODE_H

#include "ns3/node.h"
#include "ns3/callback.h"
#include "ns3/qbb-net-device.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"

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
        uint64_t m_qPkts = 0;
        uint64_t m_qBytes = 0;

        uint64_t m_rxPkts = 0;
        uint64_t m_rxBytes = 0;
        uint64_t m_dropPkts = 0;
        uint64_t m_dropBytes = 0;
        uint64_t m_txPkts = 0;
        uint64_t m_txBytes = 0;

        int Receive(Ptr<Packet> p, CustomHeader &ch);
        void Bounce();
        Ptr<Packet> GetNxtPkt(Ptr<RdmaQueuePair> qp);

        Time m_delay{MicroSeconds(100)};
        Ptr<QbbNetDevice> m_dev;

        std::string m_bufSize;
        Ptr<SimpleDropTailQueue> m_queue;

        EventId m_bounceTimer;

        TracedCallback<Ptr<const Packet>, Ptr<SpongeNode>> m_spongeRx;
        TracedCallback<Ptr<const Packet>, Ptr<SpongeNode>> m_spongeDrop;
        TracedCallback<Ptr<const Packet>, Ptr<SpongeNode>> m_spongeTx;
    };

}
#endif