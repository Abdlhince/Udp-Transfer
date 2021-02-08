/*
* FILE : Main.cpp
* PROJECT :  Assignment1
* PROGRAMMER : Abdullah Ince & Daniel Meyer
* FIRST VERSION : Feb 07 2021
* DESCRIPTION : This project implements a program to transfer binary and ASCII files using the reliable UDP protocol using the sample code provided.
*/

#define DEBUG2
#ifdef DEBUG2

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>

#include "Net.h"

// calculates the CRC 
extern unsigned long CalculateBufferCRC(unsigned int count, unsigned long crc, void* buffer);

// call the function in order to create crc table
extern void BuildCRCTable(void);


using namespace std;
using namespace net;

const int serverPort = 30001;		  // Server  Port
const int clientPort = 30002;			  // Clien Port
const int ProtocolId = 0x11223344;	  // unique ID for protocol
const float DeltaTime = 1.0f / 30.0f; //  delta time
const float sendRate = 1.0f / 30.0f;  //  send rate
const float connectTimeOut = 100.0f;
const int packetSize = 256;		  //  packet size



int getServerIP(char* IP);
char* getFName(char* f_path);
void showsResult(char* rslt, double time, double speed, int size_of_file, bool crrpt);

int writeFile(char* fName, int byteToWrite, char* fContent);
void fileRead(char* fName, char* fContent);
void packetPrs(char* packet, char* fName, char* size_of_file, unsigned long* srcCRC);

