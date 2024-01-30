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

struct Packet {
    uint32_t seq; 
    uint32_t chk;
    uint32_t len;
    vector<uint8_t> raw;
};

uint32_t convertSEQtoBigEndian(uint32_t value) {
	return ((value >> 24) & 0xFF) | ((value >> 8) & 0xFF00) | ((value << 8) & 0xFF0000) | (value << 24);
}

void extractingProcessedData(vector<Packet> satellitePacket) {
	for(int i = 0; i < satellitePacket.size(); i++){
		uint32_t seq = convertSEQtoBigEndian(satellitePacket[i].seq);
		int size = (satellitePacket[i].raw).size();
		vector<uint8_t> raw;
		raw.resize(size);
		raw = satellitePacket[i].raw;
		uint32_t checksum = seq;
		
		// Calculated CHK between SEQ and RAW at i-pos in vector<Packet>
		for(int j = 0; j < sizeof(satellitePacket[i]); j++){
			uint32_t chunk = 0; 
			for(int k = 0; k < 4 && (k + j) < raw.size(); k++){
				chunk |= raw[j + k] << (k * 8); 
			}
			checksum ^= chunk;
		}
		
		uint32_t givenCHK = satellitePacket[i].chk;
		
		ofstream processedData;
			processedData.open("sortedData.bin", ios::binary | ios::out);
			if(!processedData.is_open()) {
				cerr << "Failed to open file." << endl;
				break;
			}
			
			while(true) {
				if(givenCHK == checksum) {
					for(int h = 0; h < raw.size(); h++){
						processedData << raw[h];
					}
				}
				else {
					processedData.close();
				}
			}
	}
}

vector<Packet> extractingRawData(const string dataContents){
	 int headerSize = 3 * sizeof(uint32_t);
	 
	 vector<Packet> packets;
	 
	for (int i = 0; i < dataContents.size(); ) {
		Packet packet;
		 
		packet.seq = i;
		packet.chk = i + 1;
		packet.len = *reinterpret_cast<const uint32_t*>(dataContents.data() + i + 2);
	
		i += headerSize;
		
		packet.raw.resize(packet.len); // resize raw vector
		memcpy(packet.raw.data(), dataContents.data() + i, packet.len); //copy data from string to raw vector
		
		int remainder = packet.len % 4; // Check for remainder and perform padding
		if(remainder > 0) {
			int newSize = 0;
			packet.raw.resize(packet.len + remainder, 0xAB);
			newSize = packet.raw.size();

			uint32_t lastChunk = 0;
			for(int j = 0; j < 4; j++){
				lastChunk |= 0xAB << (j * 8);
			}
			packet.raw.push_back(lastChunk);
			i += newSize;
		}
		else {
			i += packet.len;
		}
		
		packets.push_back(packet);
	}
	
	return packets;
}


int main() {
    const int servPort = 2323;
    const string email = "angelinale021@gmail.com";

    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSock == -1) {
        perror("No socket created.");
        return -1;
    }

    struct hostent *serverHost;
    serverHost = gethostbyname("challenge.cantina.ai");
    if(serverHost == NULL) {
        herror("Cannot get host address.");
        exit(1);
    }

    char ipAddress[INET_ADDRSTRLEN];
    const char* serverIp = inet_ntop(AF_INET, (struct in_addr*)(serverHost->h_addr_list[0]), ipAddress, sizeof(ipAddress));

    // cout << serverIp << endl;

    struct sockaddr_in serverAddr;

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(servPort);

    if(inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0){
        cerr << "Invalid Server IP / Server IP not found." << endl;
        return 1;
    }

    int serverConnect = connect(clientSock, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if(serverConnect < 0) {
        cerr << "Connection Failed" << endl;
    }

    char serverPacket[256]; // WHORU:722887867
    int byteCount = recv(clientSock, serverPacket, sizeof(serverPacket), 0);
    if(byteCount == -1) {
        cerr << "Error Receiving Data." << endl;
        return 1;
    }

    serverPacket[byteCount] = '\0';
    // cout << serverPacket << endl;

    string challengeNum; 
    for(int i = 6; serverPacket[i] != '\n'; i++){
        challengeNum += serverPacket[i];
    }
    // cout << challengeNum << endl;


    string identifyPacket = "IAM:" + challengeNum + ":" + email + ":at\n";
    send(clientSock, identifyPacket.c_str(), identifyPacket.length(), 0);
    char buffer[1024];
    recv(clientSock, buffer, sizeof(buffer), 0); // SUCCESS:151536
    // cout << dataRead << endl;
    // cout << buffer << endl;

    // string dataContents;
    // cout << dataContents << endl;

    ofstream dataDump;
        dataDump.open("satData.bin", ios::binary | ios::out);
        if(!dataDump.is_open()) {
            cerr << "File is not open." << endl;
            return -1;
        };
        
        while(true) {
            char dataStream[4096];
            int dataBytes = read(clientSock, dataStream, sizeof(dataStream));
            if(dataBytes == 0) {
                perror("Connection closed.");
                break;
            } else if(dataBytes < 0) {
                cerr << "No data found." << endl;
                break;
            } else {
                buffer[dataBytes] = '\0';
                dataDump.write(dataStream, dataBytes);
            }
        };

        // while(true) {
        //     string dataStr;
        //     char ch;
        //     while(dataDump.get(ch)) {
        //         dataStr += ch;
        //     }
        // }

    close(clientSock); 
    dataDump.close();

    ifstream dataContents;
        dataContents.open("satData.bin");
        if(!dataContents.is_open()) {
            cerr << "Cannot read file." <<  endl;
            return -1;
        }

        while(true) {
            string dataStr;
            char ch;
            while(dataContents.get(ch)) {
                dataStr += ch;
            }

            vector<Packet> splittingData = extractingRawData(dataStr);


            // cout << dataStr << endl;
        }
    
    dataContents.close();
    dataDump.close();

    return 0;
}