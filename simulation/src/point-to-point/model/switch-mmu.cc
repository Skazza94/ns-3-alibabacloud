#include <iostream>
#include <fstream>
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/object-vector.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/global-value.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/random-variable.h"
#include "ns3/pointer.h"

#include "switch-mmu.h"

NS_LOG_COMPONENT_DEFINE("SwitchMmu");
namespace ns3
{
    TypeId SwitchMmu::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::SwitchMmu")
                                .SetParent<Object>()
                                .AddConstructor<SwitchMmu>()
                                .AddTraceSource("IngressDropTrace",
                                                "Tracks ingress drops",
                                                MakeTraceSourceAccessor(&SwitchMmu::m_igDropTrace),
                                                "ns3::TracedCallback")
                                .AddTraceSource("EgressDropTrace",
                                                "Tracks egress drops",
                                                MakeTraceSourceAccessor(&SwitchMmu::m_egDropTrace),
                                                "ns3::TracedCallback");
        return tid;
    }

    SwitchMmu::SwitchMmu(void)
    {
        // Buffer pools
        bufferPool = 24 * 1024 * 1024;           // ASIC buffer size i.e, total shared buffer
        ingressPool = 18 * 1024 * 1024;          // Size of ingress pool. Note: This is shared by both lossless and lossy traffic.
        egressPool[LOSSLESS] = 24 * 1024 * 1024; // Size of egress lossless pool. Lossless bypasses egress admission
        egressPool[LOSSY] = 14 * 1024 * 1024;    // Size of egress lossy pool.
        sharedPool = 18 * 1024 * 1024;           // For Reverie which maintains a single shared buffer pool, all lossless and lossy share this pool
        xoffTotal = 0;                           // 6 * 1024 * 1024; // Total headroom space in the shared buffer pool.
        // xoffTotal value is incremented when SetHeadroom function is used. So setting it to zero initially.
        // Note: This would mean that headroom must be set explicitly.
        totalIngressReserved = 0;
        totalIngressReservedUsed = 0;

        totalUsed = 0;
        egressPoolUsed[LOSSLESS] = 0; // Total bytes USED in the egress lossless pool
        egressPoolUsed[LOSSY] = 0;    // Total bytes USED in the egress lossy pool
        xoffTotalUsed = 0;            // Total headroom bytes USED so far. Updated at runtime.
        sharedPoolUsed = 0;           // For Reverie: total shared pool used buffer.

        for (uint32_t port = 0; port < pCnt; port++)
        {
            for (uint32_t q = 0; q < qCnt; q++)
            {
                reserveIngress[port][q] = 0;
                reserveEgress[port][q] = 0;
                alphaEgress[port][q] = 1;
                alphaIngress[port][q] = 1;
                xoff[port][q] = 0;
                xon[port][q] = 1248;
                xon_offset[port][q] = 2496;

                ingress_bytes[port][q] = 0;
                paused[port][q] = 0;
                egress_bytes[port][q] = 0;
                xoffUsed[port][q] = 0;
            }
        }

        memset(ingress_bytes, 0, sizeof(ingress_bytes));
        memset(paused, 0, sizeof(paused));
        memset(egress_bytes, 0, sizeof(egress_bytes));

        portCount = pCnt; // default value is 257. This should be set to the real port count using SetPortCount function externally based on the simulation setup
    }

    void
    SwitchMmu::SetBufferPool(uint64_t b)
    {
        bufferPool = b;
    }

    void
    SwitchMmu::SetIngressPool(uint64_t b)
    {
        ingressPool = b;
    }

    void
    SwitchMmu::SetSharedPool(uint64_t b)
    {
        sharedPool = b;
    }

    void
    SwitchMmu::SetEgressLossyPool(uint64_t b)
    {
        egressPool[LOSSY] = b;
    }

    void
    SwitchMmu::SetEgressLosslessPool(uint64_t b)
    {
        egressPool[LOSSLESS] = b;
    }

    void
    SwitchMmu::SetReserved(uint64_t b, uint32_t port, uint32_t q, std::string inout)
    {
        if (inout == "ingress")
        {
            if (totalIngressReserved >= reserveIngress[port][q])
                totalIngressReserved -= reserveIngress[port][q];
            else
                totalIngressReserved = 0;

            reserveIngress[port][q] = b;
            totalIngressReserved += reserveIngress[port][q];
        }
        else if (inout == "egress")
        {
            std::cout << "setting reserved for egress is not supported. Exiting!" << std::endl;
            exit(1);
        }
    }

    void
    SwitchMmu::SetReserved(uint64_t b, std::string inout)
    {
        if (inout == "ingress")
        {
            for (uint32_t port = 0; port < pCnt; port++)
            {
                for (uint32_t q = 0; q < qCnt; q++)
                {
                    if (totalIngressReserved >= reserveIngress[port][q])
                        totalIngressReserved -= reserveIngress[port][q];
                    else
                        totalIngressReserved = 0;
                    reserveIngress[port][q] = b;
                    totalIngressReserved += reserveIngress[port][q];
                }
            }
        }
        else if (inout == "egress")
        {
            std::cout << "setting reserved for egress is not supported. Exiting!" << std::endl;
            exit(1);
        }
    }

    void
    SwitchMmu::SetAlphaIngress(double value, uint32_t port, uint32_t q)
    {
        alphaIngress[port][q] = value;
    }

    void
    SwitchMmu::SetAlphaIngress(double value)
    {
        for (uint32_t port = 0; port < pCnt; port++)
        {
            for (uint32_t q = 0; q < qCnt; q++)
            {
                alphaIngress[port][q] = value;
            }
        }
    }

    void
    SwitchMmu::SetAlphaEgress(double value, uint32_t port, uint32_t q)
    {
        alphaEgress[port][q] = value;
    }

    void
    SwitchMmu::SetAlphaEgress(double value)
    {
        for (uint32_t port = 0; port < pCnt; port++)
        {
            for (uint32_t q = 0; q < qCnt; q++)
            {
                alphaEgress[port][q] = value;
            }
        }
    }

    void
    SwitchMmu::SetHeadroom(uint64_t b, uint32_t port, uint32_t q)
    {
        xoffTotal -= xoff[port][q];
        xoff[port][q] = b;
        xoffTotal += xoff[port][q];
    }

    void SwitchMmu::SetHeadroom(uint64_t b, uint32_t port)
    {
        for (uint32_t q = 0; q < qCnt; q++)
        {
            xoffTotal -= xoff[port][q];
            xoff[port][q] = b;
            xoffTotal += xoff[port][q];
        }
    }

    void
    SwitchMmu::SetHeadroom(uint64_t b)
    {
        for (uint32_t port = 0; port < pCnt; port++)
        {
            for (uint32_t q = 0; q < qCnt; q++)
            {
                xoffTotal -= xoff[port][q];
                xoff[port][q] = b;
                xoffTotal += xoff[port][q];
            }
        }
    }

    void
    SwitchMmu::SetXon(uint64_t b, uint32_t port, uint32_t q)
    {
        xon[port][q] = b;
    }

    void
    SwitchMmu::SetXon(uint64_t b)
    {
        for (uint32_t port = 0; port < pCnt; port++)
        {
            for (uint32_t q = 0; q < qCnt; q++)
            {
                xon[port][q] = b;
            }
        }
    }

    void
    SwitchMmu::SetXonOffset(uint64_t b, uint32_t port, uint32_t q)
    {
        xon_offset[port][q] = b;
    }

    void
    SwitchMmu::SetXonOffset(uint64_t b)
    {
        for (uint32_t port = 0; port < pCnt; port++)
        {
            for (uint32_t q = 0; q < qCnt; q++)
            {
                xon_offset[port][q] = b;
            }
        }
    }

    uint64_t SwitchMmu::GetIngressReservedUsed()
    {
        return totalIngressReservedUsed;
    }

    uint64_t SwitchMmu::GetIngressReservedUsed(uint32_t port, uint32_t qIndex)
    {
        if (ingress_bytes[port][qIndex] > reserveIngress[port][qIndex])
        {
            return reserveIngress[port][qIndex];
        }
        else
        {
            return ingress_bytes[port][qIndex];
        }
    }

    uint64_t SwitchMmu::GetIngressSharedUsed()
    {
        return (totalUsed - xoffTotalUsed - totalIngressReservedUsed);
    }

    uint64_t SwitchMmu::DynamicThreshold(uint32_t port, uint32_t qIndex, std::string inout, uint32_t type)
    {
        if (inout == "ingress")
        {
            double remaining = 0;
            uint64_t ingressPoolSharedUsed = GetIngressSharedUsed();
            uint64_t ingressSharedPool = ingressPool - totalIngressReserved;
            if (ingressSharedPool > ingressPoolSharedUsed)
            {
                uint64_t remaining = ingressSharedPool - ingressPoolSharedUsed;
                return std::min(uint64_t(alphaIngress[port][qIndex] * (remaining)), UINT64_MAX - 1024 * 1024);
            }
            else
            {
                return 0;
            }
        }
        else if (inout == "egress")
        {
            double remaining = 0;
            if (egressPool[type] > egressPoolUsed[type])
            {
                uint64_t remaining = egressPool[type] - egressPoolUsed[type];
                uint64_t threshold = std::min(uint64_t(alphaEgress[port][qIndex] * (remaining)), UINT64_MAX - 1024 * 1024);
                return threshold;
            }
            else
            {
                return 0;
            }
        }
    }

    bool SwitchMmu::CheckIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type)
    {
        switch (type)
        {
        case LOSSY:
            // if ingress bytes is greater than the ingress threshold
            if ((psize + ingress_bytes[port][qIndex] > DynamicThreshold(port, qIndex, "ingress", type)
                 // AND if the reserved is usedup
                 && psize + ingress_bytes[port][qIndex] > reserveIngress[port][qIndex])
                // if the ingress pool is full. With DT, this condition is redundant.
                // This is just to account for any badly configured buffer or buffer sharing if any.
                || (psize + (totalUsed - xoffTotalUsed) > ingressPool)
                // or if the switch buffer is full
                || (psize + totalUsed > bufferPool))
            {
                TraceLossyIngressDrop(port, qIndex, ingress_bytes[port][qIndex], DynamicThreshold(port, qIndex, "ingress", type), psize);
                return false;
            }
            else
            {
                return true;
            }
            break;
        case LOSSLESS:
            // if reserved is used up
            if (((psize + ingress_bytes[port][qIndex] > reserveIngress[port][qIndex])
                 // AND if per queue headroom is used up.
                 && (psize + GetHdrmBytes(port, qIndex) > xoff[port][qIndex]) && GetHdrmBytes(port, qIndex) > 0)
                // or if the headroom pool is full
                || (psize + xoffTotalUsed > xoffTotal && GetHdrmBytes(port, qIndex) > 0)
                // if the ingresspool+headroom is full. With DT, this condition is redundant.
                // This is just to account for any badly configured buffer or buffer sharing if any.
                || (psize + totalUsed > ingressPool + xoffTotal)
                // if the switch buffer is full
                || (psize + totalUsed > bufferPool))
            {
                TraceLosslessIngressDrop(port, qIndex, GetHdrmBytes(port, qIndex), xoff[port][qIndex], psize);
                return false;
            }
            else
            {
                return true;
            }
            break;
        default:
            std::cout << "unknown type came in to CheckIngressAdmission function! This is not expected. Abort!" << std::endl;
            exit(1);
        }
    }

    bool SwitchMmu::CheckEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type)
    {

        switch (type)
        {
        case LOSSY:
            // if the egress queue length is greater than the threshold
            if ((psize + egress_bytes[port][qIndex] > DynamicThreshold(port, qIndex, "egress", type)
                 // AND if the reserved is usedup. THiS IS NOT SUPPORTED AT THE MOMENT. NO reserved at the egress.
                 // && psize + egress_bytes[port][qIndex] > reserveEgress[port][qIndex]
                 )
                // or if the egress pool is full
                || (psize + egressPoolUsed[type] > egressPool[type])
                // or if the switch buffer is full
                || (psize + totalUsed > bufferPool))
            {
                TraceEgressDrop(type, port, qIndex, egress_bytes[port][qIndex], DynamicThreshold(port, qIndex, "egress", type), psize);
                return false;
            }
            else
            {
                return true;
            }
            break;
        case LOSSLESS:
            // if threshold is exceeded
            if (((psize + egress_bytes[port][qIndex] > DynamicThreshold(port, qIndex, "egress", type))
                 // AND reserved is used up. THiS IS NOT SUPPORTED AT THE MOMENT. NO reserved at the egress.
                 // && (psize + egress_bytes[port][qIndex] > reserveEgress[port][qIndex])
                 )
                // or if the corresponding egress pool is used up
                || (psize + egressPoolUsed[type] > egressPool[type])
                // or if the switch buffer is full
                || (psize + totalUsed > bufferPool))
            {
                TraceEgressDrop(type, port, qIndex, egress_bytes[port][qIndex], DynamicThreshold(port, qIndex, "egress", type), psize);
                return false;
            }
            else
            {
                return true;
            }
            break;
        default:
            std::cout << "unknown type came in to CheckEgressAdmission function! This is not expected. Abort!" << std::endl;
            exit(1);
        }

        return true;
    }

    void SwitchMmu::UpdateIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type)
    {
        if (totalIngressReservedUsed >= GetIngressReservedUsed(port, qIndex)) // removing the old reserved used (will be updated next)
            totalIngressReservedUsed -= GetIngressReservedUsed(port, qIndex);
        else
            totalIngressReservedUsed = 0;

        ingress_bytes[port][qIndex] += psize;
        totalUsed += psize; // IMPORTANT: totalUsed is only updated in the ingress. No need to update in egress. Avoid double counting.

        totalIngressReservedUsed += GetIngressReservedUsed(port, qIndex); // updating with the new reserved used.

        // Update the total headroom used.
        if (type == LOSSLESS)
        {
            sharedPoolUsed += psize;
            uint64_t threshold = DynamicThreshold(port, qIndex, "ingress", LOSSLESS);
            // First, remove the previously used headroom corresponding to queue: port, qIndex. This will be updated with current value next.
            xoffTotalUsed -= xoffUsed[port][qIndex];
            // Second, get currently used headroom by the queue: port, qIndex and update `xoffUsed[port][qIndex]`
            // if headroom is zero
            if (xoffUsed[port][qIndex] == 0)
            {
                // if ingress bytes of the queue exceeds threshold, start using headroom. pfc pause will be triggered by CheckShouldPause later.
                uint64_t temp = 0;
                temp = ingress_bytes[port][qIndex];
                if (temp > threshold)
                {
                    // LOL: The commented part below was a HUGE mistake identified after debugging some of the lossless packets being dropped. It was a good lesson.
                    // xoffUsed[port][qIndex] += ingress_bytes[port][qIndex] - threshold;
                    xoffUsed[port][qIndex] += psize;
                    sharedPoolUsed -= psize;
                }
            }
            // if we are already using headroom, any incoming packet must be added to headroom, UNTIL the queue drains and headroom becomes zero.
            else if (xoffUsed[port][qIndex] > 0)
            {
                xoffUsed[port][qIndex] += psize;
                sharedPoolUsed -= psize;
            }
            // Finally, update the total headroom used by adding (since we removed before) the latest value of xoffUsed (headroom used) by the queue
            xoffTotalUsed += xoffUsed[port][qIndex]; // add the current used headroom to total headroom
                                                     // uint64_t inst_ingress_shared_bytes = ingress_bytes[port][qIndex]-xoffUsed[port][qIndex];
                                                     // ingressLpf_bytes[port][qIndex] = Reveriegamma * ingressLpf_bytes [port][qIndex] + (1-Reveriegamma) * (inst_ingress_shared_bytes);
        }
    }

    void SwitchMmu::UpdateEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type)
    {
        egress_bytes[port][qIndex] += psize;
        egressPoolUsed[type] += psize;
        if (type == LOSSY)
        {
            sharedPoolUsed += psize;
        }
    }

    void SwitchMmu::RemoveFromIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type)
    {
        // If else are simply unnecessary but its a safety check to avoid magic scenarios (if a packet vanishes in the buffer) where we
        // might assign negative value to unsigned intergers.

        if (totalIngressReservedUsed >= GetIngressReservedUsed(port, qIndex)) // removing the old reserved used (will be updated next)
            totalIngressReservedUsed -= GetIngressReservedUsed(port, qIndex);
        else
            totalIngressReservedUsed = 0;

        if (ingress_bytes[port][qIndex] >= psize)
            ingress_bytes[port][qIndex] -= psize;
        else
            ingress_bytes[port][qIndex] = 0;

        if (totalUsed >= psize) // IMPORTANT: totalUsed is only updated in the ingress. No need to update in egress. Avoid double counting.
            totalUsed -= psize;
        else
            totalUsed = 0;

        totalIngressReservedUsed += GetIngressReservedUsed(port, qIndex); // updating with the new reserved used.

        // Update the total headroom used.
        if (type == LOSSLESS)
        {
            uint64_t inst_ingress_shared_bytes = ingress_bytes[port][qIndex]; //-xoffUsed[port][qIndex];
            // First, remove the previously used headroom corresponding to queue: port, qIndex. This will be updated with current value next.
            if (xoffTotalUsed >= xoffUsed[port][qIndex])
                xoffTotalUsed -= xoffUsed[port][qIndex];
            else
                xoffTotalUsed = 0;
            // Second, check whether we are currently using any headroom. If not, nothing to do here: headroom is zero.
            if (xoffUsed[port][qIndex] > 0)
            {
                // Depending on the value of headroom used, the following cases arise:
                // 1. A packet can be removed entirely from the headroom
                // 2. Headroom occupancy is already less than the packet size.
                // So the dequeued packet decrements some part of headroom (emptying it) and some from ingress pool.
                if (xoffUsed[port][qIndex] >= psize)
                {
                    xoffUsed[port][qIndex] -= psize;
                }
                else
                {
                    sharedPoolUsed -= psize - xoffUsed[port][qIndex];
                    xoffUsed[port][qIndex] = 0;
                }
            }
            else
            {
                if (sharedPoolUsed >= psize)
                    sharedPoolUsed -= psize;
                else
                    sharedPoolUsed = 0;
            }
            xoffTotalUsed += xoffUsed[port][qIndex]; // add the current used headroom to total headroom
        }
    }

    void SwitchMmu::RemoveFromEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type)
    {
        if (egress_bytes[port][qIndex] >= psize)
            egress_bytes[port][qIndex] -= psize;
        else
            egress_bytes[port][qIndex] = 0;

        if (egressPoolUsed[type] >= psize)
            egressPoolUsed[type] -= psize;
        else
            egressPoolUsed[type] = 0;

        if (type == LOSSY)
        {
            if (sharedPoolUsed >= psize)
                sharedPoolUsed -= psize;
            else
                sharedPoolUsed = 0;
        }
    }

    uint64_t SwitchMmu::GetHdrmBytes(uint32_t port, uint32_t qIndex)
    {
        return xoffUsed[port][qIndex];
    }

    bool SwitchMmu::CheckShouldPause(uint32_t port, uint32_t qIndex)
    {
        return !paused[port][qIndex] && (GetHdrmBytes(port, qIndex) > 0);
    }

    bool SwitchMmu::CheckShouldResume(uint32_t port, uint32_t qIndex)
    {
        if (!paused[port][qIndex])
            return false;

        return GetHdrmBytes(port, qIndex) == 0 && (ingress_bytes[port][qIndex] < xon[port][qIndex] || ingress_bytes[port][qIndex] + xon_offset[port][qIndex] <= DynamicThreshold(port, qIndex, "ingress", LOSSLESS));
    }

    void SwitchMmu::SetPause(uint32_t port, uint32_t qIndex)
    {
        paused[port][qIndex] = true;
    }

    void SwitchMmu::SetResume(uint32_t port, uint32_t qIndex)
    {
        paused[port][qIndex] = false;
    }

    bool SwitchMmu::ShouldSendCN(uint32_t ifindex, uint32_t qIndex)
    {
        if (qIndex == 0)
            return false;
        if (egress_bytes[ifindex][qIndex] > kmax[ifindex])
            return true;
        if (egress_bytes[ifindex][qIndex] > kmin[ifindex])
        {
            double p = pmax[ifindex] * double(egress_bytes[ifindex][qIndex] - kmin[ifindex]) / (kmax[ifindex] - kmin[ifindex]);
            if (UniformVariable(0, 1).GetValue() < p)
                return true;
        }
        return false;
    }

    void SwitchMmu::ConfigEcn(uint32_t port, uint32_t _kmin, uint32_t _kmax, double _pmax)
    {
        kmin[port] = _kmin * 1000;
        kmax[port] = _kmax * 1000;
        pmax[port] = _pmax;
    }

    void SwitchMmu::TraceLossyIngressDrop(uint32_t port, uint32_t qIndex, uint64_t ingress_bytes, uint64_t threshold, uint32_t psize)
    {
        NS_LOG_INFO(Simulator::Now().GetTimeStep()
                    << " node=" << node_id << " dropping lossy packet at ingress admission port="
                    << port << " qIndex=" << qIndex << " pkt_size=" << psize << " ingress_bytes="
                    << ingress_bytes << " threshold=" << threshold);

        m_igDropTrace(LOSSY, node_id, port, qIndex, ingress_bytes, threshold, psize);
    }

    void SwitchMmu::TraceLosslessIngressDrop(uint32_t port, uint32_t qIndex, uint64_t hdrmBytes, uint64_t xoff, uint32_t psize)
    {
        NS_LOG_INFO(Simulator::Now().GetTimeStep()
                    << " node=" << node_id << " dropping lossless packet at ingress admission port="
                    << port << " qIndex=" << qIndex << " pkt_size=" << psize << " headroom="
                    << hdrmBytes << " xoff=" << xoff);

        m_igDropTrace(LOSSLESS, node_id, port, qIndex, hdrmBytes, xoff, psize);
    }

    void SwitchMmu::TraceEgressDrop(uint8_t type, uint32_t port, uint32_t qIndex, uint64_t egress_bytes, uint64_t threshold, uint32_t psize)
    {
        NS_LOG_INFO(Simulator::Now().GetTimeStep()
                    << " node=" << node_id << " dropping packet at egress admission type=" << type << " port="
                    << port << " qIndex=" << qIndex << " pkt_size=" << psize << " egress_bytes=" << egress_bytes << " threshold="
                    << threshold);

        m_egDropTrace(type, node_id, port, qIndex, egress_bytes, threshold, psize);
    }
}
