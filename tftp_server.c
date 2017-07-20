////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ---------------------------------------------------------------------------------------------------------------------
// *******************************************************************************************************************//
// Simple TFTP Server 
// Made by Gaurav Dass
// dassg@rpi.edu
// *******************************************************************************************************************//
// ---------------------------------------------------------------------------------------------------------------------
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <stdlib.h>

//// VVVIMP:: Get port number output on the screen
//// Hit Ctrl + C or Exit the command window to exit server, otherwise server runs forever

void print_display_message(int no)                                                              // writes various instructions to stdout along with the chosen port number, have instructions on how to use and interpret program
{
    char a[200];
    int i;
    printf("\n Operating at port :: %d \n",no);
    for(i = 0; i < 100; i++) {printf("\n");}                                                    // since no clrscr() in standard C just flush stdout with a 100 newlines
    printf("\n Operating at port :: %d \n",no);
    printf("\n Simple TFTP File server\n Made by Gaurav Dass@dassg \n\n MAXIMUM FILESIZE ALLOWED 32 MB. \n WARNING:::::ATTEMPTS TO TRANSFER FILES GREATER THAN FILESIZE MAY LEAD TO INCOMPLETE TRANSACTIONS, SCROLL UP TO SEE ERROR / STATUS MESSAGES IF ANY\n OPERATING AT PORT::::: %d ::::: \n PRESS [Ctrl + C] OR CLOSE COMMAND WINDOW TO EXIT",no);
    printf("\n\n Files may be transferred from the current directory from where the server is running from. For eg. Running the server from the /home/user/W/ folder would cause all files transferred to by 'put' to be put in there and all files transferred via 'get' to be taken from there. \n To specify a directory to which you want to transfer your file to mention the /path/file.extension with your client 'get' or 'put' command \n");
    printf("\n\n Usage: \n\n 1.) Compile and run the file 'tftp_server' \n\n 2.) run tftp client on linux, get it by[sudo apt install tftp-server && sudo apt install tftp] \n\n 3.) start command line Ctrl + T OR Ctrl + Alt + T \n\n 4.) enter the following commands \n\n 5.) tftp\n\n 6.) mode binary \n\n 7.) verbose\n\n 8.) connect localhost <port-no> '<--- port no can be set by list_port in main()' AND CAN BE SEEN BELOW\n\n 9.) get <filename(s)> 'choose filen from the directory you're running the server program from OR give a short path to the filename of the file you want the server to send you'\n\n 10.) put <filename(s)> '<--- to put files from directory WHERE YOU RAN TFTP client FROM to the directory where the server is running from OR a short path to where you want the server to put the file in'");
    sprintf((char *)a, "\n\nListening on port %d ... \n",no);                                   // giant pile of messages
    printf("%s",a);
    
}

void getRQprop(char * buf,struct sockaddr_in cli, int * TID, char * m, char * filen)            // for processing any Read / Write Packets [helps in extracting various parameters related to the packet] RRQ/WRQ PAK format :   opcode::filename::0::mode::0
{
 
    *(TID) = ntohs (cli.sin_port);                                                              // get transfer ID from the client
    
    buf += 2;                                                                                   // buf:pointer to packet hence, moving two bytes (two memory locations in char) as they contain the opcode which has already been extracted
    
    char *p;
    int i = 0;
    for(p = buf; *(p) != 0; p++)                                                                 // processing the filename until the value of the packet becomes zero, not '\0' but a numeric zero
    {
        strcat(filen,p);
        i++;
    }
    
    filen[i] = '\0';                                                                             // setting the last bit manually or no bounds can wreak havoc
    p++;
    i = 0;
    
    for(; *(p) != 0; p++)                                                                        // processing the mode after the filename
    {                                                                                            
        strcat(m,p);
        i++;
    }
    m[i] = '\0';                                                                                 // setting last bit manually again
    
    printf("\n mode:: %s filename:: %s \n",m,filen);                                             // printed confirmation of the packet
    
}

