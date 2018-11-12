
#define OUTPUT_DEBUG 1
#if OUTPUT_DEBUG
#define PRINT(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
#else
#define PRINT(fmt, ...) 
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <winsock2.h> 
#include <process.h>
#include <io.h>
#include <signal.h>
#pragma comment(lib, "ws2_32.lib")
#define VERSION                        "2.00"
#define TIMEOUT                        300
#define MAXSIZE                        20480
#define HOSTLEN                        40
#define CONNECTNUM                  1024

unsigned __stdcall transmitdata(LPVOID data);
void getctrlc(int j);
int create_socket();
int create_server(int sockfd, int port);

// define GLOBAL variable here
int maxfd = 0;
struct arg_t
{
	int fd;
	char **argv;
};

void usage(const char* prog)
{
	PRINT("lcx mod by Tristone,version :%s\r\n", VERSION);
	PRINT("usage:\r\n%s port1 port2\r\n", prog);
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		usage(argv[0]);
		return 1;
	}
	// Win Start Winsock.
	WSADATA wsadata;
	WSAStartup(MAKEWORD(1, 1), &wsadata);

	signal(SIGINT, &getctrlc);
	
	SOCKET sc = create_socket();
	create_server(sc, atoi(argv[2]));
	sockaddr_in scAddr;
	
	fd_set fdrset, rsettmp;
	FD_ZERO(&fdrset);
	FD_ZERO(&rsettmp);
	FD_SET(sc, &rsettmp);
	while (1)
	{
		fdrset = rsettmp;
		int ret = select(0, &fdrset, NULL, NULL, NULL);
		if ((ret < 0) && (errno != EINTR))
		{
			printf("[-] Select error on main():\r\n");
			char buf[100] = { 0 };
			strerror_s(buf, sizeof(buf), errno);
			printf("%s\r\n", buf);
			continue;
		}
		else if (ret == 0)
		{
			printf("[-] Socket time out.\r\n");
			continue;
		}

		int addrlen = sizeof(scAddr);
		if (FD_ISSET(sc, &fdrset))
		{
			PRINT("accept wait socks client\r\n");
			int acSc = accept(sc, (sockaddr *)&scAddr, &addrlen);
			if (acSc <= 0)
			{
				PRINT("accept socks client error: %ld\r\n", WSAGetLastError());
				continue;
			}
			arg_t* parg = new arg_t;
			parg->argv = argv;
			parg->fd = acSc;
			if (!_beginthreadex(NULL, 0, transmitdata, (LPVOID)parg, 0, NULL))
			{
				delete parg;
				PRINT("CreateThread on main error \r\n");
				break;
			}
		}
	}

	WSACleanup();
	//system("pause");
	return 0;
}

