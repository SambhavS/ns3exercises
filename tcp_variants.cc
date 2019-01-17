#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TCP_Variants");

/*  Dumbell Topology
 *  1[0]        3[2]
 *  |           |
 *  5[4] ------ 6[5]
 *  |           |
 *  2[1]        4[3]
 */

//MyApp & Helpers copied from fifth.cc
class MyApp : public Application 
{
public:

  MyApp ();
  virtual ~MyApp();

  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);
  void ChangeDataRate(void);

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
//Method added to increase data rate of udp app
void
MyApp::ChangeDataRate(){
	m_dataRate = DataRate ("10Mbps");
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
static void
ChangeRate(Ptr<MyApp> app){
	app->ChangeDataRate();
}

int
main (int argc, char *argv[])
{
  
  Time::SetResolution (Time::NS);
  //Which logging components to add?

  // Initialize nodes, install Internet, make p2p connections, assign addresses
  NodeContainer nodes; 
  nodes.Create(6);

  InternetStackHelper stack;
  stack.Install(nodes);

  NetDeviceContainer devices;

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
  devices.Add(p2p.Install(nodes.Get(0), nodes.Get(4)));
  devices.Add(p2p.Install(nodes.Get(4), nodes.Get(1)));
  devices.Add(p2p.Install(nodes.Get(2), nodes.Get(5)));
  devices.Add(p2p.Install(nodes.Get(5), nodes.Get(3)));
  devices.Add(p2p.Install(nodes.Get(4), nodes.Get(5)));

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  
  // ***TCP Connection***
  // Create, Trace & Run TCP Sink Application
  int sinkPort1 = 8080;
  Address sinkAddress1 (InetSocketAddress (interfaces.GetAddress(2), sinkPort1));
  PacketSinkHelper packetSinkHelper1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort1));
  ApplicationContainer sink_TCP_App = packetSinkHelper1.Install(nodes.Get(2));
  devices.Get(2)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));
  sink_TCP_App.Start (Seconds (1));
  sink_TCP_App.Stop (Seconds (20));
  // Create, Trace & Run TCP Source Application (Requires a TCP socket)
  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId ());
  ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
  Ptr<MyApp> tcp_app = CreateObject<MyApp> ();
  tcp_app->Setup(ns3TcpSocket, sinkAddress1, 1040, 1000, DataRate ("2Mbps"));
  // Add application to socket
  nodes.Get(0)->AddApplication(tcp_app);
  tcp_app->SetStartTime(Seconds (1));
  tcp_app->SetStopTime (Seconds (20));

  // ***UDP Connection***
  // Create, Trace & Run UDP Sink Application
  int sinkPort2 = 8081;
  Address sinkAddress2 (InetSocketAddress (interfaces.GetAddress(3), sinkPort2));
  PacketSinkHelper packetSinkHelper2 ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort2));
  ApplicationContainer sink_UDP_App = packetSinkHelper2.Install (nodes.Get(3));
  devices.Get(3)->TraceConnectWithoutContext ("PhyRxDrop", MakeCallback (&RxDrop));
  sink_UDP_App.Start (Seconds (1));
  sink_UDP_App.Stop (Seconds (60));
  // Create, Trace & Run UDP Source Application (Requires a UDP socket)
  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket(nodes.Get(1), UdpSocketFactory::GetTypeId ());
  ns3UdpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));
  Ptr<MyApp> udp_app = CreateObject<MyApp> ();
  udp_app->Setup(ns3UdpSocket, sinkAddress2, 1040, 1000, DataRate ("5Mbps"));
  // Add application to socket
  nodes.Get(1)->AddApplication(udp_app);
  udp_app->SetStartTime(Seconds (20));
  udp_app->SetStopTime (Seconds (60));

  // Run simulator
  NS_LOG_UNCOND("Running simulator...");
  Simulator::Run ();
  Simulator::Schedule(Seconds (30), &ChangeRate, udp_app);
  Simulator::Destroy ();

}