int main(int argc, char* argv[])
{
	bool Connected = false; // check the status if connect or not
	float sendAccumulator = 0.0f;
	FlowControl flowControl;

	struct stat in_file_Info;
	unsigned char* content_of_file = NULL;
	unsigned char* dataFile = NULL;
	int minBytTrns = 10000; // minumun size for sending file
	int fileSize = 0;			 // variable for checking size of the file
	int bytes_read = 0;			 // the count of bytes to read from the file
	int real_data = 0;				 // actual size for the file
	char mySizeOfFile[12] = {};
	char strcrc[25] = {};
	unsigned long srcCRC = 0;
	unsigned long destCRC = 0;
	bool contentEnable = false; // flag variable check the status if get the size of the file to malloc the str
	bool sendFinish = false;	  // check the status if client finished sending or not
	bool IPDisply = false;	  // flag variable check the status if the display or not
	unsigned long src_crc = 0;


	enum Mode
	{
		Client,
		Server
	};
	Mode mode = Server;

	Address address;


	char fname[81];
	char IP[BUFSIZ];
	bool testCRC = false;

	if (argc != 1 && argc != 3 && argc != 4)
	{

		printf("127.0.0.1 C:\example.txt [crc]\n");
		return -1;
	}
	if (argc >= 3)
	{
		int a, b, c, d;
		if (sscanf(argv[1], "%d.%d.%d.%d", &a, &b, &c, &d))
		{
			mode = Client;
			address = Address(a, b, c, d, serverPort);
		}

		strcpy(fname, argv[2]);

		if (argc == 4)
		{
			if (strncmp(argv[3], "crc", strlen(argv[3])) == 0)
			{
				// CRC test
				testCRC = true;
			}
		}
	}

	// check errors to ensure that the socket is  valid socket
	if (!InitializeSockets())
	{
		printf("[ERROR] Failed to initialize sockets...\n");
		return 1;
	}


	ReliableConnection connection(ProtocolId, connectTimeOut);

	const int port = mode == Server ? serverPort : clientPort;

	// check ports for connection
	if (!connection.Start(port))
	{
		printf("[ERROR] Could not start connection on port %d\n", port);
		return 1;
	}

	if (mode == Client)
		connection.Connect(address);
	else
		connection.Listen();


	// build crc table
	BuildCRCTable();

	//
	while (true)
	{
		// make sure connection is start
		// update flow control
		if (connection.IsConnected())
			flowControl.Update(DeltaTime, connection.GetReliabilitySystem().GetRoundTripTime() * 1000.0f);

		const float sendRate = flowControl.GetSendRate();

		// detect changes in connection state
		if (mode == Server && Connected && !connection.IsConnected())
		{
			flowControl.Reset();
			printf("[INFO] Reset flow control\n");
			Connected = false;
		}

		if (!Connected && connection.IsConnected())
		{
			printf("[INFO] Client connected to Server...\n");
			Connected = true;
		}

		if (!Connected && connection.ConnectFailed())
		{
			printf("[ERROR] Connection failed\n");
			break;
		}


		// INCREASE the send times, to set mode penalty

		sendAccumulator += DeltaTime;

		// CLIENT SIDE

		if (mode == Client)
		{
			unsigned char clientGreetingPacket[packetSize] = { 0 };
			unsigned char serverGreetingPacket[packetSize] = { 0 };

			// get size of file 
			if (contentEnable == false)
			{
				stat(fname, &in_file_Info);
				fileSize = in_file_Info.st_size;
				content_of_file = (unsigned char*)malloc(fileSize + 1);
				fileRead(fname, (char*)content_of_file);

				// Calculate CRC of the source file
				src_crc = CalculateBufferCRC(fileSize, src_crc, content_of_file);
				contentEnable = true;
			}

			// WHEN ACCUMULATOR SEND IS AT A GOOD MODE, WE START SENDING TO SERVER
			while (sendAccumulator > 1.0f / sendRate)
			{

				int dSize = 0;
				int expected = fileSize;
				int byteToSend = minBytTrns;
				bool acked = false;
				int i = 0;
				int length = strlen((char*)content_of_file);


				while (dSize != expected)
				{
					if (dSize == 0)
					{
						strcat(fname, "/");
						sprintf(mySizeOfFile, "%d", fileSize);
						strcat(fname, mySizeOfFile);
						strcat(fname, "/");
						sprintf(strcrc, "%lu", src_crc);
						strcat(fname, strcrc);
						strcat(fname, "/");
						connection.SendPacket((unsigned char*)fname, strlen(fname));
						if (testCRC)
						{
							// corrupt the first byte of the data
							content_of_file[0] -= 1;
						}
					}
					connection.SendPacket(content_of_file + dSize, byteToSend);
					while (1)
					{
						unsigned char isAckedMSG[100] = { 0 };
						connection.ReceivePacket(isAckedMSG, sizeof(isAckedMSG));
						if (strcmp((char*)isAckedMSG, "acked") == 0)
						{
							dSize = byteToSend + dSize;
							byteToSend = expected - dSize;
							if (byteToSend >= minBytTrns)
							{
								byteToSend = minBytTrns;
							}
							i = 0;
							break;
						}
						i++;
						if (i > 1000000)
						{
							i = 0;
							break;
						}
					}
				}
				sendFinish = true;
				sendAccumulator -= 1.0f / sendRate;
			}
		}


		if (sendFinish == true)
		{
			printf("[Info] Client sent the file to server...\n");
			free(content_of_file);
			break;
		}


		// server side

		if (mode == Server)
		{
			int totalBytRead = 0;

			// DISPLAY IP
			if (IPDisply == false)
			{
				getServerIP(IP);
				printf("\t =================== Server Side ======================\n");
				printf("Server IP Address:     \t%s\n", IP);

				IPDisply = true;
			}


			unsigned char* packet = NULL;

			packet = (unsigned char*)malloc((long long)minBytTrns + 100);

			// create time 
			LARGE_INTEGER frequency; // ticks per second
			LARGE_INTEGER t1, t2;	 // ticks
			bool clockStart = false;
			int firstReadByt = 0;

			while (1)
			{
				//strcpy((char *)packet, "");


				bytes_read = connection.ReceivePacket(packet, minBytTrns);
				if ((connection.GetReliabilitySystem().GetRemoteSequence() == (connection.GetReliabilitySystem().GetReceivedPackets() - 1)) && bytes_read > 0)
				{
					// clock start
					if (clockStart == false)
					{
						QueryPerformanceFrequency(&frequency);
						QueryPerformanceCounter(&t1);
						clockStart = true;
					}
					connection.SendPacket((unsigned char*)"acked", strlen("acked"));

					// If the local sequence is 1, it means server got the first msg from client, which is the filename
					if (connection.GetReliabilitySystem().GetLocalSequence() == 1)
					{
						memset(fname, 0, sizeof(fname));
						packetPrs((char*)packet, fname, mySizeOfFile, &srcCRC);
						remove(fname); //delete file if exists
						firstReadByt = totalBytRead;
					}
					else
					{
						writeFile(fname, bytes_read, (char*)packet);
						totalBytRead = totalBytRead + bytes_read;


						if (totalBytRead >= atoi(mySizeOfFile) && totalBytRead != 0)
						{
							QueryPerformanceCounter(&t2);

							dataFile = (unsigned char*)malloc((long long)totalBytRead + 1);
							fileRead(fname, (char*)dataFile);

							// Calculate CRC of the received file
							destCRC = CalculateBufferCRC(totalBytRead, destCRC, dataFile);
							free(dataFile);

							break;
						}
					}
				}
			}
			free(packet);

			// RESET THE RELIABILITY SYSTEM AND PRINT RESULT
			if (totalBytRead >= atoi(mySizeOfFile) && totalBytRead != 0)
			{
				connection.GetReliabilitySystem().Reset();
				printf("[Info] SERVER saved all data read to file [%s].\n", fname);

				double TIME_SEND = (double)((t2.QuadPart - t1.QuadPart) * 1000 / frequency.QuadPart);             //  mili second
				double SPEED = (double)(((long long)totalBytRead * 8) / (TIME_SEND * 1000));					  // megabit per second
				unsigned char result[1000] = { 0 };
				bool corrupt = (srcCRC != destCRC);
				printf("srccrc=%lu === detcrc=%lu\n", srcCRC, destCRC);
				showsResult((char*)result, TIME_SEND, SPEED, totalBytRead, corrupt);
				printf("%s\n", result);
			}
		}

		// update connection
		connection.Update(DeltaTime);
		net::wait(DeltaTime);
	}
	ShutdownSockets();
	//free(fileContent);
	return 0;
}

