#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TCP_Variants");

class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void ChangeRate (DataRate newrate);
private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
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

MyApp::~MyApp()
{
  m_socket = 0;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
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

void
MyApp::ChangeRate(DataRate newrate)
{
   m_dataRate = newrate;
   return;
}

static void
CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
}

static void
RxDrop (Ptr<const Packet> p)
{
  NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
}
void
IncRate (Ptr<MyApp> app, DataRate rate)
{
    // Need to use pointer/attributes because data field is private
    app->ChangeRate(rate);
    return;
}
/*  Dumbell Topology
 *  1[0]        3[2]
 *  |           |
 *  5[4] ------ 6[5]
 *  |           |
 *  2[1]        4[3]
 */
int 
main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  //Create nodes (computers)
  NodeContainer nodes;
  nodes.Create(6);

  // Create container that corresponds to two nodes, each with a net device
  // The net devices are both connected to the same point to point channel
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("500kbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
  NetDeviceContainer devs04 = pointToPoint.Install(nodes.Get(0), nodes.Get(4));
  NetDeviceContainer devs14 = pointToPoint.Install(nodes.Get(1), nodes.Get(4));
  NetDeviceContainer devs45 = pointToPoint.Install(nodes.Get(4), nodes.Get(5));
  NetDeviceContainer devs25 = pointToPoint.Install(nodes.Get(2), nodes.Get(5));
  NetDeviceContainer devs35 = pointToPoint.Install(nodes.Get(3), nodes.Get(5));


  //Setup error reporting for 'errored packets'
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  devs04.Get(1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  // Install Internet
  InternetStackHelper stack;
  stack.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper IP_assigner;
  // Set address to start at
  IP_assigner.SetBase ("10.1.1.0", "255.255.255.0");
  // Assigns addresses to all devices in container
  Ipv4InterfaceContainer interf04 = IP_assigner.Assign (devs04);
  // We need to change the starting adress for these devices
  // because otherwise our assigner will restart at the base and reassign addresses
  IP_assigner.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer interf14 = IP_assigner.Assign (devs14);
  IP_assigner.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer interf45 = IP_assigner.Assign (devs45);
  IP_assigner.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer interf25 = IP_assigner.Assign (devs25);
  IP_assigner.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer interf35 = IP_assigner.Assign (devs35);

  // Enable global routing, allowing all the channels to connect
  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  // ***********************
  // TCP: Connect n1 to n3
  // We install the PacketSinkApplication on our sink node. 
  // This application tells the node how to operate as a sink.
  uint16_t sinkPort = 8080;
  Address sinkAddress (InetSocketAddress (interf25.GetAddress(0), sinkPort));
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get(2));
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (20.));

  // Here we make a sink socket and trace its congestion window
  // Congestion window is max amount of data a source can send without being acknowledged
  // Window should keep growing while it is being acknowledged
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (nodes.Get(0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));


  // The source node needs to know where to send packets to.
  // The packets should go to the sink node at the sink port, where the application is running
  // We set up our application that has our sink socket/address/info on our source node
  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3TcpSocket, sinkAddress, 1040, 1000, DataRate("100Kbps"));
  nodes.Get(0)->AddApplication (app);
  app->SetStartTime (Seconds (1.0));
  app->SetStopTime (Seconds (20.0));

  // We trace whether any packets have been dropped
  devs25.Get(0)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));
  // ***********************
  // UDP: Connect n2 to n4
  // Set up sink node
  uint16_t sinkPort2 = 7070;
  Address sinkAddress2 (InetSocketAddress (interf35.GetAddress(0), sinkPort2));
  PacketSinkHelper packetSinkHelper2("ns3::UdpSocketFactory", sinkAddress2);
  ApplicationContainer sinkApps2 = packetSinkHelper2.Install (nodes.Get(3));
  sinkApps2.Start (Seconds (0.0));
  sinkApps2.Stop (Seconds (100.0));

  // Make UDP socket
  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (nodes.Get (1), UdpSocketFactory::GetTypeId ());

   // Set up source node
  Ptr<MyApp> app2 = CreateObject<MyApp> ();
  app2->Setup (ns3UdpSocket, sinkAddress2, 1040, 100000, DataRate("250Kbps"));
  nodes.Get (1)->AddApplication (app2);
  app2->SetStartTime (Seconds (20.));
  app2->SetStopTime (Seconds (100.));

  // Increase rate
  Simulator::Schedule(Seconds(30.0),  &IncRate, app2, DataRate("500kbps"));

  // Make flow monitor
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll ();

  // Run simulator
  Simulator::Stop (Seconds (100));
  Simulator::Run ();
  // Check packet flow
  flowmon->CheckForLostPackets ();
  flowmon->SerializeToXmlFile("lab-2.flowmon", true, true);
  Simulator::Destroy ();

  return 0;
}