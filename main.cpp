#include <iostream> 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <cstring>
#include <string>
#include <fstream>
#include <vector>

using namespace std;

// Client connection to already Established server
/* 
I. SYN
    1. Create Client socket connection
    2. Prep Server -> Structure to handling internet addresses
        i. Get IPv4 Address from host name 
            a. Can only process when it's in ddd.ddd.ddd.ddd form from IPv4, and x:x:x:x:x:x:x:x form for IPv6 -> need conversion from challenge.cantina.ai
        i. Assign server family (sin_family) to ipv4
        ii. Assign server port (sin_port) to given port
        iii. Converting server address from string to binary form if not already in binary form
    3. Connect to Server
    4. Handle the handshake package from Server
II. SYN + ACK
    1. Extracting needed information from the Server packet 
    2. Store the needed info in a string for Identification packet
    3. Send the Identification packet back to the server
III. ACK
    1. When the server finished identify the client from Identification packet which outout the success message
    2. Start downloading the data
IV. Processing data:
    1. Open a file called sortedData.bin to store the streaming data
    2. As the data coming in, perform XOR CHK and only stored the corrected data that's pass
        i. Setting the structure of the PacketHeader to identify the firts known 12 bytes
        ii. Received the data from server and check whether or not there are data
        iii. Setting SEQ, CHK and LEN in PacketHeader structure to be unsigned 32bits (4bytes) with the data coming in
        iv. Checking whether or not client pulled in all data since this is not an async recv in the while-loop
            a. Break out of loop when there's no more data present.
        v. Performing XOR CHK between SEQ and Data
            a. Padding (0xAB) needed when LEN % 4 != 0
        vi. When the calculated XOR CHK and the given CHK are the same, write to the file 
        v. Delete the datdBuffer when finish with the packet to free the memory

        Note: Then loop will keep going until there's no more corrected data to be added in

    3. Close the file to make sure all the data has been written in there
V. Close client socket when finish
 * */

// Structure of the header packet when receiving raw data
struct PacketHeader {
    uint32_t seq; 
    uint32_t chk;
    uint32_t len;
} header;

int main() {
    const int servPort = 2323;
    const string email = "angelinale021@gmail.com";

    // Create Client Socket
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSock == -1) {
        perror("No socket created.");
        return -1;
    }

    // Obtain IP address from "challenge.cantina.ai"
    struct hostent *serverHost;
    serverHost = gethostbyname("challenge.cantina.ai");
    if(serverHost == NULL) {
        herror("Cannot get host address.");
        exit(1);
    }

    char ipAddress[INET_ADDRSTRLEN];
    const char* serverIp = inet_ntop(AF_INET, (struct in_addr*)(serverHost->h_addr_list[0]), ipAddress, sizeof(ipAddress));

    struct sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET; //  Setting as IPv4
    serverAddr.sin_port = htons(servPort); // Port given

    // Converting txt address to bin form
    if(inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0){
        cerr << "Invalid Server IP / Server IP not found." << endl;
        return 1;
    }

    // Connect to server
    int serverConnect = connect(clientSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if(serverConnect < 0) {
        cerr << "Connection Failed" << endl;
    }

    // Read the handshake package from server
    char serverPacket[256]; 
    int byteCount = recv(clientSock, serverPacket, sizeof(serverPacket), 0);
    if(byteCount == -1) {
        cerr << "Error Receiving Data." << endl;
        return 1;
    }

    // NULL Termination
    serverPacket[byteCount] = '\0';
    
    // Extracting Challenge Number
    string challengeNum; 
    for(int i = 6; serverPacket[i] != '\n'; i++){
        challengeNum += serverPacket[i];
    }

    // Sending back to server Identification Packet
    string identifyPacket = "IAM:" + challengeNum + ":" + email + ":at\n";
    send(clientSock, identifyPacket.c_str(), identifyPacket.length(), 0);
    char buffer[1024];
    recv(clientSock, buffer, sizeof(buffer), 0); // SUCCESS 151536

    // Open file and processing incoming data
    ofstream ofile("sortedData.bin", ios::binary);
    while(true) {
        memset(&header, 0, sizeof(PacketHeader)); // Setting Packet Header
        int bytesReceived = recv(clientSock, reinterpret_cast<char*>(&header), sizeof(PacketHeader), 0); // Receiving data
        if(bytesReceived <= 0) {
            break; // stop receiving if no data
        }

        uint32_t seq = ntohl(header.seq);
        uint32_t chk = ntohl(header.chk);
        uint32_t len = ntohl(header.len);

        char* dataBuffer = new char[len];
        int totalDataBytesReceived = 0;

        // Since recv is not async, this while-loop makes sure all raw data are pulling in the the size of LEN 
        while(totalDataBytesReceived < len) {
            bytesReceived = recv(clientSock, dataBuffer + totalDataBytesReceived, len - totalDataBytesReceived, 0);
            if(bytesReceived <= 0) {
                break;
            }
            totalDataBytesReceived += bytesReceived;
        }

        // XOR CHK with big endian SEQ
        uint32_t calculatedChk = seq;
        for (size_t i = 0; i < len; i += 4) {
            uint32_t data = (len - i < 4) ? 0xABABABAB : *(reinterpret_cast<uint32_t*>(dataBuffer+i));
            calculatedChk ^= ntohl(data);
        }

        // Checksum is correct -> write ti file
        if (calculatedChk == chk) {
            ofile.write(dataBuffer, len);
        }

        delete[] dataBuffer;
    }

    ofile.close();
    close(clientSock);

    return 0;
}