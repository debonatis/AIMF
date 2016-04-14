/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef AIMFHELPER_H
#define AIMFHELPER_H

#include "ns3/object-factory.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ipv4-routing-helper.h"
#include <map>
#include <set>


namespace ns3 {

    class AimfHelper : public Ipv4RoutingHelper {
    public:
        /**
         * Create an AimfHelper that makes life easier for people who want to install
         * AIMF routing to nodes.
         */
        AimfHelper();

        /**
         * \brief Construct an AimfHelper from another previously initialized instance
         * (Copy Constructor).
         */
        AimfHelper(const AimfHelper &);

        /**
         * \returns pointer to clone of this AimfHelper 
         * 
         * This method is mainly for internal use by the other helpers;
         * clients are expected to free the dynamic memory allocated by this method
         */
        AimfHelper* Copy(void) const;

        /**
         * \param node the node for which an exception is to be defined
         * \param interface an interface of node on which AIMF is not to be installed
         *
         * This method allows the user to specify an interface on which AIMF is not to be installed on
         */
        void ExcludeInterface(Ptr<Node> node, uint32_t interface);

        void SetListenNetDevice(Ptr<Node> node, uint32_t interface);

        /**
         * \param node the node on which the routing protocol will run
         * \returns a newly-created routing protocol
         *
         * This method will be called by ns3::InternetStackHelper::Install
         */
        virtual Ptr<Ipv4RoutingProtocol> Create(Ptr<Node> node) const;

        /**
         * \param name the name of the attribute to set
         * \param value the value of the attribute to set.
         *
         * This method controls the attributes of ns3::aimf::RoutingProtocol
         */
        void Set(std::string name, const AttributeValue &value);

        /**
         * Assign a fixed random variable stream number to the random variables
         * used by this model.  Return the number of streams (possibly zero) that
         * have been assigned.  The Install() method of the InternetStackHelper
         * should have previously been called by the user.
         *
         * \param stream first stream index to use
         * \param c NodeContainer of the set of nodes for which the AimfRoutingProtocol
         *          should be modified to use a fixed stream
         * \return the number of stream indices assigned by this helper
         */
        int64_t AssignStreams(NodeContainer c, int64_t stream);

    private:
        /**
         * \brief Assignment operator declared private and not implemented to disallow
         * assignment and prevent the compiler from happily inserting its own.
         */
        AimfHelper &operator=(const AimfHelper &);
        ObjectFactory m_agentFactory; //!< Object factory

        std::map< Ptr<Node>, std::set<uint32_t> > m_interfaceExclusions; //!< container of interfaces excluded from AIMF operations
        std::map< Ptr<Node>, std::set<uint32_t> > m_netdevices;
    };

}

#endif	/* AIMFHELPER_H */

