#include "swift-hop-tag.h"

namespace ns3
{
    SwiftHopTag::SwiftHopTag() : m_hops(0) {}

    TypeId SwiftHopTag::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::SwiftHopTag")
                                .SetParent<Tag>()
                                .AddConstructor<SwiftHopTag>();
        return tid;
    }

    TypeId SwiftHopTag::GetInstanceTypeId() const
    {
        return GetTypeId();
    }

    uint32_t SwiftHopTag::GetSerializedSize() const
    {
        return 4;
    }

    void SwiftHopTag::Serialize(TagBuffer i) const
    {
        i.WriteU32(m_hops);
    }

    void SwiftHopTag::Deserialize(TagBuffer i)
    {
        m_hops = i.ReadU32();
    }

    void SwiftHopTag::Print(std::ostream &os) const
    {
        os << "hops=" << m_hops;
    }

    void SwiftHopTag::SetHops(uint32_t hops)
    {
        m_hops = hops;
    }

    void SwiftHopTag::IncHops()
    {
        m_hops++;
    }

    uint32_t SwiftHopTag::GetHops() const
    {
        return m_hops;
    }
}
