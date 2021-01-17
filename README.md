# common-routers

This program find the longest common path(in terms of routers) from the host to servers that are present in the file input.txt.

The DNS module uses getaddrinfo() call to get the IP addresses of the servers. The routers are discovered by sending UDP packets of varying TTL. Raw sockets accept ICMP packets with the protocol as IPPROTO_ICMP to get the IP addresses of the routers corresponding to each domain. For I/O multiplexing, select() call is used.  
A **MAX_TTL(maimum number of hops to server) of 30** is considered. To make the path finidng more reliable, a retransmission of UDP packets for which the corresponding ICMP packets havent been received is done.

**Note: Only IPV4 functionality is present. A few minor additions will be required for IPV6 functionality**

To execute:  
$ gcc findLongestCommonPath.c  
$ sudo ./a.out <input-file>  
