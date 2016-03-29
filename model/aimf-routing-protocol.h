/* 
 * File:   aIMF.h
 * Author: debonatis
 *
 * Created on 19 February 2016, 12:59
 */

#ifndef AIMF_H
#define	AIMF_H
#define AIMF_C 0.0625

#include "ns3/test.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/event-garbage-collector.h"
#include "ns3/random-variable-stream.h"
#include "ns3/timer.h"
#include "ns3/traced-callback.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/ipv4-address.h"
#include "aimf-header.h"
#include "aimf-state.h"
#include "aimf-repository.h"

#include <vector>
#include <map>


namespace ns3 {
    namespace aimf {

        class RoutingProtocol;


        ///
        /// \ingroup aimf
        ///
        /// \brief AIMF routing protocol for IPv4
        ///

        class RoutingProtocol : public Ipv4RoutingProtocol {
        public:


            RoutingProtocol();
            virtual ~RoutingProtocol();



            static TypeId GetTypeId(void);

            //            /**
            //             * Return the list of routing table entries discovered by AIMF
            //             **/
            std::vector<Ipv4MulticastRoutingTableEntry> GetRoutingTableEntries() const;
            //
            //            /**
            //             * Assign a fixed random variable stream number to the random variables
            //             * used by this model.  Return the number of streams (possibly zero) that
            //             * have been assigned.
            //             *
            //             * \param stream first stream index to use
            //             * \return the number of stream indices assigned by this model
            //             */
            int64_t AssignStreams(int64_t stream);
            //
            //            /**
            //             * TracedCallback signature for Packet transmit and receive events.
            //             *
            //             * \param [in] header
            //             * \param [in] messages
            //             */
            typedef void (* PacketTxRxTracedCallback)
            (const PacketHeader & header, const MessageList & messages);
            //
            //            /**
            //             * TracedCallback signature for routing table computation.
            //             *
            //             * \param [in] size Final routing table size.
            //             */
            typedef void (* TableChangeTracedCallback) (uint32_t size);

            //
        private:
            std::set<uint32_t> m_interfaceExclusions;
            Ptr<Ipv4StaticRouting> m_routingTableAssociation;
            //
        public:

            //            /// Willingness for forwarding packets on behalf of other nodes.
            uint8_t m_willingness;

            std::set<uint32_t> GetInterfaceExclusions() const {
                return m_interfaceExclusions;
            }
            void SetInterfaceExclusions(std::set<uint32_t> exceptions);

            /// Inject Association to be sent in HNA message
            void AddHostNetworkAssociation(Ipv4Address group, Ipv4Address source);
            /// Removes Association sent in HNA message
            void RemoveHostNetworkAssociation(Ipv4Address group, Ipv4Address source);

            /// Inject Associations from an Ipv4StaticRouting instance
            void SetRoutingTableAssociation(Ptr<Ipv4StaticRouting> routingTable);

            /**
             * \brief Returns the internal HNA table
             * \returns the internal HNA table
             */
            Ptr<const Ipv4StaticRouting> GetRoutingTableAssociation() const;

        protected:
            virtual void DoInitialize(void);
        private:
            std::map<Ipv4Address, Ipv4MulticastRoutingTableEntry> m_table; ///< Data structure for the routing table.
            //

            //
            EventGarbageCollector m_events;
            //
            //            /// Packets sequence number counter.
            uint16_t m_packetSequenceNumber;
            //            /// Messages sequence number counter.
            uint16_t m_messageSequenceNumber;
            //            /// Advertised Neighbor Set sequence number.
            uint16_t m_ansn;

            //
            //            /// HELLO messages' emission interval.
            Time m_helloInterval;


            //
            //            /// Internal state with all needed data structs.
            AimfState m_state;
            //
            Ptr<Ipv4> m_ipv4;
            //
            void Clear(); //ok
            //
            //            uint32_t GetSize() const {
            //                return m_table.size();
            //            }
            void RemoveEntry(const Ipv4Address &dest); //ok
            void AddEntry(const Ipv4Address &dgroup,
                    const Ipv4Address &source,
                    uint32_t inputInterface,
                    std::vector<uint32_t> outputInterfaces);



        protected:
            virtual void DoDispose(void);

        private:
            /// Container for the network routes
            typedef std::list<std::pair <Ipv4RoutingTableEntry *, uint32_t> > NetworkRoutes;

            /// Const Iterator for container for the network routes
            typedef std::list<std::pair <Ipv4RoutingTableEntry *, uint32_t> >::const_iterator NetworkRoutesCI;

            /// Iterator for container for the network routes
            typedef std::list<std::pair <Ipv4RoutingTableEntry *, uint32_t> >::iterator NetworkRoutesI;

            /**
             * \brief Lookup in the forwarding table for destination.
             * \param dest destination address
             * \param oif output interface if any (put 0 otherwise)
             * \return Ipv4Route to route the packet to reach dest address
             */
            Ptr<Ipv4Route> LookupStatic(Ipv4Address dest, Ptr<NetDevice> oif = 0);

            /**
             * \brief Lookup in the multicast forwarding table for destination.
             * \param origin source address
             * \param group group multicast address
             * \param interface interface index
             * \return Ipv4MulticastRoute to route the packet to reach dest address
             */
            Ptr<Ipv4MulticastRoute> LookupStatic(Ipv4Address origin, Ipv4Address group,
                    uint32_t interface);

