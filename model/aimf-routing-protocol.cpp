/* -*-
 * File:   aimf-routing-protocol.cpp
 * Author: debonatis
 * 
 * Created on 19 February 2016, 12:59
 */
#define NS_LOG_APPEND_CONTEXT                                   \
  if (GetObject<Node> ()) { std::clog << "[node " << GetObject<Node> ()->GetId () << "] "; }


#include "ns3/log.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"

#include "ns3/names.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/ipv4-route.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/ipv4-header.h"
#include "ns3/aimf-header.h"
#include "ns3/udp-socket.h"
#include "ns3/aimf-repository.h"
#include "ns3/aimf-routing-protocol.h"
#include "aimf-routing-protocol.h"




///
/// \brief Gets the delay between a given time and the current time.
///
/// If given time is previous to the current one, then this macro returns
/// a number close to 0. This is used for scheduling events at a certain moment.
///
#define DELAY(time) (((time) < (Simulator::Now ())) ? Seconds (0.000001) : \
                     (time - Simulator::Now () + Seconds (0.000001)))



///
/// \brief Period at which a node must cite neighbor.
///
/// We only use this value in order to define AIMF_NEIGHB_HOLD_TIME.
///
#define AIMF_REFRESH_INTERVAL   m_helloInterval


/********** Holding times **********/

/// Neighbor holding time.
#define AIMF_NEIGHB_HOLD_TIME   Time (3 * AIMF_REFRESH_INTERVAL)
#define IS_RECEVING_MCAST  Time (4*AIMF_REFRESH_INTERVAL)



/********** Willingness **********/

/// Willingness for forwarding packets: never.
#define AIMF_WILL_NEVER         0
/// Willingness for forwarding packets: low.
#define AIMF_WILL_LOW           1
/// Willingness for forwarding packets: medium.
#define AIMF_WILL_DEFAULT       3
/// Willingness for forwarding packets: high.
#define AIMF_WILL_HIGH          6
/// Willingness for forwarding packets: always.
#define AIMF_WILL_ALWAYS        7

#define AIMF_MCAST_ADR "230.0.0.30"
/********** Miscellaneous constants **********/


/// Maximum allowed sequence number.
#define AIMF_MAX_SEQ_NUM        65535

#define AIMF_PORT_NUMBER 1337


