/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//
//                         Lan1
//                     ================(cable or wireless)
//                      |    |   |   |
//       n0   n1   n2   n3   n4  n5  n6 
//       |    |    |    |    |
//       ======================
//           Lan0
//
// - Multicast source is at node n0;
// - Multicast forwarded by node n2 onto LAN1;
// - Nodes n0, n1, n2, n3, and n4 receive the multicast frame.
// - Node n4 listens for the data 

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/aimf-helper.h"
#include "ns3/aimf-routing-protocol.h"
#include "ns3/olsr-helper.h"
#include "ns3/olsr-routing-protocol.h"
#include "ns3/mobility-module.h"
#include "src/aimf/helper/aimf-helper.h"
#include "src/aimf/model/aimf-routing-protocol.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AimfMulticast");

int
main(int argc, char *argv[]) {
    //
    // Users may find it convenient to turn on explicit debugging
    // for selected modules; the below lines suggest how to do this
    //
    bool verbose = false;
    bool wifiok = true;
    LogComponentEnable("AimfMulticast", LOG_LEVEL_INFO);
    LogComponentEnable("AimfRoutingProtocol", LOG_LEVEL_INFO);
    // LogComponentEnable("OlsrRoutingProtocol", LOG_LEVEL_INFO);
    LogComponentEnable("AimfHeader", LOG_LEVEL_INFO);

    //
    // Set up default values for the simulation.
    //
    // Select DIX/Ethernet II-style encapsulation (no LLC/Snap header)
    Config::SetDefault("ns3::CsmaNetDevice::EncapsulationMode", StringValue("Dix"));

    // Allow the user to override any of the defaults at
    // run-time, via command-line arguments
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(7);
    // We will later want two subcontainers of these nodes, for the two LANs
    NodeContainer c0 = NodeContainer(c.Get(0), c.Get(1), c.Get(2), c.Get(3), c.Get(4));
    NodeContainer c1 = NodeContainer(c.Get(3), c.Get(4), c.Get(5), c.Get(6));

    NS_LOG_INFO("Build Topology.");
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(5000000)));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // We will use these NetDevice containers later, for IP addressing
    NetDeviceContainer nd0 = csma.Install(c0); // First LAN


    WifiHelper wifi;
    if (verbose) {
        wifi.EnableLogComponents(); // Turn on all Wifi logging
    }
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    std::string phyMode("DsssRate11Mbps");
    double rss = -80; // -dBm
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    // This is one parameter that matters when using FixedRssLossModel
    // set it to zero; otherwise, gain will be added
    wifiPhy.Set("RxGain", DoubleValue(0));
    // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // The below FixedRssLossModel will cause the rss to be fixed regardless
    // of the distance between the two stations, and the transmit power
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
    wifiPhy.SetChannel(wifiChannel.Create());

    // Add a non-QoS upper mac, and disable rate control
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue(phyMode),
            "ControlMode", StringValue(phyMode));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer nd1;
    AimfHelper aimf;
    Ipv4StaticRoutingHelper staticRouting;
    Ipv4StaticRoutingHelper staticRouting2;
    Ipv4ListRoutingHelper list;
    Ipv4ListRoutingHelper list2;
    InternetStackHelper internet;
    InternetStackHelper internet2;
    InternetStackHelper internet3;

    if (wifiok) {
        nd1 = wifi.Install(wifiPhy, wifiMac, c1); // Second LAN
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(5.0, 0.0, 0.0));
        positionAlloc->Add(Vector(5.0, 2.0, 0.0));
        positionAlloc->Add(Vector(0.0, 2.0, 0.0));
        positionAlloc->Add(Vector(0.0, 2.0, 0.0));
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(c1);
        NS_LOG_INFO("Add IP Stack. Wireless chosen!");
        OlsrHelper olsr;
        OlsrHelper olsr2;
        olsr.ExcludeInterface(c.Get(3), 1);
        olsr.ExcludeInterface(c.Get(4), 1);
        aimf.ExcludeInterface(c.Get(3), 2);
        aimf.ExcludeInterface(c.Get(4), 2);
        aimf.ExcludeInterface(c.Get(3), 0);
        aimf.ExcludeInterface(c.Get(4), 0);
        list.Add(staticRouting, 10);
        list.Add(aimf, 12);
        list.Add(olsr, 11);
        list2.Add(staticRouting2, 0);
        list2.Add(olsr2, 9);


        internet.Install(NodeContainer(c.Get(0), c.Get(1), c.Get(2)));


        internet2.SetRoutingHelper(list);
        internet2.Install(NodeContainer(c.Get(3), c.Get(4)));
        internet3.SetRoutingHelper(list2);
        internet3.Install(NodeContainer(c.Get(5), c.Get(6)));
    } else {
        nd1 = csma.Install(c1); // Second LAN
        NS_LOG_INFO("Add IP Stack.");
        aimf.ExcludeInterface(c.Get(3), 2);
        aimf.ExcludeInterface(c.Get(4), 2);
        list.Add(staticRouting, 0);
        list.Add(aimf, 10);
        internet.Install(NodeContainer(c.Get(0), c.Get(1), c.Get(2), c.Get(5), c.Get(6)));
        internet2.SetRoutingHelper(list);
        internet2.Install(NodeContainer(c.Get(3), c.Get(4)));
    }
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4Addr;
    ipv4Addr.SetBase("10.1.1.0", "255.255.255.0");
    ipv4Addr.Assign(nd0);
    ipv4Addr.SetBase("10.1.2.0", "255.255.255.0");
    ipv4Addr.Assign(nd1);


    NS_LOG_INFO("Configure multicasting.");


    Ptr<Ipv4> stack = c.Get(3)->GetObject<Ipv4> ();
    Ptr<Ipv4RoutingProtocol> rp_Gw = (stack->GetRoutingProtocol());
    Ptr<Ipv4ListRouting> lrp_Gw = DynamicCast<Ipv4ListRouting> (rp_Gw);



    Ptr<Ipv4> stack2 = c.Get(4)->GetObject<Ipv4> ();
    Ptr<Ipv4RoutingProtocol> rp_Gw2 = (stack->GetRoutingProtocol());
    Ptr<Ipv4ListRouting> lrp_Gw2 = DynamicCast<Ipv4ListRouting> (rp_Gw2);


    Ptr<aimf::RoutingProtocol> aimf_Gw;
    Ptr<aimf::RoutingProtocol> aimf_Gw2;


    for (uint32_t i = 0; i < lrp_Gw->GetNRoutingProtocols(); i++) {
        int16_t priority;
        Ptr<Ipv4RoutingProtocol> temp = lrp_Gw->GetRoutingProtocol(i, priority);
        if (DynamicCast<aimf::RoutingProtocol> (temp)) {
            aimf_Gw = DynamicCast<aimf::RoutingProtocol>(temp);
        }

    }

    for (uint32_t i = 0; i < lrp_Gw2->GetNRoutingProtocols(); i++) {
        int16_t priority;
        Ptr<Ipv4RoutingProtocol> temp = lrp_Gw2->GetRoutingProtocol(i, priority);
        if (DynamicCast<aimf::RoutingProtocol> (temp)) {
            aimf_Gw2 = DynamicCast<aimf::RoutingProtocol>(temp);
        }

    }
    Ipv4StaticRoutingHelper multicast;

    Ipv4Address multicastSource("10.1.1.1");
    Ipv4Address multicastGroup("225.1.2.4");
    Ipv4Address multicastSource2("10.1.1.2");
    Ipv4Address multicastGroup2("225.1.2.5");
    //    aimf_Gw->AddHostNetworkAssociation(multicastGroup, multicastSource);
    //    aimf_Gw->AddHostNetworkAssociation(multicastGroup2, multicastSource2);



    Ptr<Node> sender = c.Get(0);
    Ptr<NetDevice> senderIf = nd0.Get(0);
    multicast.SetDefaultMulticastRoute(sender, senderIf);
    Ptr<Node> sender2 = c.Get(1);
    Ptr<NetDevice> senderIf2 = nd0.Get(1);
    multicast.SetDefaultMulticastRoute(sender2, senderIf2);


    NS_LOG_INFO("Create Applications.");

    uint16_t multicastPort = 9; // Discard port (RFC 863)


    OnOffHelper onoff("ns3::UdpSocketFactory",
            Address(InetSocketAddress(multicastGroup, multicastPort)));
    onoff.SetConstantRate(DataRate("255b/s"));
    onoff.SetAttribute("PacketSize", UintegerValue(128));

    ApplicationContainer srcC = onoff.Install(c0.Get(0));

    OnOffHelper onoff2("ns3::UdpSocketFactory",
            Address(InetSocketAddress(multicastGroup2, multicastPort)));
    onoff2.SetConstantRate(DataRate("255b/s"));
    onoff2.SetAttribute("PacketSize", UintegerValue(128));

    ApplicationContainer srcC2 = onoff2.Install(c0.Get(1));


    srcC.Start(Seconds(1.));
    srcC.Stop(Seconds(1000.));
    srcC2.Start(Seconds(1.));
    srcC2.Stop(Seconds(1000.));


    PacketSinkHelper sink("ns3::UdpSocketFactory",
            InetSocketAddress(Ipv4Address::GetAny(), multicastPort));
    ApplicationContainer sinkC = sink.Install(c1.Get(2));
    sinkC.Start(Seconds(1.0));
    sinkC.Stop(Seconds(30.0));

    NS_LOG_INFO("Configure Tracing.");
    //
    // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
    // Ascii trace output will be sent to the file "csma-multicast.tr"
    //
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("csma-multicast.tr"));

    // Also configure some tcpdump traces; each interface will be traced.
    // The output files will be named:
    //     csma-multicast-<nodeId>-<interfaceId>.pcap
    // and can be read by the "tcpdump -r" command (use "-tt" option to
    // display timestamps correctly)
    if (wifiok)csma.EnablePcapAll("aimf-multicastC", false);
    wifiPhy.EnablePcap("aimf-multicastW", c1);

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");

    Simulator::Schedule(Seconds(1.0), &aimf::RoutingProtocol::ChangeWillingness, aimf_Gw, 1);
    Simulator::Schedule(Seconds(500.0), &aimf::RoutingProtocol::ChangeWillingness, aimf_Gw, 5);
    Simulator::Schedule(Seconds(3.0), &aimf::RoutingProtocol::AddHostMulticastAssociation, aimf_Gw, multicastGroup, multicastSource);
    Simulator::Schedule(Seconds(800.0), &aimf::RoutingProtocol::AddHostMulticastAssociation, aimf_Gw2, multicastGroup2, multicastSource2);

    Simulator::Stop(Seconds(1001.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
}
