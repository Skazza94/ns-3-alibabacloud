#include "retransmission-tag.h"

namespace ns3
{
    RetransmissionTag::RetransmissionTag() : m_rtx(0) {}

    TypeId RetransmissionTag::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::RetransmissionTag")
                                .SetParent<Tag>()
                                .AddConstructor<RetransmissionTag>();
        return tid;
    }

    TypeId RetransmissionTag::GetInstanceTypeId() const
    {
        return GetTypeId();
    }

    uint32_t RetransmissionTag::GetSerializedSize() const
    {
        return 4;
    }

    void RetransmissionTag::Serialize(TagBuffer i) const
    {
        i.WriteU32(m_rtx);
    }

    void RetransmissionTag::Deserialize(TagBuffer i)
    {
        m_rtx = i.ReadU32();
    }

    void RetransmissionTag::Print(std::ostream &os) const
    {
        os << "rtx=" << m_rtx;
    }

    void RetransmissionTag::SetRetransmissions(uint32_t n)
    {
        m_rtx = n;
    }

    uint32_t RetransmissionTag::GetRetransmissions() const
    {
        return m_rtx;
    }
}
