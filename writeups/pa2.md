Programming Assignment 2 Writeup
====================

My name: [Ethan Stark]

My UTORID : [starketh]

I would like to credit/thank these classmates for their help: [Not applicable I didn't communicate with any of my classmates about this assignment.]

This programming assignment took me about [8] hours to do.

Program Structure and Design of the Router:
[
    To complete this assignment I created a struct for routing table entries which just stores the values of the arguments provided to the add_route method (but note that instead of storing the whole Address object for next_hop I instead extract the IP address and store that so that my routing table entries are more compact). Then I added a field to the router class for the routing table which I decided to implement as vector so that it is easy to iterate through all of the entries and match the prefixes.

    When adding a route to the routing table I check that the prefix length and interface numbers are valid and also make sure that it is not a duplicate entry before adding it to the table.

    When routing I simply iterate through all of the routers interfaces, then iterate through all of the datagrams stored on each interface using maybe_receive. Then, for each datagram I extract its destination IP address and iterate through each routing table entry. To get the longest matching prefix I create a prefix mask and apply it to both the destination IP address and the route prefix address. If the masked values match and this routing table entry has the longest prefix length I have seen so far then I update the longest prefix match to the current entry. Once I have found the longest prefix match I ensure the TTL is not 0 or 1, and if it is not then I decrement the TTL and recompute the datagram's checksum. At this point I am ready to send the datagram so I retrieve the correct interface depending on the routing table entry. If the next hop in the routing table entry is null then I use the destination IP address to send the datagram directly, else if the next_hop is not null then I send to the next hop IP address for further routing.
]

Implementation Challenges:
[
    This assignment was simpler than the first assignment and I was already used to the project structure and util files so I didn't struggle as much with this assignment. With that being said, the one implementation challenge I had was figuring out how to update the header values in a datagram. Once I figured out how to extract the datagram header values and update them, I forgot that whenever we update a header field we must also recalculate the checksum. Since I forgot to update the checksum, I had to do a bit of debugging to figure out why my tests were failing, until I looked at the failing tests and noticed that they updated the checksum along with the TTL field.
]

Remaining Bugs:
[
    I don't believe there are any bugs left in my router implementation, although like I mentioned in PA1 I could adhere to encapsulation principles better by adding getter and setter methods for the routing table since it is a private variable. One thing I would change about the current implementation is to handle the broadcast IP address (and other reserved IP addresses) when routing. For example ensuring that routing table entries for the broadcast IP address should be sent out on all interfaces besides the one the datagram was recieved on.
]

- Optional: I had unexpected difficulty with: [Nothing, I thought this assignment was very well structured, easy to follow, and had clear explanations of the specifications.]

- Optional: I think you could make this lab better by: [Giving a more in depth explanation of the other files in the util directory for those who are curious or would find it helpful to see how the other components of the IP and ethernet layers work.]

- Optional: I was surprised by: []

- Optional: I'm not sure about: []