int getACKpack(char * ackpak,int pkno)                                                          // generate an ACK packet,format:: opcode::blockID
{
    int tbuf = sprintf(ackpak,"%c%c%c%c",0x00,0x04,0x00,0x00);                                  // sprintf to manually input the opcode in network byte order
    short P = htons(pkno);                                                                      // convert packet number [block ID] into network byte order
    memcpy(ackpak + 2,&P,2);                                                                    // use memcpy to directly copy it into last two bytes of the packet
    
    return(tbuf);                                                                               // contains the number of bytes to be sent over the network
    
}

int getDATpack(char * datpak, int pkno, char *file_w, int rbuf)                                 // generate a DAT packet, format:: opcode::blockID::Data    inputs are the block ID, file fragment and size of the file fragment(as simply using sizeof/strlen in this function calculates only the length of the pointer rather than the original char variable)
{
    int tbuf = sprintf(datpak,"%c%c%c%c",0x00,0x03,0x00,0x00);                                  // sprintf to give data in network byte order into the packet directly and also get the exact number of bytes to be sent
    short P = htons(pkno);                                                                      // convert packet number into network byte order                                                        
    memcpy(datpak + 2,&P,2);                                                                    // copy converted value into datapak acc to correct location
    memcpy((char *) datpak + 4,file_w,rbuf);                                                    // copy file fragment into packet

    return(tbuf + rbuf);                                                                        // total length of packet becomes length of file fragments + 4 bytes (due to opcode and block ID)
}

void getpackvalues(char * pak, int * opcode, int * clino )                                      // used for processing the packet values, inputs: pointers to the packet, opcode and block ID
{
    memcpy(opcode,pak,2);                                                                       // opcode value obtained from *(pak + 0) or the 0th position(1st byte onwards) in the packet
    memcpy(clino,pak+2,2);                                                                      // block ID obtained from the 3rd byte onwards in the packet
    (*opcode) = ntohs((*opcode));                                                               // conversion from network to host order for both opcode and block ID
    (*clino) = ntohs((*clino));
    
}

