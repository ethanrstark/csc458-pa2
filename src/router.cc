#include "router.hh"

#include <iostream>
#include <limits>
#include <algorithm>

using namespace std;
    
// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const std::optional<Address> next_hop, // Consider changing to directly use IPv4 addresses if Address is unnecessary
                       const size_t interface_num) {
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

    // Validate next hop
    if (next_hop.has_value()) {
        uint32_t mask = (prefix_length == 0) ? 0 : (~0U << (32 - prefix_length));
        if ((route_prefix & mask) != (next_hop->ipv4_numeric() & mask)) {
            // Next hop does not belong to the specified prefix
            return;
        }
    }

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

    // Check for existing default route (a route with prefix length 0)
    bool has_default_route = std::any_of(routing_table_.begin(), routing_table_.end(), [](const RoutingTableEntry& entry) {
        return entry.prefix_len == 0;
    });
    if (has_default_route) {
        return;
    }

    // Add route to the routing table
    routing_table_.push_back(new_route);
}

void Router::route() {
    // Iterate through all interfaces
    for (size_t i = 0; i < interfaces_.size(); i++) {
        AsyncNetworkInterface& net_interface = interface(i);

        // Process all datagrams received on the current interface
        while (auto datagram = net_interface.maybe_receive()) {
            uint32_t dst_addr = datagram->header.dst;
            std::optional<RoutingTableEntry> best_match = std::nullopt;
            uint8_t best_match_len = 0;

            // Find the best matching route
            for (const auto& entry : routing_table_) {
                uint32_t mask = (entry.prefix_len == 0) ? 0 : (~0U << (32 - entry.prefix_len));
                if ((entry.prefix_addr & mask) == (dst_addr & mask)) {
                    if (!best_match.has_value() || entry.prefix_len > best_match_len) {
                        best_match = entry;
                        best_match_len = entry.prefix_len;
                    }
                }
            }

            if (best_match.has_value()) {
                // Decrement TTL and forward datagram if valid
                datagram->header.ttl -= 1;
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


// void Router::add_route( const uint32_t route_prefix,
//                         const uint8_t prefix_length,
//                         const optional<Address> next_hop, // TODO: Maybe change this to use the IP address directly instead of the whole Address object
//                         const size_t interface_num )
// {
//   cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
//        << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
//        << " on interface " << interface_num << "\n";
// 
//     // Validate the prefix length to ensure it's within the correct range (0-32)
//     if (prefix_length < 0 || prefix_length > 32) {
//         throw std::invalid_argument("Invalid prefix length");
//     }
// 
//     // Add the new route entry to the routing table
//     if (next_hop.has_value()) {
//       // If there's a next hop, use the next hop's IPv4 numeric address
//       routing_table_.emplace_back(RoutingTableEntry{route_prefix, prefix_length, next_hop->ipv4_numeric(), interface_num});
//     } else {
//       // If there's no next hop, set next_hop_addr to null
//       routing_table_.emplace_back(RoutingTableEntry{route_prefix, prefix_length, std::nullopt, interface_num});
//     }
// 
// 
//     // TODO we should make sure that the route we are trying to add does not already exist in the routing table
//     // TODO if prefix len is zero, this is a failsafe option but IDK what we would do if these are the 
//     // Longest prefixes anf there are multiple entries with zero prefix length
//     // Maybe we could do something like this:
//     // if (prefix_length == 0 and there is already a route with prefix length 0) {
//     //    throw std::invalid_argument("Invalid prefix length");
// 
//     // TODO Should I check whether or not the next_hop falls within the network address prefix or is this overkill?
// }


// void Router::route()
// {
//   // First iterate through all of the interfaces on this router
//   for ( size_t i = 0; i < interfaces_.size(); i++ ) {
// 
//     // Get the next interface and process all of the datagrams that have been received on it
//     AsyncNetworkInterface& net_interface = interface( i );
//     while ( auto datagram = net_interface.maybe_receive() ) { // TODO can maybe make this const
// 
//       uint32_t dst_addr = datagram->header.dst;
//       std::optional<RoutingTableEntry> best_match = std::nullopt;
//       uint8_t best_match_len = 0;
// 
//       for ( const auto& entry : routing_table_ ) {
//         uint32_t route_prefix = entry.prefix_addr;
//         uint8_t prefix_length = entry.prefix_len;
// 
//         // The mask for extracting the prefix from the addresses
//         uint32_t mask = ( prefix_length == 0 ) ? 0 : ( ~0U << ( 32 - prefix_length ) );
//         uint32_t masked_route = route_prefix & mask;
//         uint32_t masked_dst = dst_addr & mask;
// 
//         if ( masked_route == masked_dst ) {
//           if (!best_match.has_value() || prefix_length > best_match_len) {         
//             // Update the best match if this is the longest prefix match seen so far
//             best_match = entry;
//             best_match_len = prefix_length;
//           }
//         }
// 
//       if ( best_match.has_value() ) {
//         AsyncNetworkInterface& interface_out = interface( best_match->interface_num );
//         datagram->header.ttl -= 1;
//         if ( datagram->header.ttl > 0 ) {
//           // One of the prefixes matched, so send the datagram to the next hop
//           if ( best_match->next_hop_addr.has_value() ) {
//             Address next_hop = Address::from_ipv4_numeric( best_match->next_hop_addr.value() );
//             interface_out.send_datagram( *datagram, next_hop );
//           } else {
//             interface_out.send_datagram( *datagram, Address::from_ipv4_numeric( dst_addr ) );
//           }
//         }
// 
//         //Address next_hop = best_match->next_hop_addr.has_value() ? Address::from_ipv4_numeric( best_match->next_hop_addr.value() ) : Address::from_ipv4_numeric( dst_addr );
//         //interface( best_match->interface_num ).send_datagram( *datagram, next_hop );
//       }
//     }
//   }
//   }
// }


// // TODO Could make this a public method of the Router class
// 
//     // Function to find the longest matching route for a given destination address
//     std::optional<RoutingTableEntry> find_longest_matching_route(uint32_t dest_addr) {
//         std::optional<RoutingTableEntry> best_match;
//         uint8_t best_match_len = 0;
// 
//         // Iterate through the routing table and find the longest matching prefix
//         for (const auto& entry : routing_table_) {
//             // Create a mask for the prefix length
//             uint32_t mask = (entry.prefix_len == 0) ? 0 : (~0U << (32 - entry.prefix_len));
//             uint32_t masked_dest = dest_addr & mask;
//             uint32_t masked_prefix = entry.prefix_addr & mask;
// 
//             // If the prefix matches the destination address for the given prefix length
//             if (masked_dest == masked_prefix) {
//                 // If the match is longer than the current best match, update the best match
//                 if (entry.prefix_len > best_match_len) {
//                     best_match = entry;
//                     best_match_len = entry.prefix_len;
//                 }
//             }
//         }
// 
//         return best_match;
//     }
// };

