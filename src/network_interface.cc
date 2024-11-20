#include "network_interface.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// Helper function to create an Ethernet frame with the given destination MAC-address, 
// source MAC-address, and type of frame. This function only initializes the header of the frame.
// Returns the created Ethernet frame.
EthernetFrame create_ethernet_frame(EthernetAddress dst, EthernetAddress src, uint16_t type) 
{
    EthernetFrame frame;
    frame.header.dst = dst;
    frame.header.src = src;
    frame.header.type = type;
    return frame;
}

// Helper function to create an ARP message with the given opcode, sender MAC-address,
// sender IP-address, and target IP-address. Returns the created ARPMessage.
// Note that this function doesn't take the target MAC-address as an argument
// because it changes depending on whether the ARP message is a request or reply.
// If you want to create an ARP reply, you can set the target MAC-address after calling this function.
ARPMessage create_arp_message(uint16_t opcode, EthernetAddress sender_ethernet_addr, 
                            uint32_t sender_ip_addr, uint32_t target_ip_addr) 
{
    ARPMessage arp_message;
    arp_message.opcode = opcode;
    arp_message.sender_ethernet_address = sender_ethernet_addr;
    arp_message.sender_ip_address = sender_ip_addr;
    arp_message.target_ip_address = target_ip_addr;
    return arp_message;
}

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress& ethernet_address, 
                                   const Address& ip_address) : 
    ethernet_address_(ethernet_address), 
    ip_address_(ip_address),
    ready_queue_(),
    arp_queue_(),
    arp_table_() {


    cerr << "DEBUG: Network interface has Ethernet address ";
    cerr << to_string(ethernet_address_);
    cerr << " and IP address ";
    cerr << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram(const InternetDatagram& dgram, const Address& next_hop)
{
    uint32_t next_hop_IP_num = next_hop.ipv4_numeric();
    auto iterator = arp_table_.find(next_hop_IP_num);

    if (iterator != arp_table_.end() && iterator->second.state == ARPState::REACHABLE) {
        // There is an entry for the next_hop IP-address in the ARP-table and 
        // the corresponding MAC-address is known (its state is REACHABLE).
        // So, initialize the Ethernet frame and add it to the ready queue for sending
        EthernetFrame dgram_frame = create_ethernet_frame(iterator->second.mac_address, ethernet_address_, 
                                                        EthernetHeader::TYPE_IPv4);
        dgram_frame.payload = serialize(dgram);
        ready_queue_.push(dgram_frame);

    } else {
        // We are missing the MAC address of next_hop so append
        // dgram to the queue that is waiting for the ARP reply
        arp_queue_[next_hop_IP_num].push(dgram);

        if (iterator == arp_table_.end()) {
            // There is no entry for this IP-address in the ARP-table so add it
            ARPEntry arp_entry = {{}, ARPState::INCOMPLETE, 5000};
            arp_table_[next_hop_IP_num] = arp_entry;

            // Create an ARP request to find the MAC address of the next hop
            ARPMessage arp_request = create_arp_message(ARPMessage::OPCODE_REQUEST, ethernet_address_, 
                                                        ip_address_.ipv4_numeric(), next_hop_IP_num);

            // Create an Ethernet frame to encapsulate the ARP request and add it to the ready queue
            EthernetFrame arp_frame = create_ethernet_frame(ETHERNET_BROADCAST, ethernet_address_, 
                                                            EthernetHeader::TYPE_ARP);
            arp_frame.payload = serialize(arp_request);
            ready_queue_.push(arp_frame);
        }
    }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame& frame)
{
    if (frame.header.dst == ethernet_address_ || frame.header.dst == ETHERNET_BROADCAST) {
        // The packet is destined for this machine
        
        if (frame.header.type == EthernetHeader::TYPE_IPv4) {
            // The payload is an IPv4 datagram so try to parse it
            InternetDatagram dgram;
            if (!parse(dgram, frame.payload)) {
                return {};
            }
            return dgram;

        } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
            // The payload is an ARP packet so try to parse it
            ARPMessage arp_message;
            if (!parse(arp_message, frame.payload)) {
                return {};
            }
            // Add/update the entry in the ARP-table for the sender of the ARP message
            ARPEntry arp_entry = {arp_message.sender_ethernet_address, ARPState::REACHABLE, 30000};
            arp_table_[arp_message.sender_ip_address] = arp_entry;

            if (arp_message.opcode == ARPMessage::OPCODE_REQUEST && arp_message.target_ip_address == ip_address_.ipv4_numeric()) {
                // It is an ARP request for this machine's MAC-address

                // Create an ARP reply to send back
                ARPMessage arp_reply = create_arp_message(ARPMessage::OPCODE_REPLY, ethernet_address_, 
                                                        ip_address_.ipv4_numeric(), arp_message.sender_ip_address);
                arp_reply.target_ethernet_address = arp_message.sender_ethernet_address;

                // Create an Ethernet frame to encapsulate the ARP reply and add it to the ready queue
                EthernetFrame arp_frame = create_ethernet_frame(arp_reply.target_ethernet_address, ethernet_address_, 
                                                                EthernetHeader::TYPE_ARP);
                arp_frame.payload = serialize(arp_reply);
                ready_queue_.push(arp_frame);
            } 

            // Send all of the packets that were waiting for this ARP reply by adding them to the ready queue
            queue<InternetDatagram> waiting_queue = arp_queue_[arp_message.sender_ip_address];
            while (!waiting_queue.empty()) {
                InternetDatagram dgram = waiting_queue.front();
                waiting_queue.pop();
                send_datagram(dgram, Address::from_ipv4_numeric(arp_message.sender_ip_address));
            }

            // Remove this entry from the ARP queue since we have received the corresponding MAC-address
            arp_queue_.erase(arp_message.sender_ip_address);
        }
    } 
    return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick)
{
     // Update the TTL of each entry in the ARP-table and check for expired entries 
    for (ARPTable::iterator it = arp_table_.begin(); it != arp_table_.end(); ) {
        it->second.ttl -= ms_since_last_tick;
        if (it->second.ttl <= 0) {
            // The entry has expired
            if (it->second.state == ARPState::INCOMPLETE) {
                // This is an expired ARP request so clear the waiting queue
                arp_queue_.erase(it->first);
            }

            // Remove the entry from the ARP-table
            it = arp_table_.erase(it);

        } else {
            ++it;
        } 
    }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{   
    if (!ready_queue_.empty()) {
        EthernetFrame frame = ready_queue_.front();
        ready_queue_.pop();
        return frame;
    }
    return {}; 
}