struct sockaddr_in do_socket_stuff_UDP( int * a, int * port)                                    // used to create a UDP socket
{
    int fd; 
    if ( (fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0 )                                  // socket created
    {
        printf( "\n Socket Initialization falied!!! \n" );
        exit(1);
    }

    struct sockaddr_in tftpserv;                                                                // create and initialize a socket address structure
    int ad_len = sizeof(tftpserv);
    memset( &tftpserv, 0, sizeof(tftpserv) );
    tftpserv.sin_family = AF_INET;
    tftpserv.sin_port = htons( 0 );
    tftpserv.sin_addr.s_addr = htonl( INADDR_ANY );

    if ( bind(fd, (struct sockaddr *)&tftpserv, sizeof(tftpserv)) < 0 )                         // bind created socket to the address structure
    {
        printf( "\n Socket Binding Falied!!! \n" );
        exit(1);
    }
    
   if(getsockname(fd,(struct sockaddr *)&tftpserv,&ad_len) < 0)                                 // to get the actual port number assigned to it by the system
    {
        printf("\n getsockname() failed!!! \n");
        exit(1);
    }
    
    (*a) = fd;                                                                                  // the file descriptor so that it may be returned 
    (*port) = ntohs(tftpserv.sin_port);                                                         // convert that port number from network to host order
    return tftpserv;
}

void ding_dong()                                                                                // alarm return / interrupt function (called every time SIGALRM interrupts something)
{   // no return value as this function should be as empty as possible
}

void tftpread_dat(char * file_n,char * mode,int tid,struct sockaddr_in client)         // TFTP server in read mode, inputs: filename, mode, client TID, client socket address structure and file_n in local server directory where data will be fetched from, this is specified in main()
{
    
    struct sockaddr_in ACK;                                                                         // Various declarations, important ones include ackpacket, datpacket 
    FILE *filep;                                                                                    // file_n MAXWAIT and opcode
    char file_w[515],datpak[517],ackpak[517];
    char *servbuf;
    int ack_len,opcode;
    int i,n;
    int clino,pkno = 0,tbuf;
    int MAXWAIT = 10,rbuf = 0,list_fd,tfl = 0;
    extern int errno;

    signal(SIGALRM,ding_dong);                                                                      // initialized the signal with SIGALRM, and its interrupt function
    siginterrupt(SIGALRM, 1);                                                                       // have to active this otherwise SIGALRM never interrupts
    
    if((list_fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0)                                      // create a datagram socket (to talk to a specific client with)
    {
        printf("Could not create socket for client\n");
        return;
    }

    filep = fopen(file_n,"r");                                                                        // attempt to open the file

    if(filep == NULL)                                                                               // if errors in opening file
    {
        if(errno == ENOENT)                                                                         // If file does not exist send error code
        {
            tbuf = sprintf((char *) datpak,"%c%c%c%cNo such file in %s%c",0x00,0x05,0x00,0x01,file_n,0x00);

            if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
            {
                printf("\n Problem sending error packet!!\n");
            }
        }
        else if (errno == EACCES)                                                                   // If no permissions to access file send error code
        {
            tbuf = sprintf((char *) datpak,"%c%c%c%cYou do not have permission to access file in %s%c",0x00,0x05,0x00,0x02,file_n,0x00);

            if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
            {
            printf("\n Problem sending error packet!!\n");
            }
        }
        else                                                                                        // unknown file error, since filepointer null cannot proceed STOP
        {
            tbuf = sprintf((char *) datpak,"%c%c%c%cUnknown file error in %s%c",0x00,0x05,0x00,0x00,file_n,0x00);

            if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
            {
            printf("\n Problem sending error packet!!\n");
            }
            
        }
        
        return;
    }
    
    if(strstr(mode,"octet") == NULL)                                                               // strictly operating in octet mode
    {
        tbuf = sprintf((char *) datpak,"%c%c%c%cONLY OCTET MODE IS SUPPORTED NOT %s%c",0x00,0x05,0x00,0x04,mode,0x00);
        
        if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf);
        {
            printf("\nProblem sending error packet!!!\n");
        }
        printf("NOT OCTET MODE!!!\n");
        return;
    }
    
    
    
    while(1)
    {
        memset(file_w,0,sizeof(file_w));                                                             // file fragment reader, flushed to zero
        rbuf = fread(file_w,1,512,filep);                                                           // read 512 bytes from the file
        memset(datpak,0,sizeof(datpak));                                                            // set both DAT and ACK packets to 0 value
        memset(ackpak,0,sizeof(ackpak));
    
        if(rbuf != 512) { tfl = 1; pkno++; printf("\nLast Packet or fread error, either way closing connection\n"); break; }// last packet received tfl set break proceed to normal 
                                                                                                                            // termination
                                                                                                                            // last packet found when file readsize is less than 512
        pkno++;                 // count starts from zero hence incrementing follows numbers linearly                       // as fread may also have reached eof
        tbuf = getDATpack(datpak,pkno, file_w,rbuf);                                                // get datapacket using block ID and file fragment and size of file read 
        
        if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client))!= tbuf)         // sends the data packet to the client
        {
            printf("\n Size of the packet to be sent is inconsistent!!\n");
        }
        
        for(i = 0; i < MAXWAIT; i++)                                                                // Timeout mechanism: Connection Aborted if MAXWAIT (10) consecutive 1 second timeouts occur
        {
            ack_len = sizeof(ACK);  // ACK: current packet returned add struct, client: client address structure, used as reference

            alarm(1);                                                                               // set alarm value to one second, after one second it interrupts recvfrom
            n = recvfrom(list_fd,ackpak,sizeof(ackpak),0,(struct sockaddr *) &ACK,(socklen_t *) & ack_len); // receive an ACK packet in response, address data stored in ACK, ackpak stores the received packet

            if(n<0 && errno == EINTR)                                                               // timeout gone by but packet not received, EINTR is set when the timeout occurs (of one second) and SIGALRM interrupts recvfrom
            {
                
                if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client))!= tbuf)
                {
                    printf("\n Size of the packet to be sent is inconsistent!!\n");
                }
                    
                continue;           
            }
            
            else if(n<0 && errno != EINTR)                                                          // timeout not gone but no message received, some error in recvfrom
            {
                printf("\nSome error in recvfrom\n");
            }
            
            else if(errno == EINTR)                                                                 // something has been received, is it the packet we want??
            {
                alarm(0);                                                                           // stop the alarm
                
                   
                    if(tid != ntohs(ACK.sin_port))                                                  // TID of the received packet compared with expected value, if no match error received
                    {   
                        printf("\nTID's packet does not match that of selected client!!\n");
                        tbuf = sprintf((char *) ackpak,"%c%c%c%cYou're talking to the wrong server bub(Wrong TID)%c",0x00,0x05,0x00,0x05,0x00);
                        
                        if(sendto(list_fd,ackpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf);
                        {
                            printf("\n Size of the packet to be sent is inconsistent!! \n");
                        }
                        i--;                                                                        // the timeout is a limit on the client itself but if the packet we just received was not from our supposed client at all ( clients from the same source but different ports/ TIDs count differently ) then it is not the clients fault and should not contribute towards its timeout
                        continue;
                    }

                getpackvalues(ackpak, &opcode, &clino);                                             // packet values extracted
                
                if(opcode != 4)                                                                     // opcode 4 is ACK packet, so far it is known that the packet is from the correct client, but is it the right kind og packet?
                {
                    printf("\n Packet opcode %d wrong OPCODE MUST BE 4\n",opcode);

                    tbuf = sprintf((char *) datpak,"%c%c%c%cPacket pkno does not match%c",0x00,0x05,0x00,0x04,0x00);

                    if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
                    {
                        printf("\nSize of the packet to be sent is inconsistent!! \n");
                    }

                }
                else if(clino != pkno)                                                              // if it is the right kind of packet, is it out of order?
                {
                    printf("\n Out of order packet!!! want %d got %d Ignoring packet\n",pkno,clino);
                }
                
                else
                {
                    
                            // only on receiving a succesful ACK do you break out of loop
                break;      // successful ACK received move onto next packet now
                    
                }
                

            }

        }

        if(i == MAXWAIT)                                                       // if client times out on last wait cycle then abort
        {                                                                                    
            printf("\n Timeout on ACK packet \n");                                          
            fclose(filep);
            shutdown(list_fd,2);
            return;
        }
    }       // end of while

    if(tfl == 1)                                                                            // Normal Termination : Size of data packet sent is 512 bytes
    {
       memset(datpak,0,sizeof(datpak));
       memset(ackpak,0,sizeof(ackpak)); 
       
        tbuf = getDATpack(datpak,pkno, file_w,rbuf);                                          // get datapacket for last packet
        
        if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client))!= tbuf)   // sends the data packet to the client
        {
            printf("\n Size of the packet to be sent is inconsistent!!\n");
        }
        
        for(i = 0; i < MAXWAIT; i++)
        {
            ack_len = sizeof(ACK);  // ACK: current packet returned add struct, client: client address structure, used as reference

            alarm(1);                                                                        // alarm set to one second delay, after one second it would interrupt recvfrom()
            n = recvfrom(list_fd,ackpak,sizeof(ackpak),0,(struct sockaddr *) &ACK,(socklen_t *) & ack_len); // receive an ACK packet in response, address data stored in ACK, ackpak stores the received packet

            if(n<0 && errno == EINTR)                                                        // timeout gone by but packet not received
            {                                                                                
                
                if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client))!= tbuf)
                {
                    printf("\n Size of the packet to be sent is inconsistent!!\n");
                }
                    
                continue;           
            }
            
            else if(n<0 && errno != EINTR)                                                  // some problem in recvfrom
            {
                printf("\nSome error in recvfrom\n");
            }
            
            else if(errno == EINTR)                                                         // something has been received
            {
                alarm(0);                                                                   // stop alarm because something has been received
                   
                    if(tid != ntohs(ACK.sin_port))                                          // is the packet having the right TID / is it from the right client?
                    {
                        printf("\nTID's packet does not match that of selected client!!\n");
                        tbuf = sprintf((char *) ackpak,"%c%c%c%cYou're talking to the wrong server bub(Wrong TID)%c",0x00,0x05,0x00,0x05,0x00);
                        
                        if(sendto(list_fd,ackpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf);
                        {
                            printf("\n Size of the packet to be sent is inconsistent!! \n");
                        }
                        i--;
                        continue;
                    }

                getpackvalues(ackpak, &opcode, &clino);                                     // get packet values 

                if(opcode != 4)                                                             // opcode 4 is ACK packet, is the packet of the correct type?
                {
                    printf("\n Packet opcode %d wrong OPCODE MUST BE 4\n",opcode);

                    tbuf = sprintf((char *) datpak,"%c%c%c%cPacket pkno does not match%c",0x00,0x05,0x00,0x04,0x00);

                    if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
                    {
                        printf("\nSize of the packet to be sent is inconsistent!! \n");
                    }

                }
                else if(clino != pkno)                                                      // is the packet out of order?
                {
                    printf("\n Out of order packet!!! want %d got %d Ignoring packet\n",pkno,clino);
                }
                
                else                                                                        // so the packet is from the correct client (correct TID), its of the correct type (correct opcode) and right order (correct blockID)
                {                                                                           // this received packet was the ACK to the final data packet, now the connection can be closed
                    printf("\nGOT FINAL ACK CLOSING CONNECTION GOODBYE!!\n");
                    fclose(filep);                                                          // close file pointer
                    shutdown(list_fd,2);                                                    // shutdown socket
                    return;
                }
                

            }

        }

        if(i == MAXWAIT)                                                                    // if timeout for 10 seconds then close connection, at least the file has been received in its entirety as this was the last ACK
        {
            printf("\n Timeout on ACK packet but as this is the last ACK you will be fine, the file should probably have been received!! \n");
            fclose(filep);
            shutdown(list_fd,2);
            return;
        }

        
    }
    
    fclose(filep);
    shutdown(list_fd,2);                                                                    // default termination statements
    printf("\nFILE TRANSFER COMPLETE GOODBYE!!\n");
}

