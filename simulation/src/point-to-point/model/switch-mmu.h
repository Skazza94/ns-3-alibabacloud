#ifndef SWITCH_MMU_H
#define SWITCH_MMU_H

/* =============
SwitchMmu class derived from https://github.com/inet-tub/ns3-datacenter
This version models the SONiC switch shared memory (purely based on understanding and experience).
Please contact the original authors for reporting bugs or further information.
================ */

#include <unordered_map>
#include <ns3/node.h>
#include <ns3/traced-callback.h>

#define LOSSLESS 0
#define LOSSY 1

namespace ns3
{
    class Packet;

    class SwitchMmu : public Object
    {
    public:
        static const uint32_t pCnt = 1025; // Number of ports used
        static const uint32_t qCnt = 8;    // Number of queues/priorities used

        static TypeId GetTypeId(void);

        SwitchMmu(void);

        bool CheckIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);
        bool CheckEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);
        void UpdateIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);
        void UpdateEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);
        void RemoveFromIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);
        void RemoveFromEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);

        bool CheckShouldPause(uint32_t port, uint32_t qIndex);
        bool CheckShouldResume(uint32_t port, uint32_t qIndex);
        void SetPause(uint32_t port, uint32_t qIndex);
        void SetResume(uint32_t port, uint32_t qIndex);

        bool ShouldSendCN(uint32_t ifindex, uint32_t qIndex);

        void ConfigEcn(uint32_t port, uint32_t _kmin, uint32_t _kmax, double _pmax);

        void SetBufferPool(uint64_t b);

        void SetIngressPool(uint64_t b);

        void SetSharedPool(uint64_t b);

        void SetEgressLossyPool(uint64_t b);

        void SetEgressLosslessPool(uint64_t b);

        void SetReserved(uint64_t b, uint32_t port, uint32_t q, std::string inout);
        void SetReserved(uint64_t b, std::string inout);

        void SetAlphaIngress(double value, uint32_t port, uint32_t q);
        void SetAlphaIngress(double value);

        void SetAlphaEgress(double value, uint32_t port, uint32_t q);
        void SetAlphaEgress(double value);

        void SetHeadroom(uint64_t b, uint32_t port, uint32_t q);
        void SetHeadroom(uint64_t b, uint32_t port);
        void SetHeadroom(uint64_t b);

        void SetXon(uint64_t b, uint32_t port, uint32_t q);
        void SetXon(uint64_t b);

        void SetXonOffset(uint64_t b, uint32_t port, uint32_t q);
        void SetXonOffset(uint64_t b);

        uint64_t DynamicThreshold(uint32_t port, uint32_t qIndex, std::string inout, uint32_t type);

        uint64_t GetHdrmBytes(uint32_t port, uint32_t qIndex);

        uint64_t GetIngressReservedUsed();

        uint64_t GetIngressReservedUsed(uint32_t port, uint32_t qIndex);

        uint64_t GetIngressSharedUsed();

        void SetPortCount(uint32_t pc) { portCount = pc; }

        void LogIngressDrop(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);
        void LogEgressDrop(uint32_t port, uint32_t qIndex, uint32_t psize, uint32_t type);

        // config
        uint32_t node_id;
        uint32_t kmin[pCnt], kmax[pCnt];
        double pmax[pCnt];
        uint32_t portCount;

        // Buffer pools
        uint64_t bufferPool;
        uint64_t ingressPool;
        uint64_t egressPool[2];
        uint64_t sharedPool;
        uint64_t xoffTotal;
        uint64_t totalIngressReserved;

        // aggregate run time
        uint64_t totalUsed;
        uint64_t egressPoolUsed[2];
        uint64_t xoffTotalUsed;
        uint64_t totalIngressReservedUsed;
        uint64_t sharedPoolUsed;

        // buffer configuration.
        uint64_t reserveIngress[pCnt][qCnt];
        uint64_t reserveEgress[pCnt][qCnt];
        double alphaEgress[pCnt][qCnt];
        double alphaIngress[pCnt][qCnt];
        uint64_t xoff[pCnt][qCnt];
        uint64_t xon[pCnt][qCnt];
        uint64_t xon_offset[pCnt][qCnt];

        // per queue run time
        uint64_t ingress_bytes[pCnt][qCnt];
        uint64_t hdrm_bytes[pCnt][qCnt];
        uint32_t paused[pCnt][qCnt];
        uint64_t egress_bytes[pCnt][qCnt];
        uint64_t xoffUsed[pCnt][qCnt];

    private:
        // Trace functions
        void TraceLossyIngressDrop(uint32_t port, uint32_t qIndex, uint64_t ingress_bytes, uint64_t threshold, uint32_t psize);
        void TraceLosslessIngressDrop(uint32_t port, uint32_t qIndex, uint64_t hdrmBytes, uint64_t xoff, uint32_t psize);
        void TraceEgressDrop(uint8_t type, uint32_t port, uint32_t qIndex, uint64_t egress_bytes, uint64_t threshold, uint32_t psize);

        // Drops trace functions
        TracedCallback<uint8_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint32_t> m_igDropTrace;
        TracedCallback<uint8_t, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint32_t> m_egDropTrace;
    };
} /* namespace ns3 */

#endif /* SWITCH_MMU_H */
