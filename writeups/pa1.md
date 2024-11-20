Programming Assignment 1 Writeup
====================

My name: Ethan Stark

My UTORID: starketh

My email address: ethan.stark@mail.utoronto.ca


I collaborated with: [Not applicable] 
// List everybody who you discussed the assignment with here. Please remember the academic integrity policies discussed in Handout #1. 

I used the following tools: [I used ChatGPT to help me understand C++ rules and syntax so I could better understand what the code in the project meant]
// List any AI and code generation tools you may have used here.

I would like to credit/thank these classmates for their help: [Whoever answered my Piazza posts (one person that I can remember was David Lin), besides that I didn't talk to anybody about the assignment]

This programming assignment took me about [18] hours to complete.

Program Structure and Design of the NetworkInterface:
// Describe the structure of your code and any design decisions you made that you think the graders need to be aware of.

My program structure mostly follows the layout that was given to us (the network interface class and 4 methods to implement). I added some
private attributes to the NetworkInterface class for the ready-to-be-sent queue, the queue waiting for ARP replies, and the ARP table. I also
added some helper methods for creating ARPMessages and EthernetFrames. Since these operations happen multiple times I refactored the code to
make it cleaner. I also added a struct for ARP table entries which includes the MAC-address, the state of the entry, and its time to live.Generally speaking my code is structured as follows, 
	- In send_datagram: 
		- I check if the next_hop is in the ARP table, and if so, encapsulating it in an EthernetFrame and putting it in the ready queue
		- If next hop is not in the ARP table, then I add the datagram to the ARP waiting queue for next_hop. Then I check if an ARP request has ever been sent, and if not, then I add an entry in the ARP table, create an ARP request, and put it in the send queue
	- In recv_frame:
		- I first check if the frame is destined for this machine, if not, then stop immediately
		- Then I check the type of the frame
			- If it contains an IP packet, then I parse the payload of the frame and return the IP packet if the parsing succeeds
			- If it contains an ARPMessage, then I parse the payload of the frame. I then update the ARP table by adding or updating the
			sender's information in the corresponding ARPEntry. If the ARPMessage is a request then I create another ARPMessage to reply to the sender and add that to the ready queue. Finally I go through the ARP waiting queue and check if there are any packets waiting for the IP mapping
			we just learned, and if so, I add them all to the ready queue by calling send_datagram on them.
	- In tick:
		- I simply iterate through each entry in the ARP table and subtract the time that has passed from each entry's TTL.
		- After updating the TTL, if the entry is now expired then I delete it from the ARP table. Also if the entry was waiting for an
		ARP response then I remove the entry in the ARP waiting queue (which deletes all of the packets waiting for that ARP reply).
	- In maybe_send:
		- I simply check that the ready queue is not empty, in which case, I remove the first element and return it.
		- If there is nothing in the ready queue, then I return nothing.

Something I would like to note is I implemented the queue of packets waiting for ARP replies as an unordered map from IP address to a queue of InternetDatagrams. I decided to call it arp_queue_ which may cause some confusion since it is actually a map with the queues as values and the keys the IP address for which those packets are waiting for a reply from. Also note that I left them as InternetDatagrams because we don't have the destination MAC address yet so we can't encapsulate them in EthernetFrames yet.

Implementation Challenges:
// Describe any issues you encountered that might have had an impact on the performance of your solution, but you managed to address. 

My biggest challenge in this project was learning C++ since I have no prior experience with the language besides my knowledge of C. The biggest challenge I had with this was the sheer amount of C++ code and tools I had to read/look up in order to figure out what I needed to use in my
implementation. Also, my lack of C++ knowledge caused me issues when I was getting compiler errors because I didn't know what a lot of them meant so they were hard for me to address. I managed to address this by looking at some documentation for C++ and asking ChatGPT about C++ concepts that I 
was unsure about. I know this challenge doesn't have any direct performance issues (besides mistakes I made because I am unfamiliar with the language), but it was my biggest struggle I managed to address. 

Remaining Bugs:
// Describe any remaining bugs/issues that still remain in your code.

One issue is that I was going to use getter and setter methods for the private attributes of the NetworkInterface class so that I could 
better adhere to encapsulation principles, but this caused me a lot of compiler errors so I decided to take these methods
out and directly use the attributes. Since I am the only one working with the attributes, this is okay, but in the future I should
add getter and setter methods for each attribute we need to use.