void tftpwrite_dat(char * file_n, char * mode, int tid, struct sockaddr_in client)     // TFTP server in write mode, inputs: filename, mode, client TID, client socket address
{          

    struct sockaddr_in DAT;                                                                     // some important definitions made here
    FILE *filep;
    char file_w[515],datpak[517],ackpak[517];                                      // the filename, file fragment, datapack and ack packet
    char *servbuf;
    int dat_len,opcode;
    int i,n=516,tbuf,file_ex = 0;
    int clino, pkno = 0, MAXWAIT = 10,list_fd;
    extern int errno;
    struct stat filb;
    
    signal(SIGALRM,ding_dong);                                                                  // SIGALRM set up along with its interrupt function
    siginterrupt(SIGALRM, 1);                                                                   // siginterrupt setup along with SIGALRM
        
    if((list_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)                                // socket created
    {
       printf("\nError in creating a client socket CONNECTION COULD NOT BE ESTABLISHED!!\n"); 
       return;
    }
     
    if(stat(file_n,&filb) == 0)                                                                    // file already exists need not write it
    {
        tbuf = sprintf((char *) datpak,"%c%c%c%cFile %s Already Exists %c",0x00,0x05,0x00,0x06,file_n,0x00);

        if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
        {
        printf("\n Problem sending error packet!!\n");
        }
        return;
    }
        
    filep = fopen(file_n,"w");                                                                        // file opened in write mode
    
    if(filep == NULL)                                                                               // filepointer is NULL
    {
        if (errno == EACCES)                                                                        // no permission to access
        {
            tbuf = sprintf((char *) datpak,"%c%c%c%cYou do not have permission to access file in %s%c",0x00,0x05,0x00,0x02,file_n,0x00);

            if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
            {
            printf("\n Problem sending error packet!!\n");
            }
        }
        else if (errno == EDQUOT || errno == ENOMEM )                                               // ALLOCATION EXCEEDED OR DISK FULL
        {
            tbuf = sprintf((char *) datpak,"%c%c%c%cCannot write %s as allocation exceeded or disk full!! %c",0x00,0x05,0x00,0x03,file_n,0x00);

            if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
            {
            printf("\n Problem sending error packet!!\n");
            }
            
        }
        else if(errno == EEXIST)                                                                     // File already exists, sometimes this flag may miss being called, hence stat() before
        {
            tbuf = sprintf((char *) datpak,"%c%c%c%cFile %s Already Exists %c",0x00,0x05,0x00,0x06,file_n,0x00);

            if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
            {
            printf("\n Problem sending error packet!!\n");
            }
            
        }
        
        else                                                                                         // Some file error
        {
            tbuf = sprintf((char *) datpak,"%c%c%c%cUnknown file error in %s%c",0x00,0x05,0x00,0x00,file_n,0x00);

            if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
            {
            printf("\n Problem sending error packet!!\n");
            }
            
        }
        
        return;
    }
    
    if(strstr(mode,"octet") == NULL)                                                              // strictly operating in octet mode
    {
        tbuf = sprintf((char *) datpak,"%c%c%c%cONLY OCTET MODE IS SUPPORTED NOT %s%c",0x00,0x05,0x00,0x04,mode,0x00);
        
        if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf);
        {
            printf("\nProblem sending error packet!!!\n");
        }
        printf("NOT OCTET MODE!!!\n");
        return;
    }
    
    while(1)
    {
        memset(file_w,0,sizeof(file_w));                                                                  // flushing file fragment traversor with zeroes
        memset(datpak,0,sizeof(datpak));
        memset(ackpak,0,sizeof(ackpak));                                                              // ackpak and datpak initialized with zeroes for this iteration
        
        tbuf = getACKpack(ackpak,pkno);
        
        if(sendto(list_fd,ackpak,tbuf,0,(struct sockaddr *)&client,sizeof(client))!=tbuf)              // send ACK packet
        {
            printf("\nError in sending, Resending packet!!\n");                                
            continue;   // free resending in case of erraneous sending
        }
        
        pkno++;                     // after sending this ACK expecting packet number pkno + 1
        dat_len = sizeof(DAT);
        
        for(i = 0; i < MAXWAIT; i++)                                                                    // client timeout is 1s, if MAXWAIT instances occur continiously then connection aborted
        {
            alarm(1);                                                                                   // alarm set SIGALRM interrupts every 1 second to see if anything has been obtained in recvfrom
             n = recvfrom(list_fd,datpak,sizeof(datpak),0,(struct sockaddr *) &DAT,(socklen_t *) & dat_len);
                        
            if(n<0 && errno == EINTR) // timeout yet no data packet, Data packed not received hence need to resend ACK 
            {
                
                if(sendto(list_fd,ackpak,tbuf,0,(struct sockaddr *)&client,sizeof(client))!=tbuf)           // resend ACK
                {
                    printf("\nError in sending Resending packet!!\n");                                                    
                } //resending
               continue;
            }
            
            else if(n<0 && errno != EINTR)
            {
                printf("\nSome error in recvfrom\n");
            }
            
            else if (errno == EINTR) // timeout with something, some packet has been received but dont know what?
            {
                alarm(0);                                                                                    // alarm stopped
                                
                if(tid != ntohs(DAT.sin_port))                                                               // packet from correct client? (TID ok?)
                {
                    printf("\nTID's packet does not match that of selected client!!\n");
                    tbuf = sprintf((char *) datpak,"%c%c%c%cYou're talking to the wrong server bub(Wrong TID)%c",0x00,0x05,0x00,0x05,0x00);
                    
                    if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf);
                    {
                        printf("\n Size of the packet to be sent is inconsistent!! \n");
                    }
                    i--;                                                                                     // if from wrong client should not consume timeout 
                    continue;                                                                                // on real client continue
                }

                getpackvalues(datpak, &opcode, &clino);                                                      // get packet values from datapacket
                servbuf = datpak + 4;

                        
            if(opcode != 3)        // opcode 3 is data packet                                               // packet correct kind of packet?
            {                                                                                               // since the client sending this packet is a viable one
                printf("\n Packet opcode %d wrong OPCODE MUST BE 3\n",opcode);                              // timeout consumed

                tbuf = sprintf((char *) datpak,"%c%c%c%cPacket opcode does not match%c",0x00,0x05,0x00,0x04,0x00);

                if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
                {
                    printf("\nSize of the packet to be sent is inconsistent!! \n");
                }

            }
            else if(clino != pkno)                                                                           // packet out of order? Only in write mode, whenever there is 
            {                                                                                               // a duplicate Data packet there should be a retransmission of
                                                                                                            // the ACK message
                if(clino == pkno - 1)                           // in case of duplicate data packet rertansmit last ACK message (to avoid sorcerers apprentice bug)                                                                                       // no retransmission should be done in case of a duplicate     
                {                                                                                           // ACK message, this is the solution to the sorcerers 
                    if(sendto(list_fd,ackpak,tbuf,0,(struct sockaddr *)&client,sizeof(client))!=tbuf)       // apprentice bug
                    {
                        printf("\nError in sending Resending packet!!\n");                                                    
                    } //resending
                    
                                                                                                            // timeout consumed due to packet being from valid client
                }
                
            }
            
            else // succesful DATA PACKET RECEIVED AND CONFIRMED, this is a correct data packet, in order and from the correct client
            {
            
                if(n > 4)
                {
                    memcpy((char *) file_w, servbuf, n - 4);                                               // writing from the packet to the 
                    fwrite(file_w, 1, n - 4, filep);                                                       // writing the file fragment to file
                }
                else            
                {
                    memcpy((char *) file_w, servbuf, n);
                    fwrite(file_w, 1, n, filep);    
                }
                
                if (n < 516)                                                                               // if size of the Data packet < 516 LAST DATA PACKET
                {                                                                                           // NORMAL TERMINATION START
                    printf("\nLast Data packet received!!! Sending Final ACK!!\n");
                    
        int I;             
        memset(datpak,0,sizeof(datpak));                // send the ACK one last time
        memset(ackpak,0,sizeof(ackpak));
        
        tbuf = getACKpack(ackpak,pkno);
        
        if(sendto(list_fd,ackpak,tbuf,0,(struct sockaddr *)&client,sizeof(client))!=tbuf)                   // send ACK again
        {
            printf("\nError in sending, Resending packet!!\n");                                
            continue; 
        }
                
        for(I = 0; I < MAXWAIT; I++)                                                                        // Implement timeout procedure as before
        {
            alarm(1);
             n = recvfrom(list_fd,datpak,sizeof(datpak),0,(struct sockaddr *) &DAT,(socklen_t *) & dat_len);
                        
            if(n<0 && errno == EINTR) // timeout yet no data packet                                         // Logic flipped this time if no response in timeout
            {                                                                                           // assume client got packet hence can close connection
                    printf("\nLast Packet Received, Connection now closed, Have a Nice Day!!\n");
                    fclose(filep);
                    shutdown(list_fd,2);
                    return;
            }
            
            else if(n<0 && errno != EINTR)
            {
                printf("\nSome error in recvfrom\n");
            }
            
            else if (errno == EINTR) // timeout with something, what kind of packet is it?
            {
                alarm(0);
                                
                if(tid != ntohs(DAT.sin_port))                                              // TID correct? /  from correct client??
                {
                    printf("\nTID's packet does not match that of selected client!!\n");
                    tbuf = sprintf((char *) datpak,"%c%c%c%cYou're talking to the wrong server bub(Wrong TID)%c",0x00,0x05,0x00,0x05,0x00);
                    
                    if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf);
                    {
                        printf("\n Size of the packet to be sent is inconsistent!! \n");
                    }
                    I--;                                                                   // wrong client, no need to use timeout from current client
                    continue;
                }

                getpackvalues(datpak, &opcode, &clino);
                servbuf = datpak + 4;

                        
            if(opcode != 3)        // opcode 3 is data packet                               // correct type of packet? / opcode right?
            {
                printf("\n Packet opcode %d wrong OPCODE MUST BE 3\n",opcode);

                tbuf = sprintf((char *) datpak,"%c%c%c%cPacket pkno does not match%c",0x00,0x05,0x00,0x04,0x00);

                if(sendto(list_fd,datpak,tbuf,0,(struct sockaddr *) &client,sizeof(client)) != tbuf)
                {
                    printf("\nSize of the packet to be sent is inconsistent!! \n");
                }

            }
            else if(clino != pkno)                                                          // packet in order?
            {                                                                               // as last packet any requests for duplicate packets need not be taken care of
                // as last packet ignore requests for any duplicate packets                 // all data packets have been received at this point so its fine to ignore
            }                                                                               // duplicate packets
            
            else // succesful DATA PACKET RECEIVED AND CONFIRMED                            // datapacket confirmed, correct datapacket so client must really want
            {                                                                               // that confirmation ACK so give client that confirmation LASTACK
        
                if(sendto(list_fd,ackpak,tbuf,0,(struct sockaddr *)&client,sizeof(client))!=tbuf)
                {
                    printf("\nError in sending, Resending packet!!\n");                                
                    continue; 
                }
                
            }
                
            }
        }
             
             if(I == MAXWAIT)                                                              // timeout on connection close it (LAST PACKET)
             {
                 printf("\n Timeout obtained!! Closing connection!!\n");
                    fclose(filep);
                    shutdown(list_fd,2);
                    return;
             }
              
              
            fclose(filep);
            shutdown(list_fd,2);
            return;  
    
                    
                }
                
                break;
            }

            }
            
        }
                    
        if(i == MAXWAIT)                                                                  // timeout on connection close it (NORMAL OPERATION)
        {
            printf("\nServer Timeout. Closing connection!!!\n");
            fclose(filep);
            shutdown(list_fd,2);
            return;
        }
        
    } // end of while
    
    