using std::make_pair;
namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("AimfRoutingProtocol");

    namespace aimf {

        NS_OBJECT_ENSURE_REGISTERED(RoutingProtocol);

        TypeId
        RoutingProtocol::GetTypeId(void) {
            static TypeId tid = TypeId("ns3::aimf::RoutingProtocol")
                    .SetParent<Ipv4RoutingProtocol> ()
                    .SetGroupName("Aimf")
                    .AddConstructor<RoutingProtocol> ()
                    .AddAttribute("HelloInterval", "HELLO messages emission interval.",
                    TimeValue(Seconds(2)),
                    MakeTimeAccessor(&RoutingProtocol::m_helloInterval),
                    MakeTimeChecker())
                    .AddAttribute("olsrPollInterval", "The poll interval of the OLSR routing table.",
                    TimeValue(Seconds(5)),
                    MakeTimeAccessor(&RoutingProtocol::m_olsrCheckInterval),
                    MakeTimeChecker())
                    .AddAttribute("Willingness", "Willingness of a node to carry and forward traffic compared to other similar nodes.",
                    EnumValue(AIMF_WILL_DEFAULT),
                    MakeEnumAccessor(&RoutingProtocol::m_willingness),
                    MakeEnumChecker(AIMF_WILL_NEVER, "never",
                    AIMF_WILL_LOW, "low",
                    AIMF_WILL_DEFAULT, "default",
                    AIMF_WILL_HIGH, "high",
                    AIMF_WILL_ALWAYS, "always"))
                    .AddTraceSource("RoutingTableChanged", "The AIMF routing table has changed.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_routingTableChanged),
                    "ns3::aimf::RoutingProtocol::TableChangeTracedCallback")
                    .AddTraceSource("Rx", "Receive AIMF packet.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_rxHelloPacketTrace),
                    "ns3::aimf::RoutingProtocol::PacketTxRxTracedCallback")
                    .AddTraceSource("Tx", "Send AIMF packet.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_txHelloPacketTrace),
                    "ns3::aimf::RoutingProtocol::PacketTxRxTracedCallback")
                    .AddTraceSource("McTx", "Forward multicast packet.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_txMcastPacketTrace),
                    "ns3::aimf::RoutingProtocol::PacketTxRxTracedCallback")
                    .AddTraceSource("McRx", "Receive multicast packet.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_rxMcastPacketTrace),
                    "ns3::aimf::RoutingProtocol::PacketTxRxTracedCallback")
                    ;
            return tid;
        }

        RoutingProtocol::RoutingProtocol() :
        m_routingTableAssociation(0),
        m_ipv4(0),
        m_helloTimer(Timer::CANCEL_ON_DESTROY), m_olsrCheck(Timer::CANCEL_ON_DESTROY) {
            m_uniformRandomVariable2 = CreateObject<UniformRandomVariable> ();



        }

        RoutingProtocol::~RoutingProtocol() {
        };

        ///< Data structure for the multicast routing table.
        std::map<ns3::Ipv4Address, Ipv4MulticastRoutingTableEntry> m_table;
        ///< Data structure for the routing table.

        ns3::Ptr<ns3::Ipv4StaticRouting> m_RoutingTable;

        ns3::EventGarbageCollector m_events;

        /// Packets sequence number counter.
        uint16_t m_packetSequenceNumber;
        /// Messages sequence number counter.
        uint16_t m_messageSequenceNumber;

        /// HELLO messages' emission interval.
        Time m_helloInterval;
        /// OLSR check interval.
        Time m_olsrCheckInterval;

        volatile uint8_t m_willingness;
        volatile uint8_t m_lastwill;

        std::vector<olsr::RoutingTableEntry> olsrTable;

        /// Internal state with all needed data structs.

        ns3::Ptr<ns3::Ipv4> m_ipv4;
        Ptr<olsr::RoutingProtocol> olsr_onNode;

        uint16_t RoutingProtocol::GetPacketSequenceNumber() {
            m_packetSequenceNumber = (m_packetSequenceNumber + 1) % (AIMF_MAX_SEQ_NUM + 1);
            return m_packetSequenceNumber;
        }

        /// Increments message sequence number and returns the new value.

        uint16_t RoutingProtocol::GetMessageSequenceNumber() {
            m_messageSequenceNumber = (m_messageSequenceNumber + 1) % (AIMF_MAX_SEQ_NUM + 1);
            return m_messageSequenceNumber;
        }

        void RoutingProtocol::RecvAimf(Ptr<Socket> socket) {
            Ptr<Packet> receivedPacket;
            Address sourceAddress;
            receivedPacket = socket->RecvFrom(sourceAddress);
            Ptr<Packet> k = receivedPacket->Copy();

            m_rxHelloPacketTrace(k, m_ipv4, socket->GetBoundNetDevice()->GetIfIndex());

            InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom(sourceAddress);
            Ipv4Address senderIfaceAddr = inetSourceAddr.GetIpv4();
            Ipv4Address receiverIfaceAddr = m_socketAddresses[socket].GetLocal();
            NS_ASSERT(receiverIfaceAddr != Ipv4Address());
            NS_LOG_DEBUG("AIMF node " << m_mainAddress << " received a AIMF packet from "
                    << senderIfaceAddr << " to " << receiverIfaceAddr);

            // All routing messages are sent from and to port AIMF_PORT_NUMBER,
            // so we check it.
            NS_ASSERT(inetSourceAddr.GetPort() == AIMF_PORT_NUMBER);

            Ptr<Packet> packet = receivedPacket;

            aimf::PacketHeader aimfPacketHeader;
            packet->RemoveHeader(aimfPacketHeader);
            NS_ASSERT(aimfPacketHeader.GetPacketLength() >= aimfPacketHeader.GetSerializedSize());
            uint32_t sizeLeft = aimfPacketHeader.GetPacketLength() - aimfPacketHeader.GetSerializedSize();

            MessageList messages;

            while (sizeLeft) {
                MessageHeader messageHeader;
                if (packet->RemoveHeader(messageHeader) == 0)
                    NS_ASSERT(false);

                sizeLeft -= messageHeader.GetSerializedSize();

                NS_LOG_DEBUG("Aimf Msg received with type "
                        << std::dec << int (messageHeader.GetMessageType())
                        << " TTL=" << int (messageHeader.GetTimeToLive())
                        << " origAddr=" << messageHeader.GetOriginatorAddress());
                messages.push_back(messageHeader);
            }



            for (MessageList::const_iterator messageIter = messages.begin();
                    messageIter != messages.end(); messageIter++) {
                const MessageHeader &messageHeader = *messageIter;
                // If ttl is less than or equal to zero, or
                // the receiver is the same as the originator,
                // the message must be silently dropped
                if (messageHeader.GetTimeToLive() == 0
                        || messageHeader.GetOriginatorAddress() == m_mainAddress) {
                    packet->RemoveAtStart(messageHeader.GetSerializedSize()
                            - messageHeader.GetSerializedSize());
                    continue;
                }
                switch (messageHeader.GetMessageType()) {
                    case aimf::MessageHeader::HELLO_MESSAGE:
                        NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                                << "s AIMF node " << m_mainAddress
                                << " received HELLO message of size " << messageHeader.GetSerializedSize());
                        ProcessHello(messageHeader, receiverIfaceAddr, senderIfaceAddr);
                        break;


                    default:
                        NS_LOG_DEBUG("AIMF message type " <<
                                int (messageHeader.GetMessageType()) <<
                                " not implemented");
                }

            }
            RoutingTableComputation();
        }

        void
        RoutingProtocol::ReceivingMulticast(Ipv4Address group) {

            Time * t = m_state.FindTimer(group);
            if (t == NULL) {
                Time k = Simulator::Now() + IS_RECEVING_MCAST + Time::FromInteger((int) (7 - m_willingness)*2, Time::S);
                m_state.AddTimer(group, k);
                Simulator::ScheduleWithContext(group.Get() + GetObject<Node> ()->GetId(), DELAY(k), &aimf::RoutingProtocol::ReceivingMulticast, this, group);
                t = m_state.FindTimer(group);
            }
            if (*t < Simulator::Now()) {
                NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " " << group << " is not spotted. ");
                NS_LOG_DEBUG("Old time: " << t->GetSeconds() << ".");
                *t = *t + IS_RECEVING_MCAST + Seconds(m_willingness);
                NS_LOG_DEBUG("New time: " << t->GetSeconds() << ".");
            } else if (*t > Simulator::Now()) {
                NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " " << group << " is spotted. ");
                NS_LOG_DEBUG("Old time: " << t->GetSeconds() << ".");
                *t = Simulator::Now() + IS_RECEVING_MCAST + Time::FromInteger((int) (AIMF_WILL_ALWAYS - m_willingness), Time::S);
                NS_LOG_DEBUG("New time: " << t->GetSeconds() << ".");
            }
        }
        Ptr<Ipv4Route>
        RoutingProtocol::LookupStatic(Ipv4Address dest, Ptr<NetDevice> oif) {
            NS_LOG_FUNCTION(this << dest << " " << oif);
            Ptr<Ipv4Route> rtentry = 0;
            uint16_t longest_mask = 0;
            uint32_t shortest_metric = 0xffffffff;
            if (dest.IsLocalMulticast()) {
                NS_ASSERT_MSG(oif, "Try to send on link-local multicast address, and no interface index is given!");
                NS_LOG_DEBUG("src address= " << m_ipv4->GetAddress(oif->GetIfIndex() + 1, 0).GetLocal());
                rtentry = Create<Ipv4Route> ();
                rtentry->SetDestination(dest);
                rtentry->SetGateway(m_ipv4->GetAddress(oif->GetIfIndex() + 1, 0).GetLocal());
                rtentry->SetOutputDevice(oif);
                rtentry->SetSource(m_ipv4->GetAddress(oif->GetIfIndex() + 1, 0).GetLocal());
                return rtentry;
            }
            for (NetworkRoutesI i = m_networkRoutes.begin();
                    i != m_networkRoutes.end();
                    i++) {
                Ipv4RoutingTableEntry *j = i->first;
                uint32_t metric = i->second;
                Ipv4Mask mask = (j)->GetDestNetworkMask();
                uint16_t masklen = mask.GetPrefixLength();
                Ipv4Address entry = (j)->GetDestNetwork();
                NS_LOG_LOGIC("Searching for route to " << dest << ", checking against route to " << entry << "/" << masklen);
                if (mask.IsMatch(dest, entry)) {
                    NS_LOG_LOGIC("Found global network route " << j << ", mask length " << masklen << ", metric " << metric);
                    if (oif != 0) {
                        if (oif != m_ipv4->GetNetDevice(j->GetInterface())) {
                            NS_LOG_LOGIC("Not on requested interface, skipping");
                            continue;
                        }
                    }
                    if (masklen < longest_mask) // Not interested if got shorter mask
                    {
                        NS_LOG_LOGIC("Previous match longer, skipping");
                        continue;
                    }
                    if (masklen > longest_mask) // Reset metric if longer masklen
                    {
                        shortest_metric = 0xffffffff;
                    }
                    longest_mask = masklen;
                    if (metric > shortest_metric) {
                        NS_LOG_LOGIC("Equal mask length, but previous metric shorter, skipping");
                        continue;
                    }
                    shortest_metric = metric;
                    Ipv4RoutingTableEntry* route = (j);
                    uint32_t interfaceIdx = route->GetInterface();
                    rtentry = Create<Ipv4Route> ();
                    rtentry->SetDestination(route->GetDest());
                    rtentry->SetSource(SourceAddressSelection(interfaceIdx, route->GetDest()));
                    rtentry->SetGateway(route->GetGateway());
                    rtentry->SetOutputDevice(m_ipv4->GetNetDevice(interfaceIdx));
                }
            }
            if (rtentry != 0) {
                NS_LOG_LOGIC("Matching route via " << rtentry->GetGateway() << " at the end");
            } else {
                NS_LOG_LOGIC("No matching route to " << dest << " found");
            }
            return rtentry;
        }
        Ptr<Ipv4MulticastRoute>
        RoutingProtocol::LookupStatic(
                Ipv4Address origin,
                Ipv4Address group,
                uint32_t interface, uint8_t ttl) {
            NS_LOG_FUNCTION(this << origin << " " << group << " " << interface);
            Ptr<Ipv4MulticastRoute> mrtentry = 0;
            NS_LOG_DEBUG("Node " << m_mainAddress << "(S,G) pair: (" << origin << "," << group << ")");
            for (std::map<Ipv4Address, Ipv4MulticastRoutingTableEntry>::const_iterator i = m_table.begin();
                    i != m_table.end();
                    i++) {
                Ipv4MulticastRoutingTableEntry route = i->second;

                if (origin == route.GetOrigin() && group == route.GetGroup() && group == i->first) {
                    // TODO Use will for each multicast flow. The TTL should be used.
                    NS_LOG_LOGIC("Found multicast source specific route" << i->first);
                }
                if (group == route.GetGroup() && group == i->first) {
                    if (interface == Ipv4::IF_ANY ||
                            interface == route.GetInputInterface()) {
                        ReceivingMulticast(group);
                        NS_LOG_LOGIC("Found multicast route" << i->first);
                        mrtentry = Create<Ipv4MulticastRoute> ();
                        mrtentry->SetGroup(route.GetGroup());
                        mrtentry->SetOrigin(route.GetOrigin());
                        mrtentry->SetParent(route.GetInputInterface());
                        for (uint32_t j = 0; j < route.GetNOutputInterfaces(); j++) {
                            if (route.GetOutputInterface(j)) {
                                NS_LOG_LOGIC("Setting output interface index " << route.GetOutputInterface(j));
                                mrtentry->SetOutputTtl(route.GetOutputInterface(j), Ipv4MulticastRoute::MAX_TTL - 1);
                            }
                        }
                        return mrtentry;
                    }
                    //                    else{
                    //                        mrtentry = Create<Ipv4MulticastRoute> ();
                    //                        mrtentry->SetGroup(route.GetGroup());
                    //                        mrtentry->SetOrigin(route.GetOrigin());
                    //                        mrtentry->SetParent(2);
                    //                        mrtentry->SetOutputTtl(2, Ipv4MulticastRoute::MAX_TTL - 1);
                    //                        
                    //                    }
                    ///ALERT PIM there is a "new" multicast group spotted on the MANET                    
                }
            }

            return mrtentry;
        }

        Ipv4Address
        RoutingProtocol::SourceAddressSelection(uint32_t interfaceIdx, Ipv4Address dest) {
            NS_LOG_FUNCTION(this << interfaceIdx << " " << dest);
            if (m_ipv4->GetNAddresses(interfaceIdx) == 1) // common case
            {
                return m_ipv4->GetAddress(interfaceIdx, 0).GetLocal();
            }
            Ipv4Address candidate = m_ipv4->GetAddress(interfaceIdx, 0).GetLocal();
            for (uint32_t i = 0; i < m_ipv4->GetNAddresses(interfaceIdx); i++) {
                Ipv4InterfaceAddress test = m_ipv4->GetAddress(interfaceIdx, i);
                if (test.GetLocal().CombineMask(test.GetMask()) == dest.CombineMask(test.GetMask())) {
                    if (test.IsSecondary() == false) {
                        return test.GetLocal();
                    }
                }
            }
            return candidate;
        }

        void
        RoutingProtocol::SetIpv4(Ptr<Ipv4> ipv4) {
            NS_ASSERT(ipv4 != 0);
            NS_ASSERT(m_ipv4 == 0);
            NS_LOG_DEBUG("Created aimf::RoutingProtocol");
            m_helloTimer.SetFunction(&RoutingProtocol::HelloTimerExpire, this);
            m_olsrCheck.SetFunction(&RoutingProtocol::OlsrTimerExpire, this);
            m_packetSequenceNumber = AIMF_MAX_SEQ_NUM;
            m_messageSequenceNumber = AIMF_MAX_SEQ_NUM;
            Ptr<Ipv4RoutingProtocol> nodeRouting = (ipv4->GetRoutingProtocol());
            Ptr<Ipv4ListRouting> nodeListRouting = DynamicCast<Ipv4ListRouting> (nodeRouting);
            for (uint32_t i = 0; i < nodeListRouting->GetNRoutingProtocols(); i++) {
                int16_t priority;
                Ptr<Ipv4RoutingProtocol> temp = nodeListRouting->GetRoutingProtocol(i, priority);
                if (DynamicCast<olsr::RoutingProtocol> (temp)) {
                    m_olsr_onNode = DynamicCast<olsr::RoutingProtocol>(temp);
                }
            }
            m_ipv4 = ipv4;
        }
        void RoutingProtocol::DoDispose() {
            m_ipv4 = 0;
            m_table.clear();
            m_state.ClearTimer();
            for (std::map< Ptr<Socket>, Ipv4InterfaceAddress >::iterator iter = m_socketAddresses.begin();
                    iter != m_socketAddresses.end(); iter++) {
                iter->first->Close();
            }
            m_socketAddresses.clear();

            m_helloTimer.Cancel();
            m_olsrCheck.Cancel();
            forward = false;
            m_willingness = 1;
        }
        void RoutingProtocol::DoStop() {
            m_table.clear();
            m_state.ClearTimer();
            for (std::map< Ptr<Socket>, Ipv4InterfaceAddress >::iterator iter = m_socketAddresses.begin();
                    iter != m_socketAddresses.end(); iter++) {
                iter->first->Close();
            }
            m_socketAddresses.clear();
            m_helloTimer.Cancel();
            m_olsrCheck.Cancel();
            forward = false;
        }
        void RoutingProtocol::DoStart() {
            DoInitialize();
        }
        Ptr<Ipv4Route>
        RoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno & sockerr) {
            Ptr<Ipv4Route> rtentry = 0;
            if (header.GetDestination().IsMulticast()) {
                NS_LOG_LOGIC("RouteOutput()::Multicast destination");
            }
            rtentry = LookupStatic(header.GetDestination(), oif);
            if (rtentry) {
                sockerr = Socket::ERROR_NOTERROR;
            } else {
                sockerr = Socket::ERROR_NOROUTETOHOST;
            }
            return rtentry;
        }
        bool RoutingProtocol::RouteInput(Ptr<const Packet> p,
                const Ipv4Header &header, Ptr<const NetDevice> idev,
                UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                LocalDeliverCallback lcb, ErrorCallback ecb) {
            NS_LOG_FUNCTION(this << p << header << header.GetSource() << header.GetDestination() << idev << &ucb << &mcb << &lcb << &ecb);
            NS_ASSERT(m_ipv4 != 0);
            // Check if input device supports IP 
            NS_ASSERT(m_ipv4->GetInterfaceForDevice(idev) >= 0);
            if (header.GetDestination().IsMulticast()) {
                NS_LOG_LOGIC("Multicast destination");
                Ptr<Ipv4MulticastRoute> mrtentry = LookupStatic(header.GetSource(),
                        header.GetDestination(), m_ipv4->GetInterfaceForDevice(idev), header.GetTtl());
                if (mrtentry) {
                    m_rxMcastPacketTrace(p->Copy(), m_ipv4, idev->GetIfIndex());
                    NS_LOG_LOGIC("Multicast rute ok");
                    if (!forward) {
                        return false;
                    }
                    mcb(mrtentry, p, header);
                    m_txMcastPacketTrace(p->Copy(), m_ipv4, idev->GetIfIndex());
                    NS_LOG_DEBUG("Packet routed with destination: " << header.GetDestination() << " and source: " << header.GetSource() << " Will = " << (int) m_willingness << " . It has a TTl of " << int (header.GetTtl()) << "---------------------------------------------------------------------------------------------");
                    return true;
                } else {
                    NS_LOG_LOGIC("Multicast rute er ikke funnet");
                    return false;
                }
            }
            return false;
        }
        bool
        RoutingProtocol::IsMyOwnAddress(const Ipv4Address & a) const {
            for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
                    m_socketAddresses.begin(); j != m_socketAddresses.end(); ++j) {
                Ipv4InterfaceAddress iface = j->second;
                if (a == iface.GetLocal()) {
                    return true;
                }
            }
            return false;
        }
        void
        RoutingProtocol::NotifyInterfaceUp(uint32_t i) {
        }
        void
        RoutingProtocol::NotifyInterfaceDown(uint32_t i) {
        }
        void
        RoutingProtocol::NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address) {
        }
        void
        RoutingProtocol::NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address) {
        }
        std::vector<Ipv4MulticastRoutingTableEntry>
        RoutingProtocol::GetRoutingTableEntries() const {
            std::vector<Ipv4MulticastRoutingTableEntry> retval;
            for (std::map<Ipv4Address, Ipv4MulticastRoutingTableEntry>::const_iterator iter = m_table.begin();
                    iter != m_table.end(); iter++) {
                retval.push_back(iter->second);
            }
            return retval;
        }
        int64_t
        RoutingProtocol::AssignStreams(int64_t stream) {
            NS_LOG_FUNCTION(this << stream);
            m_uniformRandomVariable2->SetStream(stream);
            return 1;
        }
        void
        RoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const {
            std::ostream* os = stream->GetStream();
            *os << "Source\t\tGroup\t\tInterface\tDistance\n";
            for (std::map<Ipv4Address, Ipv4MulticastRoutingTableEntry>::const_iterator iter = m_table.begin();
                    iter != m_table.end(); iter++) {
                *os << iter->second.GetOrigin() << "\t\t";
                *os << iter->second.GetGroup() << "\t\t";
                if (Names::FindName(m_ipv4->GetNetDevice(iter->second.GetInputInterface())) != "") {
                    *os << Names::FindName(m_ipv4->GetNetDevice(iter->second.GetInputInterface())) << "\t\t";
                } else {
                    *os << iter->second.GetInputInterface() << "\t\t";
                }
                *os << iter->second.GetNOutputInterfaces() << "\t";
                *os << "\n";
            }
        }
        void RoutingProtocol::ChangeWillingness(uint8_t will) {
            m_willingness = will;
            m_lastwill = will;
            SendHello();
        }
        void RoutingProtocol::DoInitialize() {
            Ipv4Address loopback("127.0.0.1");
            m_lastwill = m_willingness;
            NS_LOG_DEBUG("MainAddress " << m_mainAddress);
            if (m_mainAddress == Ipv4Address()) {
                for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
                    // Use primary address, if multiple
                    Ipv4Address addr = m_ipv4->GetAddress(i, 0).GetLocal();
                    NS_LOG_DEBUG("One of the node's addresses: " << addr);
                    if (addr != loopback) {
                        m_mainAddress = addr;
                        break;
                    }
                }
                NS_ASSERT(m_mainAddress != Ipv4Address());
            }
            bool canRunAimf = false;
            for (uint32_t i = 0; i < (m_ipv4->GetNInterfaces()); i++) {
                Ipv4Address addr = m_ipv4->GetAddress(i, 0).GetLocal();
                if (addr == loopback)
                    continue;
                if (m_interfaceExclusions.find(i) != m_interfaceExclusions.end())
                    continue;
                // Create a socket to listen only on this interface
                Ptr<Socket> socket = Socket::CreateSocket(GetObject<Node> (),
                        UdpSocketFactory::GetTypeId());
                NS_LOG_DEBUG("Trying to bind X on " << addr);
                InetSocketAddress inetAddr(Ipv4Address::GetAny(), AIMF_PORT_NUMBER);
                socket->SetRecvCallback(MakeCallback(&RoutingProtocol::RecvAimf, this));
                if (socket->Bind(inetAddr)) {
                    NS_FATAL_ERROR("Failed to bind() AIMF socket " << addr);
                }
                socket->BindToNetDevice(m_ipv4->GetNetDevice(i));
                m_socketAddresses[socket] = m_ipv4->GetAddress(i, 0);
                NS_LOG_DEBUG("One of the node's addresses which will be used to send hellos: " << m_ipv4->GetAddress(i, 0).GetLocal() << " .");
                m_mainAddress = m_ipv4->GetAddress(i, 0).GetLocal();
                NS_LOG_DEBUG("Starting AIMF on node " << m_mainAddress);
                canRunAimf = true;
                Ipv4RoutingTableEntry * defMcRoute = new Ipv4RoutingTableEntry();
                *defMcRoute = Ipv4RoutingTableEntry::CreateHostRouteTo(Ipv4Address(AIMF_MCAST_ADR), i);
                m_networkRoutes.push_back(make_pair<Ipv4RoutingTableEntry*, uint32_t>(defMcRoute, 1));
            }
            if (canRunAimf) {
                m_uniformRandomVariable2->SetStream(m_mainAddress.Get());
                HelloTimerExpire();
                Simulator::Schedule(Time(Simulator::Now() + Seconds(10)), &RoutingProtocol::OlsrTimerExpire, this);
                forward = true;
                NS_LOG_DEBUG("AIMF on node " << m_mainAddress << " started");
            }
        }
        void
        RoutingProtocol::AssociationTupleTimerExpire(Ipv4Address advertiser, Ipv4Address group, Ipv4Address source) {
            AssociationTuple *tuple = m_state.FindAssociationTuple(advertiser, group, source);
            if (tuple == NULL) {
                return;
            }
            if (tuple->expirationTime < Simulator::Now()) {
                RemoveAssociationTuple(*tuple);
            } else {
                m_events.Track(Simulator::Schedule(DELAY(tuple->expirationTime),
                        &RoutingProtocol::AssociationTupleTimerExpire,
                        this, advertiser, group, source));
            }
        }
        void
        RoutingProtocol::RemoveAssociationTuple(const AssociationTuple &tuple) {
            m_state.EraseAssociationTuple(tuple);
            //RemoveEntry(tuple.group);
        }
        void
        RoutingProtocol::AddAssociationTuple(const AssociationTuple &tuple) {
            m_state.InsertAssociationTuple(tuple);
        }
        void
        RoutingProtocol::ProcessHello(const aimf::MessageHeader &msg,
                const Ipv4Address &receiverIface,
                const Ipv4Address & senderIface) {
            NS_LOG_FUNCTION(msg << receiverIface << senderIface);
            const aimf::MessageHeader::Hello &hello = msg.GetHello();
#ifdef NS3_LOG_ENABLE
            Time now = Simulator::Now();
            // 2. For each (group, source) pair in the
            // message:
            for (std::vector<aimf::MessageHeader::Hello::Association>::const_iterator it = hello.associations.begin();
                    it != hello.associations.end(); it++) {
                AssociationTuple *tuple = m_state.FindAssociationTuple(msg.GetOriginatorAddress(), it->group, it->source);
                if (tuple != NULL) {
                    tuple->expirationTime = now + msg.GetVTime();
                } else {
                    AssociationTuple assocTuple = {
                        msg.GetOriginatorAddress(),
                        it->group,
                        it->source,
                        now + msg.GetVTime(),
                        it->willGroupSSM
                    };
                    AddAssociationTuple(assocTuple);
                    //Schedule Association Tuple deletion
                    Simulator::Schedule(DELAY(assocTuple.expirationTime),
                            &RoutingProtocol::AssociationTupleTimerExpire, this,
                            assocTuple.advertiser, assocTuple.group, assocTuple.source);
                }
            }
#endif // NS3_LOG_ENABLE
            PopulateNeighborSet(msg, now);
        }
        void RoutingProtocol::SetInterfaceExclusions(std::set<uint32_t> exceptions) {
            m_interfaceExclusions = exceptions;
        }
        void RoutingProtocol::SetNetdevicelistener(std::set<uint32_t> listen) {
            m_netdevice = listen;
        }
        void
        RoutingProtocol::AddNeigbour(NeighborTuple tuple) {
            m_state.InsertNeighborTuple(tuple);
        }
        void
        RoutingProtocol::PopulateNeighborSet(const aimf::MessageHeader &msg,
                const Time & now) {
            NS_LOG_DEBUG("received willingness " << int(msg.GetHello().willingness) << " from " << msg.GetOriginatorAddress());
            NeighborTuple *tuple = m_state.FindNeighborTuple(msg.GetOriginatorAddress());
            if (tuple != NULL) {
                NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                        << "s AIMF node " << m_mainAddress
                        << " updating " << tuple->neighborMainAddr << "'s expiration time from " << tuple->expirationTime.GetSeconds() << " to " << ((Time) now + msg.GetVTime()).GetSeconds());
                tuple->expirationTime = now + msg.GetVTime();
                tuple->willingness = msg.GetHello().willingness;
            } else {
                NeighborTuple nb_tuple = {msg.GetOriginatorAddress()
                    , now + msg.GetVTime(), msg.GetHello().willingness};
                AddNeigbour(nb_tuple);
                NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                        << "s AIMF node " << m_mainAddress
                        << " adding " << nb_tuple.neighborMainAddr << " as neighbour. Scheduled for removal at: " << nb_tuple.expirationTime.GetSeconds());
                Simulator::Schedule(DELAY(nb_tuple.expirationTime), &RoutingProtocol::RemoveNeighborset, this, nb_tuple.neighborMainAddr);
            }
        }
        void
        RoutingProtocol::RemoveNeighborset(Ipv4Address adress) {
            NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                    << "s AIMF node " << m_mainAddress
                    << " erasing node " << adress);
            NeighborTuple *tuple = m_state.FindNeighborTuple(adress);
            if (tuple == NULL) {
                return;
            }
            if (tuple->expirationTime < Simulator::Now()) {
                m_state.EraseNeighborTuple(adress);
            } else {
                m_events.Track(Simulator::Schedule(DELAY(tuple->expirationTime), &RoutingProtocol::RemoveNeighborset, this, tuple->neighborMainAddr));
            }
        }
        void
        RoutingProtocol::Clear() {
            NS_LOG_FUNCTION_NOARGS();
            m_table.clear();
        }
        void
        RoutingProtocol::RemoveEntry(Ipv4Address const &group) {
            m_table.erase(group);
        }
        void
        RoutingProtocol::AddEntry(Ipv4Address const &group,
                Ipv4Address const &source,
                uint32_t inputInterface,
                std::vector<uint32_t> outputInterfaces) {
            NS_LOG_FUNCTION(this << group << source << m_mainAddress);
            NS_ASSERT(m_ipv4);
            Ipv4MulticastRoutingTableEntry &entry = m_table[group];
            entry = entry.CreateMulticastRoute(source, group, inputInterface, outputInterfaces);
        }
        void
        RoutingProtocol::RoutingTableComputation() {
            NS_LOG_DEBUG(Simulator::Now().GetSeconds() << " s: Node " << m_mainAddress
                    << ": RoutingTableComputation begin...");
            Clear();
            std::vector<uint32_t> outint(m_netdevice.size() - 1);
            std::copy(m_netdevice.begin(), m_netdevice.end(), std::back_inserter(outint));
            const Associations &localHmaAssociations = m_state.GetAssociations();
            for (Associations::const_iterator assocIterator = localHmaAssociations.begin();
                    assocIterator != localHmaAssociations.end(); assocIterator++) {
                Association const &localHmaAssoc = *assocIterator;
                AddEntry(localHmaAssoc.group, localHmaAssoc.source, m_ipv4->GetInterfaceForAddress(m_mainAddress), outint);
                NS_LOG_DEBUG("Node " << m_mainAddress << ": Adding local (" << localHmaAssoc.group << "," << localHmaAssoc.source << ") pair to routing table. OuputInterface is: " << m_ipv4->GetAddress(outint.front(), 0).GetLocal() << " and Input is: " << m_ipv4->GetAddress(1, 0).GetLocal());
            }
            const AssociationSet &localHmaAssociationSets = m_state.GetAssociationSet();
            for (AssociationSet::const_iterator assocSetIterator = localHmaAssociationSets.begin();
                    assocSetIterator != localHmaAssociationSets.end(); assocSetIterator++) {
                AssociationTuple const &localHmaAssocSet = *assocSetIterator;
                AddEntry(localHmaAssocSet.group, localHmaAssocSet.source, m_ipv4->GetInterfaceForAddress(m_mainAddress), outint);
                NS_LOG_DEBUG("Node " << m_mainAddress << ": Adding adjacent (" << localHmaAssocSet.group << "," << localHmaAssocSet.source << ") pair to routing table. OuputInterface is: " << m_ipv4->GetAddress(outint.front(), 0).GetLocal() << " and Input is: " << m_ipv4->GetAddress(1, 0).GetLocal());
            }
            NS_LOG_DEBUG("Node " << m_mainAddress << ": RoutingTableComputation end.");
            m_routingTableChanged(m_table.size());
        }
        void
        RoutingProtocol::SendHello() {
            NS_LOG_FUNCTION(this);
            aimf::MessageHeader msg;
            msg.SetVTime(AIMF_NEIGHB_HOLD_TIME);
            msg.SetOriginatorAddress(m_mainAddress);
            msg.SetTimeToLive(255);
            msg.SetMessageSequenceNumber(GetMessageSequenceNumber());
            MessageHeader::Hello &hello = msg.GetHello();
            hello.willingness = m_willingness;
            std::vector<aimf::MessageHeader::Hello::Association> &associations = hello.associations;
            // Add all local HMA associations to the HMA message
            const Associations &localHelloAssociations = m_state.GetAssociations();
            for (Associations::const_iterator it = localHelloAssociations.begin();
                    it != localHelloAssociations.end(); it++) {
                aimf::MessageHeader::Hello::Association assoc = {it->group, it->source, it->will};
                associations.push_back(assoc);
            }
            NS_LOG_DEBUG("AIMF HELLO message size: " << int (msg.GetSerializedSize()));
            SendMessage(msg);
        }
        void
        RoutingProtocol::SendPacket(Ptr<Packet> packet) {
            NS_LOG_DEBUG("AIMF node " << m_mainAddress << " sending a AIMF packet");
            // Add a header
            aimf::PacketHeader header;
            header.SetPacketLength(header.GetSerializedSize() + packet->GetSize());
            header.SetPacketSequenceNumber(GetPacketSequenceNumber());
            packet->AddHeader(header);
            // Send it
            for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator i =
                    m_socketAddresses.begin(); i != m_socketAddresses.end(); i++) {
                Ipv4Address mcast = Ipv4Address(AIMF_MCAST_ADR);
                NS_LOG_DEBUG("Using socket with  " << i->second.GetLocal() << " as src address.");
                i->first->SendTo(packet, 0, InetSocketAddress(mcast, AIMF_PORT_NUMBER));
                Ptr<Packet> packetCopy;
                packetCopy = packet->Copy();
                m_txHelloPacketTrace(packetCopy, m_ipv4, i->first->GetBoundNetDevice()->GetIfIndex());
            }
        }
        uint16_t GetMessageSequenceNumber() {
            m_messageSequenceNumber = (m_messageSequenceNumber + 1) % (AIMF_MAX_SEQ_NUM + 1);
            return m_messageSequenceNumber;
        }
        void
        RoutingProtocol::SendMessage(const MessageHeader &message) {
            Ptr<Packet> packet = Create<Packet> ();
            NS_LOG_DEBUG("Aimf node " << m_mainAddress << ": SendMessage");
            Ptr<Packet> p = Create<Packet> ();
            (*p).AddHeader(message);
            packet->AddAtEnd(p);
            SendPacket(packet);
        }
        void
        RoutingProtocol::AddHostMulticastAssociation(Ipv4Address group, Ipv4Address source) {
            // Check if the (group, source) tuple already exist
            // in the list of local HMA associations. IGMP was responsible for the function call.
            const Associations &localHnaAssociations = m_state.GetAssociations();
            for (Associations::const_iterator assocIterator = localHnaAssociations.begin();
                    assocIterator != localHnaAssociations.end(); assocIterator++) {
                Association const &localHnaAssoc = *assocIterator;
                if (localHnaAssoc.group == group && localHnaAssoc.source == source) {
                    NS_LOG_INFO("HMA association for multicast group (" << group << "," << source << ") already exists.");
                    return;
                }
            }
            // If the tuple does not already exist, add it to the list of local HMA associations.
            NS_LOG_INFO("Adding HMA association for multicast group (" << group << "," << source << ").");
            uint8_t k = 64;
            m_state.InsertAssociation((Association) {
                group, source, m_mainAddress, k
            });
            RoutingTableComputation();
        }
        void RoutingProtocol::RemoveHostMulticastAssociation(Ipv4Address group, Ipv4Address source) {
            m_state.EraseAssociation((Association) {
                group, source
            });
            RoutingTableComputation();
            m_state.EraseTimer(group);
        }
        void
        RoutingProtocol::HelloTimerExpire() {
            SendHello();
            m_helloTimer.Schedule(m_helloInterval);
        }
        void RoutingProtocol::OlsrTimerExpire() {
            double t = 0;
            uint8_t j = 0;
            std::vector<olsr::RoutingTableEntry> v = m_olsr_onNode->GetRoutingTableEntries();
            switch (m_willingness) {
                case AIMF_WILL_ALWAYS:
                    forward = true;
                    break;
                case 1:
                    t++;
                case 2:
                    t++;
                case AIMF_WILL_DEFAULT:
                    t++;
                case 4:
                    t++;
                case 5:
                    t++;
                case AIMF_WILL_HIGH:
                    m_olsrCheck.Schedule(m_olsrCheckInterval + Time(Seconds(t)));
                    for (std::vector<NeighborTuple>::iterator neig = m_state.GetNeighbors().begin(); neig != m_state.GetNeighbors().end(); neig++) {
                        for (std::vector<olsr::RoutingTableEntry>::iterator route = v.begin(); route != v.end(); route++) {
                            if (route->destAddr == neig->neighborMainAddr) {
                                if (neig->willingness >= j) {
                                    j = neig->willingness;
                                }
                            }
                        }
                    }
                    if (j <= m_willingness) {
                        forward = true; //ALERT PIM
                    } else {
                        forward = false; //ALERT PIM
                    }
                    break;
                case AIMF_WILL_NEVER:
                    forward = false;
                    break;
            }
        }
    }
}
