#include "priority-tag.h"

namespace ns3
{
  TypeId
  PriorityTag::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::PriorityTag")
                            .SetParent<Tag>()
                            .AddConstructor<PriorityTag>()
                            .AddAttribute("priority",
                                          "Priority Value",
                                          EmptyAttributeValue(),
                                          MakeUintegerAccessor(&PriorityTag::GetPriority),
                                          MakeUintegerChecker<uint8_t>());
    return tid;
  }

  TypeId
  PriorityTag::GetInstanceTypeId(void) const
  {
    return GetTypeId();
  }

  uint32_t
  PriorityTag::GetSerializedSize(void) const
  {
    return 1;
  }

  void
  PriorityTag::Serialize(TagBuffer i) const
  {
    i.WriteU8(m_priority);
  }

  void
  PriorityTag::Deserialize(TagBuffer i)
  {
    m_priority = i.ReadU8();
  }

  void
  PriorityTag::Print(std::ostream &os) const
  {
    os << "v=" << (uint32_t)m_priority;
  }

  void
  PriorityTag::SetPriority(uint8_t value)
  {
    m_priority = value;
  }

  uint8_t
  PriorityTag::GetPriority(void) const
  {
    return m_priority;
  }
}