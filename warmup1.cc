/* Warmup Exercise 1 Summary:
 * I modified the 'myfirst.cc' file, which already set up two nodes with
 * a Point2Point connection. I added an additional server and client
 * on the same nodes, with new server/client helpers. To ensure that 
 * the clients sent the packets to the correct server, I configured the
 * servers to use different port numbers and configured each client to
 * send packets to the correct port.
 * Run this file from the ns-3.29 directory with `./waf --run scratch/warmup1`
 *
 * Towards the bottom of the file, I added the standard lines to create ascii
 * and pcap traces. Read the pcap files with `tcpdump -nn -tt -r warmup1-1-1.pcap`
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Warmup1");

int
main (int argc, char *argv[])
{
  
  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  InternetStackHelper stack;
  stack.Install (nodes);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("0ms"));

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");

  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer1 (9);
  UdpEchoServerHelper echoServer2 (12);

  ApplicationContainer serverApp1 = echoServer1.Install (nodes.Get (1));
  ApplicationContainer serverApp2 = echoServer2.Install (nodes.Get (1));
  serverApp1.Start (Seconds (2.0));
  serverApp1.Stop (Seconds (4.0));
  serverApp2.Start (Seconds (6.0));
  serverApp2.Stop (Seconds (8.0));

  UdpEchoClientHelper echoClient1 (interfaces.GetAddress (1), 9);
  echoClient1.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient1.SetAttribute ("Interval", TimeValue (Seconds (1)));
  echoClient1.SetAttribute ("PacketSize", UintegerValue (1024));

  UdpEchoClientHelper echoClient2 (interfaces.GetAddress (1), 12);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApp1 = echoClient1.Install (nodes.Get (0));
  ApplicationContainer clientApp2 = echoClient2.Install (nodes.Get (0));
  clientApp1.Start (Seconds (2.0));
  clientApp1.Stop (Seconds (4.0));
  clientApp2.Start (Seconds (6.0));
  clientApp2.Stop (Seconds (8.0));

  // Ascii & Pcap Trace Setup
  AsciiTraceHelper ascii;
  pointToPoint.EnableAsciiAll (ascii.CreateFileStream ("warmup1.tr"));
  pointToPoint.EnablePcapAll ("warmup1");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
