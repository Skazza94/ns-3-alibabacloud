#ifndef PRIORITY_TAG_H
#define PRIORITY_TAG_H

#include "ns3/tag.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include <iostream>

namespace ns3
{
  class PriorityTag : public Tag
  {
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    virtual TypeId GetInstanceTypeId(void) const;
    virtual uint32_t GetSerializedSize(void) const;
    virtual void Serialize(TagBuffer i) const;
    virtual void Deserialize(TagBuffer i);
    virtual void Print(std::ostream &os) const;

    /**
     * Set the tag value
     * \param value The tag value.
     */
    void SetPriority(uint8_t value);

    /**
     * Get the tag value
     * \return the tag value.
     */
    uint8_t GetPriority(void) const;

  private:
    uint8_t m_priority; //!< tag value
  };

}

#endif /* PRIORITY_TAG_H */
