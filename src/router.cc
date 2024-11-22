#include "router.hh"

#include <iostream>
#include <limits>
#include <algorithm>

using namespace std;

// Helper function to find the longest matching route in the given routing table for a given destination IP address
std::optional<RoutingTableEntry> longest_prefix_match(const std::vector<RoutingTableEntry>& routing_table, const uint32_t dst_addr) 
{
    std::optional<RoutingTableEntry> best_match = std::nullopt;
    uint8_t best_match_len = 0;

    for (const auto& entry : routing_table) {
        uint32_t mask = (entry.prefix_len == 0) ? 0 : (~0U << (32 - entry.prefix_len));
        uint32_t masked_dst = dst_addr & mask;
        uint32_t masked_prefix = entry.prefix_addr & mask;

        if (masked_dst == masked_prefix) {
            if (!best_match.has_value() || entry.prefix_len > best_match_len) {
                best_match = entry;
                best_match_len = entry.prefix_len;
            }
        }
    }

    return best_match;
}

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const std::optional<Address> next_hop, // Consider changing to directly use IPv4 addresses if Address is unnecessary
                       const size_t interface_num) 
{
    // Debug logging
    std::cerr << "DEBUG: adding route "
              << Address::from_ipv4_numeric(route_prefix).ip() << "/"
              << static_cast<int>(prefix_length) << " => "
              << (next_hop.has_value() ? next_hop->ip() : "(direct)")
              << " on interface " << interface_num << "\n";

    // Validate the prefix length
    if (prefix_length > 32) {
        return;
    }

    // Make sure the interface number is valid
    if (interface_num >= interfaces_.size()) {
        return;
    }

    // Create a new routing table entry
    RoutingTableEntry new_route = {
        route_prefix,
        prefix_length,
        next_hop ? std::optional(next_hop->ipv4_numeric()) : std::nullopt,
        interface_num
    };

    // Check for duplicate routes
    if (std::find(routing_table_.begin(), routing_table_.end(), new_route) != routing_table_.end()) {
        return;
    }

    // Add route to the routing table
    routing_table_.push_back(new_route);
}

void Router::route() 
{
    // Iterate through all interfaces
    for (size_t i = 0; i < interfaces_.size(); i++) {
        AsyncNetworkInterface& net_interface = interface(i);

        // Process all datagrams received on the current interface
        while (auto datagram = net_interface.maybe_receive()) {
            uint32_t dst_addr = datagram->header.dst;
            std::optional<RoutingTableEntry> best_match = longest_prefix_match(routing_table_, dst_addr);
            if (best_match.has_value()) {
                // Check if the TTL has already expired
                if (datagram->header.ttl == 0) {
                    return;
                }

                // Decrement TTL and forward datagram if the TTL is still valid
                datagram->header.ttl -= 1;
                datagram->header.compute_checksum();
                if (datagram->header.ttl > 0) {
                    AsyncNetworkInterface& interface_out = interface(best_match->interface_num);
                    Address next_hop = best_match->next_hop_addr.has_value()
                                       ? Address::from_ipv4_numeric(best_match->next_hop_addr.value())
                                       : Address::from_ipv4_numeric(dst_addr);
                    interface_out.send_datagram(*datagram, next_hop);
                }
            }
        }
    }
}
