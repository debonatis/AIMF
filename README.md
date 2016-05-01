# AIMF
Multicast routing for edge MANET routers.


Model Description
*****************

The source code for the new module lives in the directory ``src/aimf``.

Aimf is trying to model a routing protocol that is embedded for wired to wireless multicast forwarding.
The main goal is to have only one forwarding instance at all times. It uses OLSR to see if other AIMF-neigbours are in range within the MANET.

Design
======

It has an object oriented style.  

Scope and Limitations
=====================

The model is for IPv4 only.  


* The use of multiple interfaces is supported but they have to be defined using AimfHelper;
* AIMF does not respond to the routing event notifications corresponding to dynamic interface up and down (``ns3::RoutingProtocol::NotifyInterfaceUp`` and ``ns3::RoutingProtocol::NotifyInterfaceDown``) or address insertion/removal ``ns3::RoutingProtocol::NotifyAddAddress`` and ``ns3::RoutingProtocol::NotifyRemoveAddress``).



Usage
*****

See examples in "src/aimf/examples".


Helpers
=======

A helper class for AIMF(AimfHelper) has been written.  After an IPv4 topology
has been created and unique IP addresses assigned to each node, the
simulation script writer can call one of three overloaded functions
with different scope to enable AIMF: ``ns3::AimfHelper::Install
(NodeContainer container)``; ``ns3::AimfHelper::AimfHelper (Ptr<Node>
node)``; or ``ns3::AimfHelper::InstallAll (void)``
It is mandatory that you exclude the wireless interface/s with ``ns3::AimfHelper::ExcludeInterface(Ptr<Node> node, uint32_t interface)`` and set the outgoing one/s with ``ns3::AimfHelper::SetMANETNetDeviceID(Ptr<Node> node, uint32_t interface)``. 

Attributes
==========

The method ``ns3::AimfHelper::Set ()`` can be used
to set AIMF attributes.  These include HelloInterval, olsrPollIntervall,
and Willingness.  

Output
======

There are tracers. See in "aimf-routing-protocol.cpp.

Advanced Usage
==============

To be able to inject routes and (S,G) pair/s (Simulating IGMP) you have to make yourselves a reference to the running aimf instance of choice.

     Ptr<Ipv4> stack = c.Get(3)->GetObject<Ipv4> ();
     Ptr<Ipv4RoutingProtocol> rp_Gw = (stack->GetRoutingProtocol());
     Ptr<Ipv4ListRouting> lrp_Gw = DynamicCast<Ipv4ListRouting> (rp_Gw); 
     Ptr<aimf::RoutingProtocol> aimf_Gw; 
     for (uint32_t i = 0; i < lrp_Gw->GetNRoutingProtocols(); i++) {
         int16_t priority; 
         Ptr<Ipv4RoutingProtocol> temp = lrp_Gw->GetRoutingProtocol(i, priority); 
        if (DynamicCast<aimf::RoutingProtocol> (temp)) {
             aimf_Gw = DynamicCast<aimf::RoutingProtocol>(temp);` 
        }

    }


Changing the willingness and injecting a (S,G) pair.

``Simulator::Schedule(Seconds(1.0), &aimf::RoutingProtocol::ChangeWillingness, aimf_Gw, 2);`` 
    
`` Simulator::Schedule(Seconds(3.0), &aimf::RoutingProtocol::AddHostMulticastAssociation, aimf_Gw, multicastGroup, multicastSource);``
    
Troubleshooting
===============

Be careful with the HelloInterval and the olsrPollInterval to avoid hysteria and get the interface selection right.



