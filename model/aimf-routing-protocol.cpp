/* -*-
 * File:   aimf-routing-protocol.cpp
 * Author: debonatis
 * 
 * Created on 19 February 2016, 12:59
 */
#define NS_LOG_APPEND_CONTEXT                                   \
  if (GetObject<Node> ()) { std::clog << "[node " << GetObject<Node> ()->GetId () << "] "; }
#include "aimf-routing-protocol.h"
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

/********** Useful macros **********/

///
/// \brief Gets the delay between a given time and the current time.
///
/// If given time is previous to the current one, then this macro returns
/// a number close to 0. This is used for scheduling events at a certain moment.
///
#define DELAY(time) (((time) < (Simulator::Now ())) ? Seconds (0.000001) : \
                     (time - Simulator::Now () + Seconds (0.000001)))



///
/// \brief Period at which a node must cite every link and every neighbor.
///
/// We only use this value in order to define AIMF_NEIGHB_HOLD_TIME.
///
#define AIMF_REFRESH_INTERVAL   m_helloInterval


/********** Holding times **********/

/// Neighbor holding time.
#define AIMF_NEIGHB_HOLD_TIME   Time (3 * AIMF_REFRESH_INTERVAL)
/// HMA holding time.
#define AIMF_HNA_HOLD_TIME      Time (1 * m_hnaInterval)

/********** Link types **********/

/// Unspecified link type.
#define AIMF_UNSPEC_LINK        0
/// Asymmetric link type.
#define AIMF_ASYM_LINK          1
/// Symmetric link type.
#define AIMF_SYM_LINK           2
/// Lost link type.
#define AIMF_LOST_LINK          3

/********** Neighbor types **********/

/// Not neighbor type.
#define AIMF_NOT_NEIGH          0
/// Symmetric neighbor type.
#define AIMF_SYM_NEIGH          1
/// Asymmetric neighbor type.
#define AIMF_MPR_NEIGH          2


/********** Willingness **********/

/// Willingness for forwarding packets from other nodes: never.
#define AIMF_WILL_NEVER         0
/// Willingness for forwarding packets from other nodes: low.
#define AIMF_WILL_LOW           1
/// Willingness for forwarding packets from other nodes: medium.
#define AIMF_WILL_DEFAULT       3
/// Willingness for forwarding packets from other nodes: high.
#define AIMF_WILL_HIGH          6
/// Willingness for forwarding packets from other nodes: always.
#define AIMF_WILL_ALWAYS        7

#define AIMF_MCAST_ADR "224.0.0.12"
/********** Miscellaneous constants **********/

/// Maximum allowed jitter.
#define AIMF_MAXJITTER          (m_helloInterval.GetSeconds () / 4)
/// Maximum allowed sequence number.
#define AIMF_MAX_SEQ_NUM        65535
/// Random number between [0-AIMF_MAXJITTER] used to jitter AIMF packet transmission.
#define JITTER (Seconds (m_uniformRandomVariable->GetValue (0, AIMF_MAXJITTER)))


#define AIMF_PORT_NUMBER 1337
/// Maximum number of messages per packet.
#define AIMF_MAX_MSGS           64

/// Maximum number of hellos per message (4 possible link types * 3 possible nb types).
#define AIMF_MAX_HELLOS         12

