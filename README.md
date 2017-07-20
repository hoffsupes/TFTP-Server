# TFTP-Server


Simple TFTP Client
by Gaurav Dass
dassg@rpi.edu

This is a simple TFTP server. It is based on the RFC 1350. This was developed and tested on Ubuntu 16 and 14.04.

Approximate time to review the literature, study,implement and debug the above took about 1.5 weeks. 

If any problems while running the code, please feel free to contact me at dassg@rpi.edu. Instructions on how to use the program are output to stdout itself (along with the port number which is the first thing output).

Please find attached screenshots of the program while running in Ubuntu 14.04 Trusty Tahr. http://imgur.com/a/Ai67g

To use the program just run it from the terminal. To terminate it, press Ctrl + C or close the terminal. The program keeps refreshing stdout with each receipt of a request and handling it, but displays its port number through it all. Its usage instructions are output to stdout every time.
