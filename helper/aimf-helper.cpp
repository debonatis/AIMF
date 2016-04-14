#include "aimf-helper.h"
#include "ns3/aimf-routing-protocol.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/aimf-helper.h"

namespace ns3 {

    AimfHelper::AimfHelper() {
        m_agentFactory.SetTypeId("ns3::aimf::RoutingProtocol");
    }

    AimfHelper::AimfHelper(const AimfHelper &o)
    : m_agentFactory(o.m_agentFactory) {
        m_interfaceExclusions = o.m_interfaceExclusions;
        m_netdevices = o.m_netdevices;
    }

    AimfHelper*
    AimfHelper::Copy(void) const {
        return new AimfHelper(*this);
    }

    void
    AimfHelper::ExcludeInterface(Ptr<Node> node, uint32_t interface) {
        std::map< Ptr<Node>, std::set<uint32_t> >::iterator it = m_interfaceExclusions.find(node);

        if (it == m_interfaceExclusions.end()) {
            std::set<uint32_t> interfaces;
            interfaces.insert(interface);

            m_interfaceExclusions.insert(std::make_pair(node, std::set<uint32_t> (interfaces)));
        } else {
            it->second.insert(interface);
        }
    }

    void AimfHelper::SetListenNetDevice(Ptr<Node> node, uint32_t interface) {
        std::map<Ptr<Node>, std::set<uint32_t>  >::iterator it = m_netdevices.find(node);

        if (it == m_netdevices.end()) {
            std::set<uint32_t> interfaces;
            interfaces.insert(interface);
            
            m_netdevices.insert(std::make_pair(node, std::set<uint32_t> (interfaces)));
        } else{
            it->second.insert(interface);
        }
    }

    Ptr<Ipv4RoutingProtocol>
    AimfHelper::Create(Ptr<Node> node) const {
        Ptr<aimf::RoutingProtocol> agent = m_agentFactory.Create<aimf::RoutingProtocol> ();

        std::map<Ptr<Node>, std::set<uint32_t> >::const_iterator it = m_interfaceExclusions.find(node);
        std::map<Ptr<Node>, std::set<uint32_t> >::const_iterator it2 = m_netdevices.find(node);

        if (it != m_interfaceExclusions.end()) {
            agent->SetInterfaceExclusions(it->second);
        }

        if (it2 != m_netdevices.end()) {

            agent->SetNetdevicelistener(it2->second);
        }

        node->AggregateObject(agent);
        return agent;
    }

    void
    AimfHelper::Set(std::string name, const AttributeValue &value) {
        m_agentFactory.Set(name, value);
    }

    int64_t
    AimfHelper::AssignStreams(NodeContainer c, int64_t stream) {
        int64_t currentStream = stream;
        Ptr<Node> node;
        for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i) {
            node = (*i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
            NS_ASSERT_MSG(ipv4, "Ipv4 not installed on node");
            Ptr<Ipv4RoutingProtocol> proto = ipv4->GetRoutingProtocol();
            NS_ASSERT_MSG(proto, "Ipv4 routing not installed on node");
            Ptr<aimf::RoutingProtocol> aimf = DynamicCast<aimf::RoutingProtocol> (proto);
            if (aimf) {
                currentStream += aimf->AssignStreams(currentStream);
                continue;
            }
            // Aimf may also be in a list
            Ptr<Ipv4ListRouting> list = DynamicCast<Ipv4ListRouting> (proto);
            if (list) {
                int16_t priority;
                Ptr<Ipv4RoutingProtocol> listProto;
                Ptr<aimf::RoutingProtocol> listAimf;
                for (uint32_t i = 0; i < list->GetNRoutingProtocols(); i++) {
                    listProto = list->GetRoutingProtocol(i, priority);
                    listAimf = DynamicCast<aimf::RoutingProtocol> (listProto);
                    if (listAimf) {
                        currentStream += listAimf->AssignStreams(currentStream);
                        break;
                    }
                }
            }
        }
        return (currentStream - stream);

    }
}