/// Maximum number of addresses advertised on a message.
#define AIMF_MAX_ADDRS          64

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
                    .AddAttribute("Willingness", "Willingness of a node to carry and forward traffic compared to other similar nodes.",
                    EnumValue(AIMF_WILL_DEFAULT),
                    MakeEnumAccessor(&RoutingProtocol::m_willingness),
                    MakeEnumChecker(AIMF_WILL_NEVER, "never",
                    AIMF_WILL_LOW, "low",
                    AIMF_WILL_DEFAULT, "default",
                    AIMF_WILL_HIGH, "high",
                    AIMF_WILL_ALWAYS, "always"))
                    .AddTraceSource("Rx", "Receive AIMF packet.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_rxPacketTrace),
                    "ns3::aimf::RoutingProtocol::PacketTxRxTracedCallback")
                    .AddTraceSource("Tx", "Send AIMF packet.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_txPacketTrace),
                    "ns3::aimf::RoutingProtocol::PacketTxRxTracedCallback")
                    .AddTraceSource("RoutingTableChanged", "The AIMF routing table has changed.",
                    MakeTraceSourceAccessor(&RoutingProtocol::m_routingTableChanged),
                    "ns3::aimf::RoutingProtocol::TableChangeTracedCallback")
                    ;
            return tid;
        }

        RoutingProtocol::RoutingProtocol() :
        m_routingTableAssociation(0),
        m_ipv4(0),
        m_helloTimer(Timer::CANCEL_ON_DESTROY),
        m_queuedMessagesTimer(Timer::CANCEL_ON_DESTROY) {
            m_uniformRandomVariable = CreateObject<UniformRandomVariable> ();



        }

        RoutingProtocol::~RoutingProtocol() {
        };


        std::map<ns3::Ipv4Address, Ipv4MulticastRoutingTableEntry> m_table; ///< Data structure for the routing table.

        ns3::Ptr<ns3::Ipv4StaticRouting> m_RoutingTable;

        ns3::EventGarbageCollector m_events;

        /// Packets sequence number counter.
        uint16_t m_packetSequenceNumber;
        /// Messages sequence number counter.
        uint16_t m_messageSequenceNumber;
        /// Advertised Neighbor Set sequence number.
        uint16_t m_ansn;

        /// HELLO messages' emission interval.
        Time m_helloInterval;
        /// TC messages' emission interval.
        /// HNA messages' emission interval.
        Time m_hnaInterval;
        /// Willingness for forwarding packets on behalf of other nodes.
        uint8_t m_willingness;

        /// Internal state with all needed data structs.

        ns3::Ptr<ns3::Ipv4> m_ipv4;

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

            InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom(sourceAddress);
            Ipv4Address senderIfaceAddr = inetSourceAddr.GetIpv4();
            Ipv4Address receiverIfaceAddr = m_socketAddresses[socket].GetLocal();
            NS_ASSERT(receiverIfaceAddr != Ipv4Address());
            NS_LOG_DEBUG("AIMF node " << m_mainAddress << " received a AIMF packet from "
                    << senderIfaceAddr << " to " << receiverIfaceAddr);

            // All routing messages are sent from and to port RT_PORT,
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

            m_rxPacketTrace(aimfPacketHeader, messages);

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

                // If the message has been processed it must not be processed again



                // Get main address of the peer, which may be different from the packet source address
                //       const IfaceAssocTuple *ifaceAssoc = m_state.FindIfaceAssocTuple (inetSourceAddr.GetIpv4 ());
                //       Ipv4Address peerMainAddress;
                //       if (ifaceAssoc != NULL)
                //         {
                //           peerMainAddress = ifaceAssoc->mainAddr;
                //         }
                //       else
                //         {
                //           peerMainAddress = inetSourceAddr.GetIpv4 () ;
                //         }


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

        Ptr<Ipv4Route>
        RoutingProtocol::LookupStatic(Ipv4Address dest, Ptr<NetDevice> oif) {
            NS_LOG_FUNCTION(this << dest << " " << oif);
            Ptr<Ipv4Route> rtentry = 0;
            uint16_t longest_mask = 0;
            uint32_t shortest_metric = 0xffffffff;
            /* when sending on local multicast, there have to be interface specified */
            if (dest.IsLocalMulticast()) {
                NS_ASSERT_MSG(oif, "Try to send on link-local multicast address, and no interface index is given!");

                rtentry = Create<Ipv4Route> ();
                rtentry->SetDestination(dest);
                rtentry->SetGateway(Ipv4Address::GetZero());
                rtentry->SetOutputDevice(oif);
                rtentry->SetSource(m_ipv4->GetAddress(oif->GetIfIndex(), 0).GetLocal());
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
                uint32_t interface) {
            NS_LOG_FUNCTION(this << origin << " " << group << " " << interface);
            Ptr<Ipv4MulticastRoute> mrtentry = 0;

            if (!m_state.WillingnessOk(m_willingness))return mrtentry;
            NS_LOG_DEBUG("Node " << m_mainAddress << ": willingness = " << int(m_willingness) << ". ");

            for (std::map<Ipv4Address, Ipv4MulticastRoutingTableEntry>::const_iterator i = m_table.begin();
                    i != m_table.end();
                    i++) {
                Ipv4MulticastRoutingTableEntry route = i->second;
                //
                // We've been passed an origin address, a multicast group address and an 
                // interface index.  We have to decide if the current route in the list is
                // a match.
                //
                // The first case is the restrictive case where the origin, group and index
                // matches.
                //
                if (origin == route.GetOrigin() && group == route.GetGroup()) {
                    // Skipping this case (SSM) for now
                    NS_LOG_LOGIC("Found multicast source specific route" << i->first);
                }
                if (group == route.GetGroup() && group == i->first) {
                    if (interface == Ipv4::IF_ANY ||
                            interface == route.GetInputInterface()) {
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
            // no way to determine the scope of the destination, so adopt the
            // following rule:  pick the first available address (index 0) unless
            // a subsequent address is on link (in which case, pick the primary
            // address if there are multiple)
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
            m_queuedMessagesTimer.SetFunction(&RoutingProtocol::SendQueuedMessages, this);

            m_packetSequenceNumber = AIMF_MAX_SEQ_NUM;
            m_messageSequenceNumber = AIMF_MAX_SEQ_NUM;
            m_ansn = AIMF_MAX_SEQ_NUM;

            m_linkTupleTimerFirstTime = true;

            m_ipv4 = ipv4;


        }

        void RoutingProtocol::DoDispose() {
            m_ipv4 = 0;

            m_routingTableAssociation = 0;

            for (std::map< Ptr<Socket>, Ipv4InterfaceAddress >::iterator iter = m_socketAddresses.begin();
                    iter != m_socketAddresses.end(); iter++) {
                iter->first->Close();
            }
            m_socketAddresses.clear();

            Ipv4RoutingProtocol::DoDispose();
        }

        Ptr<Ipv4Route>
        RoutingProtocol::RouteOutput(Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno & sockerr) {
            Ptr<Ipv4Route> rtentry = 0;

            // Multicast goes here
            if (header.GetDestination().IsMulticast()) {
                // Note:  Multicast routes for outbound packets are stored in the
                // normal unicast table.  An implication of this is that it is not
                // possible to source multicast datagrams on multiple interfaces.
                // This is a well-known property of sockets implementation on 
                // many Unix variants.
                // So, we just log it and fall through to LookupStatic ()
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
                        header.GetDestination(), m_ipv4->GetInterfaceForDevice(idev));

                if (mrtentry) {

                    NS_LOG_LOGIC("Multicast rute ok");
                    mcb(mrtentry, p, header);
                    NS_LOG_DEBUG("Packet routed with destination: " << header.GetDestination() << " and source: " << header.GetSource() << " . It has a TTl of " << int (header.GetTtl()));
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


        ///
        /// \brief Adds a new entry into the routing table.
        ///
        /// If an entry for the given destination existed, it is deleted and freed.
        ///
        /// \param dest address of the destination node.
        /// \param next address of the next hop node.
        /// \param iface address of the local interface.
        /// \param dist distance to the destination node.
        ///

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
            m_uniformRandomVariable->SetStream(stream);
            return 1;
        }

        void
        RoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const {
            std::ostream* os = stream->GetStream();
            *os << "Destination\t\tNextHop\t\tInterface\tDistance\n";

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

        void RoutingProtocol::DoInitialize() {
            Ipv4Address loopback("127.0.0.1");
            if (m_mainAddress == Ipv4Address()) {

                for (uint32_t i = 0; i < m_ipv4->GetNInterfaces(); i++) {
                    // Use primary address, if multiple
                    Ipv4Address addr = m_ipv4->GetAddress(i, 0).GetLocal();
                    if (addr != loopback) {
                        m_mainAddress = addr;
                        break;
                    }
                }

                NS_ASSERT(m_mainAddress != Ipv4Address());
            }

            NS_LOG_DEBUG("Starting AIMF on node " << m_mainAddress);



            bool canRunAimf = false;
            for (uint32_t i = 0; i < (m_ipv4->GetNInterfaces()); i++) {
                Ipv4Address addr = m_ipv4->GetAddress(i, 0).GetLocal();
                if (addr == loopback)
                    continue;

                if (addr != m_mainAddress) {
                    // Create never expiring interface association tuple entries for our
                    // own network interfaces, so that GetMainAddress () works to
                    // translate the node's own interface addresses into the main address.
                    IfaceAssocTuple tuple;
                    tuple.ifaceAddr = addr;
                    tuple.mainAddr = m_mainAddress;
                    AddIfaceAssocTuple(tuple);
                    NS_ASSERT(GetMainAddress(addr) == m_mainAddress);
                }

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

                canRunAimf = true;
            }

            if (canRunAimf) {
                HelloTimerExpire();


                NS_LOG_DEBUG("AIMF on node " << m_mainAddress << " started");
            }
        }

        Ipv4Address
        RoutingProtocol::GetMainAddress(Ipv4Address iface_addr) const {
            const IfaceAssocTuple *tuple =
                    m_state.FindIfaceAssocTuple(iface_addr);

            if (tuple != NULL)
                return tuple->mainAddr;
            else
                return iface_addr;
        }

        void
        RoutingProtocol::AddIfaceAssocTuple(const IfaceAssocTuple &tuple) {
            //   debug("%f: Node %d adds iface association tuple: main_addr = %d iface_addr = %d\n",
            //         Simulator::Now (),
            //         OLSR::node_id(ra_addr()),
            //         OLSR::node_id(tuple->main_addr()),
            //         OLSR::node_id(tuple->iface_addr()));

            m_state.InsertIfaceAssocTuple(tuple);
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
                        now + msg.GetVTime()
                    };
                    NeighborTuple nb_tuple = {msg.GetOriginatorAddress()
                        , nb_tuple.willingness = msg.GetHello().willingness};
                    AddAssociationTuple(assocTuple);

                    //Schedule Association Tuple deletion
                    Simulator::Schedule(DELAY(assocTuple.expirationTime),
                            &RoutingProtocol::AssociationTupleTimerExpire, this,
                            assocTuple.advertiser, assocTuple.group, assocTuple.source);

                }

            }
#endif // NS3_LOG_ENABLE

            PopulateNeighborSet(msg, hello);





        }

        void RoutingProtocol::SetInterfaceExclusions(std::set<uint32_t> exceptions) {
            m_interfaceExclusions = exceptions;
        }

        void
        RoutingProtocol::AddNeigbour(NeighborTuple tuple) {

            m_state.InsertNeighborTuple(tuple);
            
        }

        void
        RoutingProtocol::PopulateNeighborSet(const aimf::MessageHeader &msg,
                const aimf::MessageHeader::Hello & hello) {
            NeighborTuple *nb_tuple = m_state.FindNeighborTuple(msg.GetOriginatorAddress());
            if (nb_tuple != NULL) {
                nb_tuple->willingness = hello.willingness;
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

        ///
        /// \brief Looks up an entry for the specified destination address.
        /// \param dest destination address.
        /// \param outEntry output parameter to hold the routing entry result, if fuond
        /// \return true if found, false if not found
        ///

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

            // 1. All the entries from the routing table are removed.
            Clear();

            std::vector<uint32_t> outint;
            outint.push_back(2);
            const Associations &localHmaAssociations = m_state.GetAssociations();
            for (Associations::const_iterator assocIterator = localHmaAssociations.begin();
                    assocIterator != localHmaAssociations.end(); assocIterator++) {
                Association const &localHmaAssoc = *assocIterator;
                AddEntry(localHmaAssoc.group, localHmaAssoc.source, 1, outint);
                NS_LOG_DEBUG("Node " << m_mainAddress << ": Adding local (" << localHmaAssoc.group << "," << localHmaAssoc.source << ") pair to routing table. ");
            }

            outint.clear();
            outint.push_back(2);
            const AssociationSet &localHmaAssociationSets = m_state.GetAssociationSet();
            for (AssociationSet::const_iterator assocSetIterator = localHmaAssociationSets.begin();
                    assocSetIterator != localHmaAssociationSets.end(); assocSetIterator++) {
                AssociationTuple const &localHmaAssocSet = *assocSetIterator;
                AddEntry(localHmaAssocSet.group, localHmaAssocSet.source, this->m_ipv4->GetInterfaceForPrefix(localHmaAssocSet.advertiser, "255.255.255.0"), outint);
                NS_LOG_DEBUG("Node " << m_mainAddress << ": Adding adjacent (" << localHmaAssocSet.group << "," << localHmaAssocSet.source << ") pair to routing table. ");
            }



            NS_LOG_DEBUG("Node " << m_mainAddress << ": RoutingTableComputation end.");

        }

        void
        RoutingProtocol::SendHello() {
            NS_LOG_FUNCTION(this);

            aimf::MessageHeader msg;


            msg.SetVTime(AIMF_NEIGHB_HOLD_TIME);
            msg.SetOriginatorAddress(m_mainAddress);
            msg.SetTimeToLive(255);
            msg.SetHopCount(0);
            msg.SetMessageSequenceNumber(GetMessageSequenceNumber());
            MessageHeader::Hello &hello = msg.GetHello();


            hello.willingness = m_willingness;

            std::vector<aimf::MessageHeader::Hello::Association> &associations = hello.associations;

            // Add all local HMA associations to the HMA message
            const Associations &localHelloAssociations = m_state.GetAssociations();
            for (Associations::const_iterator it = localHelloAssociations.begin();
                    it != localHelloAssociations.end(); it++) {
                aimf::MessageHeader::Hello::Association assoc = {it->group, it->source};
                associations.push_back(assoc);
            }
            // If there is no HMA associations to send, return without queuing the message
            if (associations.size() == 0) {
                return;
            }



            NS_LOG_DEBUG("AIMF HELLO message size: " << int (msg.GetSerializedSize()));
            QueueMessage(msg, JITTER);
        }

        void
        RoutingProtocol::QueueMessage(const MessageHeader &message, Time delay) {
            m_queuedMessages.push_back(message);
            if (not m_queuedMessagesTimer.IsRunning()) {
                m_queuedMessagesTimer.SetDelay(delay);
                m_queuedMessagesTimer.Schedule();
            }
        }

        void
        RoutingProtocol::SendPacket(Ptr<Packet> packet,
                const MessageList & containedMessages) {
            NS_LOG_DEBUG("AIMF node " << m_mainAddress << " sending a AIMF packet");

            // Add a header
            aimf::PacketHeader header;
            header.SetPacketLength(header.GetSerializedSize() + packet->GetSize());
            header.SetPacketSequenceNumber(GetPacketSequenceNumber());
            packet->AddHeader(header);

            // Trace it
            m_txPacketTrace(header, containedMessages);

            // Send it
            for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator i =
                    m_socketAddresses.begin(); i != m_socketAddresses.end(); i++) {
                Ipv4Address mcast = Ipv4Address(AIMF_MCAST_ADR);
                i->first->SendTo(packet, 0, InetSocketAddress(mcast, AIMF_PORT_NUMBER));
            }
        }

        uint16_t GetMessageSequenceNumber() {
            m_messageSequenceNumber = (m_messageSequenceNumber + 1) % (AIMF_MAX_SEQ_NUM + 1);
            return m_messageSequenceNumber;
        }

        void
        RoutingProtocol::SendQueuedMessages() {
            Ptr<Packet> packet = Create<Packet> ();
            int numMessages = 0;

            NS_LOG_DEBUG("Aimf node " << m_mainAddress << ": SendQueuedMessages");

            MessageList msglist;

            for (std::vector<MessageHeader>::const_iterator message = m_queuedMessages.begin();
                    message != m_queuedMessages.end();
                    message++) {
                Ptr<Packet> p = Create<Packet> ();
                (*p).AddHeader(*message);
                packet->AddAtEnd(p);
                msglist.push_back(*message);
                if (++numMessages == AIMF_MAX_MSGS) {
                    SendPacket(packet, msglist);
                    msglist.clear();
                    // Reset variables for next packet
                    numMessages = 0;
                    packet = Create<Packet> ();
                }
            }

            if (packet->GetSize()) {
                SendPacket(packet, msglist);
            }

            m_queuedMessages.clear();
        }

        void
        RoutingProtocol::AddHostNetworkAssociation(Ipv4Address group, Ipv4Address source) {
            // Check if the (group, source) tuple already exist
            // in the list of local HMA associations
            const Associations &localHnaAssociations = m_state.GetAssociations();
            for (Associations::const_iterator assocIterator = localHnaAssociations.begin();
                    assocIterator != localHnaAssociations.end(); assocIterator++) {
                Association const &localHnaAssoc = *assocIterator;
                if (localHnaAssoc.group == group && localHnaAssoc.source == source) {
                    NS_LOG_INFO("HMA association for multicast group (" << group << "," << source << ") already exists.");
                    return;
                }
            }
            // If the tuple does not already exist, add it to the list of local HNA associations.
            NS_LOG_INFO("Adding HMA association for multicast group (" << group << "," << source << ").");

            m_state.InsertAssociation((Association) {
                group, source
            });
            RoutingTableComputation();
        }

        void
        RoutingProtocol::HelloTimerExpire() {
            SendHello();
            m_helloTimer.Schedule(m_helloInterval);
        }
    }
}