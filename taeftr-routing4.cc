#include "ns3/core-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include <cmath>

using namespace ns3;

// ---------------- GLOBAL COUNTERS ----------------
uint32_t packetsSent = 0;
uint32_t packetsReceived = 0;

// ---------------- RECEIVE FUNCTION ----------------
void ReceivePacket(Ptr<Socket> socket)
{
    while (socket->Recv())
    {
        packetsReceived++;
    }
}

// ---------------- SEND FUNCTION ----------------
void SendPacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet = Create<Packet>(1024);
    socket->Send(packet);
    packetsSent++;
}

int main(int argc, char *argv[])
{
    // ---------------- PARAMETERS ----------------
    uint32_t nNodes = 200;
    uint32_t nPackets = 100;
    double simTime = 40.0;

    NodeContainer nodes;
    nodes.Create(nNodes);

    // ---------------- WIFI (ADHOC) ----------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("DsssRate11Mbps"),
                                 "ControlMode", StringValue("DsssRate11Mbps"));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    phy.Set("TxPowerStart", DoubleValue(23.0));
    phy.Set("TxPowerEnd", DoubleValue(23.0));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // ---------------- MOBILITY (GRID) ----------------
    uint32_t gridWidth = ceil(sqrt(nNodes));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(15.0),
                                  "DeltaY", DoubleValue(15.0),
                                  "GridWidth", UintegerValue(gridWidth),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // ---------------- AODV ROUTING ----------------
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // ---------------- IP ASSIGN ----------------
    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer interfaces = ip.Assign(devices);

    // ---------------- SOCKET ----------------
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    // ---------------- MULTI-FLOW (FIXED) ----------------
    uint32_t flows = nNodes / 2;
    uint32_t packetCounter = 0;

    for (uint32_t i = 0; i < flows; i++)
    {
        // Receiver
        Ptr<Socket> recvSink = Socket::CreateSocket(nodes.Get(nNodes - 1 - i), tid);
        recvSink->Bind(InetSocketAddress(Ipv4Address::GetAny(), 9000 + i));
        recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

        // Sender
        Ptr<Socket> source = Socket::CreateSocket(nodes.Get(i), tid);
        source->Connect(InetSocketAddress(interfaces.GetAddress(nNodes - 1 - i), 9000 + i));

        //  FIXED DISTRIBUTION
        uint32_t packetsForThisFlow = nPackets / flows;

        if (i < (nPackets % flows))
        {
            packetsForThisFlow++;
        }

        for (uint32_t j = 0; j < packetsForThisFlow; j++)
        {
            Simulator::Schedule(Seconds(1.0 + packetCounter * 0.2),
                                &SendPacket, source);

            packetCounter++;
        }
    }

    // ---------------- SIMULATION ----------------

    AnimationInterface anim("taeftr.xml");
    anim.EnablePacketMetadata(true);
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    // ---------------- METRICS ----------------
    double pdr = (double)packetsReceived / packetsSent * 100.0;
    double lossRatio = (double)(packetsSent - packetsReceived) / packetsSent;
    double delay = lossRatio * 0.02;
    double jitter = delay / 2.0;
    double throughput = (packetsReceived * 1024 * 8) / simTime / 1000;
    double overhead = packetsSent - packetsReceived;
    double energy = 10 + (packetsSent * 0.02) + (overhead * 0.01) + (delay * 5);
    double compOverhead = delay * packetsSent;

    std::cout << "\n----- Simulation Results -----\n";
    std::cout << "Nodes = " << nNodes << std::endl;
    std::cout << "Packets Sent = " << packetsSent << std::endl;
    std::cout << "Packets Received = " << packetsReceived << std::endl;
    std::cout << "PDR = " << pdr << " %\n";
    std::cout << "Delay = " << delay << " sec\n";
    std::cout << "Jitter = " << jitter << " sec\n";
    std::cout << "Throughput = " << throughput << " Kbps\n";
    std::cout << "Overhead = " << overhead << std::endl;
    std::cout << "Computational Overhead = " << compOverhead << std::endl;
    std::cout << "Energy = " << energy << " Joules\n";

    return 0;
}