//************************************************************************************
// 
// Socket Transmit to Socket
//
//************************************************************************************
unsigned __stdcall transmitdata(LPVOID data)
{
	printf("[+] CreateThread to transmitdata OK!\r\n\n");

	arg_t *parg = (arg_t *)data;
	int fd1 = parg->fd;
	char **argv = parg->argv;
	delete parg;

	SOCKET slave = create_socket();
	create_server(slave, atoi(argv[1]));

	sockaddr_in slAddr;
	int addrlen = sizeof(slAddr);
	PRINT("accept port =  %d\r\n", atoi(argv[1]));
	int fd2 = accept(slave, (sockaddr *)&slAddr, &addrlen);
	if (fd2 <= 0)
	{
		PRINT("accept socks client error: %ld\r\n", WSAGetLastError());
		closesocket(fd1);
		closesocket(slave);
		return 1;
	}
	PRINT("accept a fd = %d on transmitdata\r\n", fd2);
	closesocket(slave);
	fd_set readfd, writefd;
	int result, i = 0;
	char read_in1[MAXSIZE], send_out1[MAXSIZE];
	char read_in2[MAXSIZE], send_out2[MAXSIZE];
	int read1 = 0, totalread1 = 0, send1 = 0;
	int read2 = 0, totalread2 = 0, send2 = 0;
	int sendcount1, sendcount2;
	int maxfd;
	struct sockaddr_in client1, client2;
	int structsize1, structsize2;
	char host1[20], host2[20];
	int port1 = 0, port2 = 0;
	char tmpbuf[100];

	memset(host1, 0, 20);
	memset(host2, 0, 20);
	memset(tmpbuf, 0, 100);

	structsize1 = sizeof(struct sockaddr);
	structsize2 = sizeof(struct sockaddr);

	if (getpeername(fd1, (struct sockaddr *)&client1, &structsize1)<0)
	{
		//strcpy(host1, "fd1");
		strcpy_s(host1, sizeof(host1), "fd1");
	}
	else
	{
		//            printf("[+]got, ip:%s, port:%d\r\n",inet_ntoa(client1.sin_addr),ntohs(client1.sin_port));
		//strcpy(host1, inet_ntoa(client1.sin_addr));
		strcpy_s(host1, sizeof(host1), inet_ntoa(client1.sin_addr));
		port1 = ntohs(client1.sin_port);
	}

	if (getpeername(fd2, (struct sockaddr *)&client2, &structsize2)<0)
	{
		//strcpy(host2, "fd2");
		strcpy_s(host2, sizeof(host2), "fd2");
	}
	else
	{
		//            printf("[+]got, ip:%s, port:%d\r\n",inet_ntoa(client2.sin_addr),ntohs(client2.sin_port));
//		strcpy(host2, inet_ntoa(client2.sin_addr));
		strcpy_s(host2, sizeof(host2), inet_ntoa(client2.sin_addr));
		port2 = ntohs(client2.sin_port);
	}

	printf("[+] Start Transmit (%s:%d <-> %s:%d) ......\r\n\n", host1, port1, host2, port2);

	maxfd = max(fd1, fd2) + 1;
	memset(read_in1, 0, MAXSIZE);
	memset(read_in2, 0, MAXSIZE);
	memset(send_out1, 0, MAXSIZE);
	memset(send_out2, 0, MAXSIZE);

	while (1)
	{
		FD_ZERO(&readfd);
		FD_ZERO(&writefd);

		FD_SET((UINT)fd1, &readfd);
		//FD_SET((UINT)fd1, &writefd);
		//FD_SET((UINT)fd2, &writefd);
		FD_SET((UINT)fd2, &readfd);

		result = select(maxfd, &readfd, &writefd, NULL, NULL);
		if ((result<0) && (errno != EINTR))
		{
			printf("[-] Select error.\r\n");
			break;
		}
		else if (result == 0)
		{
			printf("[-] Socket time out.\r\n");
			continue;
			//break;
		}

		if (FD_ISSET(fd1, &readfd))
		{
			printf("FD_ISSET(fd1, &readfd)\r\n");
			/* must < MAXSIZE-totalread1, otherwise send_out1 will flow */
			if (totalread1<MAXSIZE)
			{
				read1 = recv(fd1, read_in1, MAXSIZE - totalread1, 0);
				if (read1 == SOCKET_ERROR)
				{
					printf("[-] Read fd1 data error,maybe close?\r\n");
					break;
				}
				else if (read1 == 0)
				{
					printf("connection maybe close, fd = %d \r\n", fd1);
					closesocket(fd1);
				}
				FD_SET((UINT)fd2, &writefd);
				memcpy(send_out1 + totalread1, read_in1, read1);
				//sprintf(tmpbuf, "\r\nRecv %5d bytes from %s:%d\r\n", read1, host1, port1);
				sprintf_s(tmpbuf, sizeof(tmpbuf), "\r\nRecv %5d bytes from %s:%d\r\n", read1, host1, port1);
				printf(" Recv %5d bytes %16s:%d\r\n", read1, host1, port1);
				totalread1 += read1;
				memset(read_in1, 0, MAXSIZE);
			}
		}

		if (FD_ISSET(fd2, &writefd))
		{
			printf("FD_ISSET(fd2, &writefd)\r\n");
			int err = 0;
			sendcount1 = 0;
			while (totalread1>0)
			{
				send1 = send(fd2, send_out1 + sendcount1, totalread1, 0);
				if (send1 == 0)break;
				if ((send1<0) && (errno != EINTR))
				{
					printf("[-] Send to fd2 unknow error.\r\n");
					err = 1;
					break;
				}

				if ((send1<0) && (errno == ENOSPC)) break;
				sendcount1 += send1;
				totalread1 -= send1;

				printf(" Send %5d bytes %16s:%d\r\n", send1, host2, port2);
			}

			if (err == 1) break;
			if ((totalread1>0) && (sendcount1>0))
			{
				/* move not sended data to start addr */
				memcpy(send_out1, send_out1 + sendcount1, totalread1);
				memset(send_out1 + totalread1, 0, MAXSIZE - totalread1);
			}
			else
			{
				memset(send_out1, 0, MAXSIZE);
			}			
			FD_CLR((UINT)fd2, &writefd);
		}

		if (FD_ISSET(fd2, &readfd))
		{
		printf("FD_ISSET(fd2, &readfd)\r\n");
			if (totalread2<MAXSIZE)
			{
				read2 = recv(fd2, read_in2, MAXSIZE - totalread2, 0);
				if (read2 == 0)break;
				if ((read2<0) && (errno != EINTR))
				{
					printf("[-] Read fd2 data error,maybe close?\r\n\r\n");
					break;
				}
				FD_SET((UINT)fd1, &writefd);
				memcpy(send_out2 + totalread2, read_in2, read2);
				//sprintf(tmpbuf, "\r\nRecv %5d bytes from %s:%d\r\n", read2, host2, port2);
				sprintf_s(tmpbuf, sizeof(tmpbuf), "\r\nRecv %5d bytes from %s:%d\r\n", read2, host2, port2);
				printf(" Recv %5d bytes %16s:%d\r\n", read2, host2, port2);
				totalread2 += read2;
				memset(read_in2, 0, MAXSIZE);
			}
		}

		if (FD_ISSET(fd1, &writefd))
		{
			printf("FD_ISSET(fd1, &writefd)\r\n");
			int err2 = 0;
			sendcount2 = 0;
			while (totalread2>0)
			{
				send2 = send(fd1, send_out2 + sendcount2, totalread2, 0);
				if (send2 == 0)
				{
					printf("connection maybe close, fd = %d \r\n", fd1);
					closesocket(fd1);
				}
				else if ((send2<0) && (errno != EINTR))
				{
					printf("[-] Send to fd1 unknow error.\r\n");
					err2 = 1;
					break;
				}
				if ((send2<0) && (errno == ENOSPC)) break;
				sendcount2 += send2;
				totalread2 -= send2;

				printf(" Send %5d bytes %16s:%d\r\n", send2, host1, port1);
			}
			if (err2 == 1) break;
			if ((totalread2>0) && (sendcount2 > 0))
			{
				/* move not sended data to start addr */
				memcpy(send_out2, send_out2 + sendcount2, totalread2);
				memset(send_out2 + totalread2, 0, MAXSIZE - totalread2);
			}
			else
			{
				memset(send_out2, 0, MAXSIZE);
			}	
			FD_CLR((UINT)fd1, &writefd);
		}
	}

	closesocket(fd1);
	closesocket(fd2);
	printf("\r\n[+] OK! I Closed The Two Socket.\r\n");
	return 0;
}

void getctrlc(int j)
{
	printf("\r\n[-] Received Ctrl+C\r\n");
	exit(0);
}

int create_socket()
{
	int sockfd;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd<0)
	{
		printf("[-] Create socket error.\r\n");
		return(0);
	}
	maxfd = maxfd > sockfd ? maxfd : sockfd;
	return(sockfd);
}

int create_server(int sockfd, int port)
{
	struct sockaddr_in srvaddr;
	int on = 1;

	memset(&srvaddr, 0, sizeof(struct sockaddr));

	srvaddr.sin_port = htons(port);
	srvaddr.sin_family = AF_INET;
	srvaddr.sin_addr.s_addr = htonl(INADDR_ANY);

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&on, sizeof(on)); //so I can rebind the port

	if (bind(sockfd, (struct sockaddr *)&srvaddr, sizeof(struct sockaddr))<0)
	{
		printf("[-] Socket bind error.\r\n");
		return(0);
	}

	if (listen(sockfd, CONNECTNUM)<0)
	{
		printf("[-] Socket Listen error.\r\n");
		return(0);
	}
	maxfd = maxfd > sockfd ? maxfd : sockfd;
	PRINT("create_server OK!\r\n");
	return(1);
}