            /**
             * \brief Choose the source address to use with destination address.
             * \param interface interface index
             * \param dest IPv4 destination address
             * \return IPv4 source address to use
             */
            Ipv4Address SourceAddressSelection(uint32_t interface, Ipv4Address dest);
        public:
            /**
             * \brief the forwarding table for network.
             */
            NetworkRoutes m_networkRoutes;

            /**
             * \brief the forwarding table for multicast.
             */


            bool FindSendEntry(const Ipv4MulticastRoutingTableEntry &entry,
                    Ipv4MulticastRoutingTableEntry & outEntry) const;
            //
            //            // From Ipv4RoutingProtocol
            virtual Ptr<Ipv4Route> RouteOutput(Ptr<Packet> p,
                    const Ipv4Header &header,
                    Ptr<NetDevice> oif,
                    Socket::SocketErrno & sockerr);
            virtual bool RouteInput(Ptr<const Packet> p,
                    const Ipv4Header &header,
                    Ptr<const NetDevice> idev,
                    UnicastForwardCallback ucb,
                    MulticastForwardCallback mcb,
                    LocalDeliverCallback lcb,
                    ErrorCallback ecb);
            virtual void NotifyInterfaceUp(uint32_t interface);
            virtual void NotifyInterfaceDown(uint32_t interface);
            virtual void NotifyAddAddress(uint32_t interface, Ipv4InterfaceAddress address);
            virtual void NotifyRemoveAddress(uint32_t interface, Ipv4InterfaceAddress address);
            virtual void SetIpv4(Ptr<Ipv4> ipv4); //ok
            virtual void PrintRoutingTable(Ptr<OutputStreamWrapper> stream) const; //ok
            void AssociationTupleTimerExpire(Ipv4Address advertiser, Ipv4Address group, Ipv4Address source);



            void SendPacket(Ptr<Packet> packet, const MessageList & containedMessages); //ok

            /// Increments packet sequence number and returns the new value.
            inline uint16_t GetPacketSequenceNumber(); //ok
            /// Increments message sequence number and returns the new value.
            inline uint16_t GetMessageSequenceNumber(); //ok

            void RecvAimf(Ptr<Socket> socket); //ok


            void RoutingTableComputation(); //ok
            Ipv4Address GetMainAddress(Ipv4Address iface_addr) const;
            //            bool UsesNonAimfOutgoingInterface(const Ipv4RoutingTableEntry &route);
            //
            //            // Timer handlers
            Timer m_helloTimer;
            void HelloTimerExpire();
            //

            //
            //            void DupTupleTimerExpire(Ipv4Address address, uint16_t sequenceNumber);
            bool m_linkTupleTimerFirstTime;
            //            void LinkTupleTimerExpire(Ipv4Address neighborIfaceAddr);
            //            void Nb2hopTupleTimerExpire(Ipv4Address neighborMainAddr, Ipv4Address twoHopNeighborAddr);
            //            void MprSelTupleTimerExpire(Ipv4Address mainAddr);
            //            void TopologyTupleTimerExpire(Ipv4Address destAddr, Ipv4Address lastAddr);
            //            void IfaceAssocTupleTimerExpire(Ipv4Address ifaceAddr);
            //            void AssociationTupleTimerExpire(Ipv4Address gatewayAddr, Ipv4Address networkAddr, Ipv4Mask netmask);
            //
            //            void IncrementAnsn();
            //
            //            /// A list of pending messages which are buffered awaiting for being sent.
            aimf::MessageList m_queuedMessages;
            Timer m_queuedMessagesTimer; // timer for throttling outgoing messages
            //
            //            void ForwardDefault(aimf::MessageHeader aimfMessage,
            //                    DuplicateTuple *duplicated,
            //                    const Ipv4Address &localIface,
            //                    const Ipv4Address &senderAddress);
            void QueueMessage(const aimf::MessageHeader &message, Time delay); //ok
            void SendQueuedMessages(); //ok
            void SendHello(); //ok
            void AddAssociationTuple(const AssociationTuple &tuple);
            void RemoveAssociationTuple(const AssociationTuple &tuple);
            void AddIfaceAssocTuple(const IfaceAssocTuple &tuple);




            void ProcessHello(const aimf::MessageHeader &msg,
                    const Ipv4Address &receiverIface,
                    const Ipv4Address & senderIface); //ok

            void PopulateNeighborSet(const aimf::MessageHeader &msg,
                    const aimf::MessageHeader::Hello & hello);
            /// Check that address is one of my interfaces
            bool IsMyOwnAddress(const Ipv4Address & a) const;

            Ipv4Address m_mainAddress;


            std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;

            TracedCallback <const PacketHeader &,
            const MessageList &> m_rxPacketTrace;
            TracedCallback <const PacketHeader &,
            const MessageList &> m_txPacketTrace;
            TracedCallback <uint32_t> m_routingTableChanged;

            /// Provides uniform random variables.
            Ptr<UniformRandomVariable> m_uniformRandomVariable;

        };

    }
} // namespace ns3


#endif	/* AIMF_H */

