#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-cubic.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/log.h"
using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("FifthScriptExample");

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets,
              DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);
  void ScheduleTx (void);
  void SendPacket (void);
  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};
MyApp::MyApp ()
    : m_socket (0),
      m_peer (),
      m_packetSize (0),
      m_nPackets (0),
      m_dataRate (0),
      m_sendEvent (),
      m_running (false),
      m_packetsSent (0)
{
}
MyApp::~MyApp ()
{
  m_socket = 0;
}
void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets,
              DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}
void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  m_socket->Bind ();
  m_socket->Connect (m_peer);
  SendPacket ();
}
void
MyApp::StopApplication (void)
{
  m_running = false;
  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }
  if (m_socket)
    {
      m_socket->Close ();
    }
}
void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);
  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}
void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}
static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd << "\t" << oldCwnd);
}
//trace the instantaneous throughput
static void
ThroughputTrace (Ptr<const Packet> packet, const TcpHeader &tcpHeader,
                 Ptr<const TcpSocketBase> socket)
{
  // Calculate throughput
//   double throughput = packet->GetSize () * 8.0 / 1e6; // in Mbps
//   NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\tThroughput: " << throughput << " \tMbps");
//   NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << " " << throughput);
}
int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);
  // Create nodes
  NodeContainer nodes;
  nodes.Create (4);
  // Set node positions using MobilityHelper
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      double angle = (i * 2.0 * M_PI) / (nodes.GetN ()); // Place nodes symmetrically
      double x = 10.0 * std::cos (angle);
      double y = 10.0 * std::sin (angle);
      positionAlloc->Add (Vector (x, y, 0.0));
    }
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);
  // Create remote host
  NodeContainer remoteHost;
  remoteHost.Create (1);
  // Set remote host position
  Ptr<ListPositionAllocator> remotePositionAlloc = CreateObject<ListPositionAllocator> ();
  remotePositionAlloc->Add (Vector (0.0, 0.0, 0.0)); // Center of the ring 
  mobility.SetPositionAllocator (remotePositionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (remoteHost);
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  std::string delay = "2ms";
  p2p.SetChannelAttribute ("Delay", StringValue (delay));
  
  NetDeviceContainer devices;
  for (uint32_t i = 0; i < nodes.GetN () - 1; ++i)
    {
      devices.Add (p2p.Install (nodes.Get (i), nodes.Get ((i + 1) % nodes.GetN ())));
    }
  NetDeviceContainer remoteDevices;
  for (uint32_t i = 0; i < nodes.GetN () - 1; ++i)
    {
      remoteDevices.Add (p2p.Install (nodes.Get (i), remoteHost.Get (0)));
    }
  // Install Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);
  stack.Install (remoteHost);
  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  address.SetBase ("10.1.2.0", "255.255.255.200");
  Ipv4InterfaceContainer interfaces2 = address.Assign (remoteDevices);
  // Set up TCP applications Cubic as one of the TCP congestion control algorithms
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpCubic"));
  uint32_t port = 9;
  BulkSendHelper bulkSender (
      "ns3::TcpSocketFactory",
      InetSocketAddress (remoteHost.Get (0)->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal (),
                         port));
  bulkSender.SetAttribute ("MaxBytes", UintegerValue (10000000)); //Set maximum bytes to send
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      ApplicationContainer appsBulk = bulkSender.Install (nodes.Get (i));
      appsBulk.Start (Seconds (1.0));
      appsBulk.Stop (Seconds (50.0));
    }
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interfaces2.GetAddress (0), sinkPort));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      ApplicationContainer Sinkapps = packetSinkHelper.Install (nodes.Get (i));
      Sinkapps.Start (Seconds (0.0));
      Sinkapps.Stop (Seconds (50.0));
    }
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get (0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
  ns3TcpSocket->TraceConnectWithoutContext ("Tx", MakeCallback (&ThroughputTrace));
  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 1040, 1000, DataRate ("1Mbps"));
  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      nodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (1.));
      app->SetStopTime (Seconds (50.));
    }
  // Create FlowMonitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll ();
  Simulator::Stop (Seconds (50));
  Simulator::Run ();
  Simulator::Destroy ();
  // Collect statistics Print flow statistics
  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator it = stats.begin ();
       it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      std::cout << "Flow " << it->first << " (" << t.sourceAddress << " -> " << t.destinationAddress
                << ")\n";
      std::cout << " Tx Packets: " << it->second.txPackets << "\n";
      std::cout << " Tx Bytes: " << it->second.txBytes << "\n";
      std::cout << " Rx Packets: " << it->second.rxPackets << "\n";
      std::cout << " Rx Bytes: " << it->second.rxBytes << "\n";
      std::cout << " Throughput: " << it->second.rxBytes * 8.0 / (50.0 * 1000000.0) << " Mbps\n";
      // Add print statements for Delay and Packet Loss Rate
      std::cout << " Delay: " << it->second.delaySum.GetSeconds () / it->second.rxPackets
                << " seconds\n";
      std::cout << " Packet Loss Rate: "
                << static_cast<double> (it->second.lostPackets) /
                       static_cast<double> (it->second.txPackets)
                << "\n";
      std::cout << t.sourceAddress << " -> " << t.sourcePort << " - " << t.destinationAddress
                << " -> " << t.destinationPort << "\n\n";
    }
  return 0;
}