#include "themis-loop-tag.h"

namespace ns3
{
    ThemisLoopTag::ThemisLoopTag() : m_left(-1) {}

    ThemisLoopTag::ThemisLoopTag(int32_t left) : m_left(left) {}

    TypeId ThemisLoopTag::GetTypeId()
    {
        static TypeId tid = TypeId("ns3::ThemisLoopTag")
                                .SetParent<Tag>()
                                .AddConstructor<ThemisLoopTag>();
        return tid;
    }

    TypeId ThemisLoopTag::GetInstanceTypeId() const
    {
        return GetTypeId();
    }

    uint32_t ThemisLoopTag::GetSerializedSize() const
    {
        return 4;
    }

    void ThemisLoopTag::Serialize(TagBuffer i) const
    {
        i.WriteU32((uint32_t)m_left);
    }

    void ThemisLoopTag::Deserialize(TagBuffer i)
    {
        m_left = (int32_t)i.ReadU32();
    }

    void ThemisLoopTag::Print(std::ostream &os) const
    {
        os << "left=" << m_left;
    }

    void ThemisLoopTag::SetLeft(int32_t left)
    {
        m_left = left;
    }

    int32_t ThemisLoopTag::GetLeft() const
    {
        return m_left;
    }
}
