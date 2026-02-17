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
        void SetBounceDelay(Time d) { m_delay = d; }

    protected:
        void NotifyConstructionCompleted() override;

    private:
        int Receive(Ptr<Packet> p, CustomHeader &ch);
        void Bounce();
        Ptr<Packet> GetNxtPkt(Ptr<RdmaQueuePair> qp);

        Time m_delay{MicroSeconds(100)};
        Ptr<QbbNetDevice> m_dev;
        
        std::string m_bufSize;
        Ptr<SimpleDropTailQueue> m_queue;

        EventId m_bounceTimer;
    };

}
#endif