#endif // DEBUG2

/*
Function: packetPrs
Parameter: char * packet, char *fileName, char *sizeOfFile, unsigned long* srcCRC
Return Value: None
Description: This function is used to parse the contents of an incoming packet.
*/
void packetPrs(char* packet, char* fName, char* sizeOfFile, unsigned long* srcCRC)
{
	int count = 0;
	int i = 0;
	int j = 0;
	char crc[20] = { 0 };
	char temFName[500] = { 0 };

	for (i = 0; i < strlen(packet); i++)
	{
		if (packet[i] != '/')
		{
			temFName[i] = packet[i];
		}
		else
		{
			break;
		}
	}
	strcpy(fName, getFName(temFName));

	for (j = i + 1; packet[j] != '/'; j++)
	{
		sizeOfFile[count] = packet[j];
		count++;
	}

	count = 0;
	for (int k = j + 1; packet[k] != '/'; k++)
	{
		crc[count] = packet[k];
		count++;
	}
	*srcCRC = strtoul(crc, NULL, 10);
}

/*
Function: fileRead
Parameter: char* fName, char* fileContent
Return Value: None
Description: This function is used to read the content of the binary file.
*/
void fileRead(char* fName, char* fileContent)
{
	FILE* fpRead = NULL;
	// // READ THE CONTENT OF THE BINARY FILE.
	fpRead = fopen(fName, "rb");
	if (fpRead == NULL)
	{
		printf("[Info] CLIENT: CANNOT OPEN THE FILE...\n");
		return;
	}

	// READ THE CONTENT OF THE FILE AND PUT TO fileContent
	bool endOfFile = feof(fpRead);
	char nextChar;
	unsigned int i;
	for (i = 0; !endOfFile; i++)
	{
		nextChar = getc(fpRead);
		if (feof(fpRead))
		{
			endOfFile = true;
			i--;
		}
		else
		{
			fileContent[i] = nextChar;
		}
	}
	fileContent[i] = '\0';

	fclose(fpRead);
}

/*
Function: writeFile
Parameter: char *fName, int byteToWrite, char *content
Return Value: int
Description: This function is used to read an item in a file by a certain size and write it to another file.
*/
int writeFile(char* fName, int byteToWrite, char* content)
{
	FILE* fOutput = NULL;
	fOutput = fopen(fName, "ab+");
	if (fOutput == NULL)
	{
		printf("[ERROR] Cannot write to file...\n");
		return -2;
	}

	unsigned int ret;
	for (ret = 0; ret < byteToWrite; ++ret)
	{
		fputc(content[ret], fOutput);
	}
	fclose(fOutput);

	return ret;
}


/*
Function: showsResult
Parameter: char *result, double time, double speed, int filesize, bool corrupt
Return Value: None
Description: This function is used to analyze the transmitted file and display the file size, the time transferred, and the transmission speed.
*/
void showsResult(char* result, double time, double speed, int filesize, bool corrupt)
{
	sprintf(result, "File size: %d bytes. Time transfer: %.2lf milisecond. Speed: %.2lf megabits/second\n\tThere is %s corruption in the received file", filesize, time, speed, corrupt ? "data" : "no");
}

/*
Function: getFName
Parameter: char* filePath
Return Value: char *
Description: This function takes the user input from command argument and determine whether the input contains path and extension and parse the input to get the filename
*/
char* getFName(char* filePath)
{
	string filename = string(filePath);
	// Remove directory if present.
	// Do this before extension removal incase directory has a period character.
	const size_t last_slash_idx = filename.find_last_of("\\/");
	if (std::string::npos != last_slash_idx)
	{
		filename.erase(0, last_slash_idx + 1);
	}

	char ret[100] = { 0 };
	strcpy(ret, filename.c_str());
	return ret;
}

/*
Function: getServerIP
Parameter: char *ip
Return Value: return 1 when it is successful, otherwise 0
Description: This function is used to get local host ip with hostname.
*/
int getServerIP(char* ip)
{
	unsigned int addr;
	struct hostent* hp;
	char* localip = (char*)"127.0.0.1";
	char buf[512];

	gethostname(buf, 512);
	addr = inet_addr(buf);
	if (addr == 0xFFFFFFFF)
	{
		hp = gethostbyname(buf);
		if (hp == NULL)
			return 0;
		addr = *((unsigned int*)hp->h_addr_list[0]);
	}
	else
	{
		hp = gethostbyaddr((char*)&addr, 4, AF_INET);
		if (hp == NULL)
			return 0;
		addr = *((unsigned int*)hp->h_addr_list[0]);
	}

#ifdef LITTLEENDIAN
	sprintf(ip, "%d.%d.%d.%d",
		(addr & 0xFF),
		((addr >> 8) & 0xFF),
		((addr >> 16) & 0xFF),
		((addr >> 24) & 0xFF));
#else
	sprintf(ip, "%d.%d.%d.%d",
		((addr >> 24) & 0xFF),
		((addr >> 16) & 0xFF),
		((addr >> 8) & 0xFF),
		(addr & 0xFF));
#endif

	return 1;
}


