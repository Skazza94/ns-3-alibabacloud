#include "deflection-tag.h"

namespace ns3
{
    DeflectionTag::DeflectionTag() : m_deflections(0) {}

    TypeId DeflectionTag::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::DeflectionTag")
                                .SetParent<Tag>()
                                .AddConstructor<DeflectionTag>();
        return tid;
    }

    TypeId DeflectionTag::GetInstanceTypeId() const
    {
        return GetTypeId();
    }

    uint32_t DeflectionTag::GetSerializedSize() const
    {
        return 2;
    }

    void DeflectionTag::Serialize(TagBuffer i) const
    {
        i.WriteU16(m_deflections);
    }

    void DeflectionTag::Deserialize(TagBuffer i)
    {
        m_deflections = i.ReadU16();
    }

    void DeflectionTag::Print(std::ostream &os) const
    {
        os << "deflections=" << m_deflections;
    }

    void DeflectionTag::Increment()
    {
        m_deflections++;
    }

    uint16_t DeflectionTag::GetDeflections() const
    {
        return m_deflections;
    }
}
