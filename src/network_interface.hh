#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"
#include "arp_message.hh"

#include <iostream>
#include <list>
#include <optional>
#include <queue>
#include <unordered_map>
#include <utility>

// Enumeration of possible states for an ARP entry
enum class ARPState {
  REACHABLE,  // ARP entry is valid and the MAC address is currently usable
  INCOMPLETE, // ARP request has been sent but the corresponding MAC address has not yet been resolved
  STALE,      // ARP entry was used previously but has not been confirmed recently
  PERMANENT   // ARP entry will not be aged out (manually added static entry)
};
// Note that we never use the STALE or PERMANENT states for the assignment implementation, but 
// it is included for completeness and extendability.


// Structure to store an ARP entry in the ARP table
struct ARPEntry {
  EthernetAddress mac_address; // MAC address
  ARPState state;              // Status of the entry
  int ttl;                     // Time to live in milliseconds
};


// Type declarations for the ready queue, ARP waiting queue, and ARP table:
using ReadyQueue = std::queue<EthernetFrame>;
using ARPQueue = std::unordered_map<uint32_t, std::queue<InternetDatagram>>;
using ARPTable = std::unordered_map<uint32_t, ARPEntry>;


// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
private:
  // Ethernet (known as hardware, network-access, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as Internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Ready-to-be-sent queue, stored as a queue of Ethernet frames
  ReadyQueue ready_queue_;

  // Queue of Ethernet frames awaiting transmission once the MAC address is resolved
  // Stored as a mapping from IP address to a queue of IPv4 datagrams
  ARPQueue arp_queue_;

  // Mapping from IP address to corresponding ethernet address
  ARPTable arp_table_;

public:
  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address );

  // Access queue of Ethernet frames awaiting transmission
  std::optional<EthernetFrame> maybe_send();

  // Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address
  // for the next hop.
  // ("Sending" is accomplished by making sure maybe_send() will release the frame when next called,
  // but please consider the frame sent as soon as it is generated.)
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, returns the datagram.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  std::optional<InternetDatagram> recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

};