shutdown(list_fd,2);    
fclose(filep);                                                                                  // close file and shutdown socket
printf("\nFILE TRANSFER COMPLETE GOODBYE\n");
}

void run_tftp_server()                                                                             // runs the tftp server, input is the path to the directory NF of the server
{       

int lisfd,list_port;
struct sockaddr_in serv = do_socket_stuff_UDP(&lisfd,&list_port);                                           // create a socket endpoint, bind it and get its address
struct sockaddr_in cli;
int no_cli = 0,n, TID;
char rqpak[9000];                                                                                        // create a common rqpak of 9000

while(1)
{   // while loop begin
   int cli_len = sizeof(cli);
   memset(rqpak,0,9000);                                                                                  
   char filen[256], m[15];                                                                            // initialize variables, clear common rqpak
   int opcode;
   
   print_display_message(list_port);                                                                        // print the wall of text along with port numbers

    n = recvfrom(lisfd,rqpak,9000,0,(struct sockaddr *) &cli,(socklen_t *) & cli_len);                     // listen for any WRQ / RRQ requests
    
    if(n < 0){printf("\n Some error in recvfrom resetting server!! \n");continue;}                          // handle case where some error in recvfrom
    
    memcpy(&opcode,rqpak,2);                                                                               // get the opcode
    opcode = ntohs(opcode);                                                                                 // convert from network to host order
    
    int p_id = fork();                                                                                      // fork, now program spilt into two processes, parent and child
                                                                                                            // child shares all of the code of the parent but will have zero 
    if(opcode == 1)                // Read Mode --- RRQ                                                     // value of pid
    {
        
        if(p_id == 0)                                                                                       // if in child
        {
            getRQprop(rqpak,cli, &TID, m, filen);                                                          // get parameter values
            tftpread_dat(filen,m,TID,cli);                                                                  // call tftpread, handle request
            print_display_message(list_port);                                                               // print display message for next iteration
            exit(1);                                                                                        // child exits
        }

        
    }
    
    else if(opcode == 2)        // Write Mode --- WRQ                           
    {    
        
        if(p_id == 0)                                                                                       // if in child
        {
            getRQprop(rqpak,cli, &TID, m, filen);                                                          // get parameter values
            tftpwrite_dat(filen,m,TID,cli);                                                                // call tftpwrite, handle request
            print_display_message(list_port);                                                               // print display message for next iteration
            exit(1);                                                                                        // child handles request and exits
        }
    
        
    }
    
    else                                                                                                    // packet type not a WRQ/RRQ so reset
    {
        printf("\nIncorrect kind of packet received!! \n Resetting!!! ");
        continue;
    }
        
    
    
}   

}          

int main()
{
    run_tftp_server();                                                                 // run tftp server 
    return 1;
}