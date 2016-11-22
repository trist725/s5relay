
#include "socks5tran.h"

#define OUTPUT_DEBUG 1
#if OUTPUT_DEBUG
#define PRINT(fmt, ...) fprintf(stdout, fmt, ##__VA_ARGS__)
#else
#define PRINT(fmt, ...) 
#endif

#define LISTEN_BACKLOG    (INT_MAX-1)
#define LOCALHOST "0.0.0.0"
#define LISTEN_QUE 1024
#define EPOLL_SIZE 1024
#define EPOLL_TIME_OUT 300
#define EPOLL_TIME_WAIT -1
#define CONNECT_STEP 3000000

CSocket::CSocket()
{
}

CSocket::~CSocket()
{
}

void CSocket::Socket(int &_outFd) const
{
	_outFd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == _outFd)
	{
		perror("socket");
		exit(-1);
	}
}

void CSocket::Bind(int fd, const sockaddr *addr, int addrlen) const
{
	int ret = bind(fd, addr, addrlen);
	if (-1 == ret)
	{
		perror("bind");
		exit(-1);
	}
}

void CSocket::Listen(int fd, int backlog) const
{
	int ret = listen(fd, backlog);
	if (-1 == ret)
	{
		perror("listen");
		exit(-1);
	}
}

void CSocket::Setsockopt(int fd, int optname, const void* optval, socklen_t optlen) const
{
	int ret = setsockopt(fd, SOL_SOCKET, optname, optval, optlen);
	if (-1 == ret)
	{
		perror("setsockopt");
		exit(-1);
	}
}

void CSocket::Connect(int fd, const sockaddr *addr, socklen_t optlen) const
{
	if (connect(fd, addr, optlen) < 0)
	{
		perror("connect");
		exit(-1);
	}
}

void CSocket::SetNonblocking(int fd) const
{
	int flags;
	if ((flags = fcntl(fd, F_GETFL)) < 0)
	{
		perror("fcntl(Getfl)");
		exit(-1);
	}
	flags = flags | O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
	{
		perror("fcntl(set Nonblocking)");
		exit(-1);
	}
}

void CSocket::EpollEtRelay(int fd1, int fd2)
{
	PRINT("EpollEtRelay entry....\r\n");

	SetNonblocking(fd1);
	SetNonblocking(fd2);
	epoll_event* pEvents = new epoll_event[EPOLL_SIZE];
	epoll_event ev1, ev2;
	ev1.events = ev2.events = EPOLLIN | EPOLLET;
	ev1.data.fd = fd1;
	int epfd = epoll_create(EPOLL_SIZE);
	if (epfd < 0)
	{
		perror("epoll create");
		exit(-1);
	}
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd1, &ev1);
	if (ret < 0)
	{
		perror("epoll ctl add fd1");
		exit(-1);
	}
	ev2.data.fd = fd2;
	ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd2, &ev2);
	if (ret < 0)
	{
		perror("epoll ctl add fd2");
		exit(-1);
	}

	bool bRun = true;
	Relay_T rt1, rt2;
	bzero(&rt1, sizeof(rt1));
	bzero(&rt2, sizeof(rt2));
	rt1.fd = fd1;
	rt2.fd = fd2;
	while (bRun)
	{
		int n = epoll_wait(epfd, pEvents, EPOLL_SIZE, EPOLL_TIME_OUT); //等待EPOLL事件的发生
		if (n <= 0)
		{
			continue;
		}
		//m_bOnTimeChecking = FALSE;
		//g_CurTime = time(NULL);
		for (int i = 0; i < n; i++)
		{
			PRINT("epoll get %d events..\r\n", n);
			try
			{
				//		if (m_events[i].data.fd == m_listen_http_fd) //如果新监测到一个HTTP用户连接到绑定的HTTP端口则建立新连接。
				//		{
				//			OnAcceptHttpEpoll();
				//		}
				//		else if (m_events[i].data.fd == m_listen_sock_fd) //如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口则
				//建立新的连接。
				//		{
				//			OnAcceptSockEpoll();
				//		}
				//		else if (m_events[i].events & EPOLLIN) //如果是已经连接的用户，并且收到数据，那么进行读入操作。

				if ((pEvents[i].events & (EPOLLERR | EPOLLHUP)) == (EPOLLERR | EPOLLHUP))
				{
					PRINT("fd = %d get EPOLLERR | EPOLLHUP\r\n", pEvents[i].data.fd);
					bRun = false;
					break;
				}
				//对端fd关闭
				if ((pEvents[i].events & (EPOLLIN | EPOLLRDHUP)) == (EPOLLIN | EPOLLRDHUP))
				{
					PRINT("fd = %d get EPOLLIN | EPOLLRDHUP\r\n", pEvents[i].data.fd);
					bRun = false;
					break;
				}
				if (pEvents[i].events & EPOLLIN)
				{
					if (pEvents[i].data.fd == fd1)
					{
						OnReadEtRelay(rt1, fd2, epfd);
					}
					else
					{
						OnReadEtRelay(rt2, fd1, epfd);
					}
				}
				else if (pEvents[i].events & EPOLLOUT)
				{
					if (pEvents[i].data.fd == fd1)
					{
						OnWriteEtRelay(rt2, fd1, epfd);
					}
					else
					{
						OnWriteEtRelay(rt1, fd2, epfd);
					}
				}
			}
			catch (int)
			{
				PRINT("CATCH捕获错误\n");
				continue;
			}
		}
		//m_bOnTimeChecking = TRUE;
		//OnTimer(); //进行一些定时的操作，主要就是删除一些断线用户等。
	}

	close(fd1);
	close(fd2);
	PRINT("close 2 fds, fd1 = %d, fd2 = %d\r\n", fd1, fd2);
	delete[] pEvents;
}

void CSocket::OnReadEtRelay(Relay_T &rt, int fd, int epfd)
{
	PRINT("OnReadEpoll \r\nalreadyRead = %d\r\n", rt.alreadyRead);
	int rcvbuf_len;
	int len = sizeof(rcvbuf_len);
	if (getsockopt(rt.fd, SOL_SOCKET, SO_RCVBUF, (void *)&rcvbuf_len, (socklen_t *)&len) < 0){
		perror("getsockopt: ");
		return;
	}
	PRINT("recv buf len = %d\r\n", rcvbuf_len);
	if (0 == rt.alreadyRead)
	{
		bzero(rt.buf, BUFF_SIZE);
	}
	while (true)
	{
		if (rt.alreadyRead >= BUFF_SIZE)
		{
			PRINT("Read buffer is full, Transmit data maybe error.....\r\n");
			return;
			//exit(0);
		}
		int nRead = recv(rt.fd, rt.buf + rt.alreadyRead, BUFF_SIZE - rt.alreadyRead, MSG_DONTWAIT);
		if (0 == nRead)
		{
			PRINT("fd = %d maybe closed\r\n", rt.fd);
			epoll_event ev;
			ev.events = EPOLLRDHUP | EPOLLIN | EPOLLET;
			ev.data.fd = rt.fd;
			int ret = epoll_ctl(epfd, EPOLL_CTL_MOD, rt.fd, &ev);
			if (ret < 0)
			{
				perror("epoll_ctl mod\r\n");
			}
			PRINT("OnReadEtRelay mod fd=%d events with EPOLLRDHUP | EPOLLIN | EPOLLET\r\n", fd);
			return;
		}
		else if (nRead < 0)
		{
			//no buffer data to read
			if (errno == EAGAIN)
			{
				//perror("onread  EAGAIN\r\n");
				break;
			}
			//由于信号中断而没读到数据(ET模式下貌似不会被信号中断)
			else if (errno == EINTR)
			{
				perror("onread  EINTR\r\n");
				continue;
			}
			else
			{
				perror("recv");
				exit(-1);
			}
		}
		else
		{
			PRINT("-------------------read %d bytes from fd = %d :\r\n", nRead, rt.fd);
			PRINT("-------------------alreadyRead = %d\r\n", rt.alreadyRead);
			//PRINT("!!!!!!!!!!!!!!!!!!    %s\r\n", rt.buf);
			rt.alreadyRead += nRead;
		}
	}
	PRINT("-------------------after read loop, alreadyRead = %d\r\n", rt.alreadyRead);

	epoll_event ev;
	ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
	ev.data.fd = fd;
	int ret = epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
	if (ret < 0)
	{
		perror("epoll_ctl mod\r\n");
	}
	PRINT("OnReadEtRelay mod fd=%d events with EPOLLIN | EPOLLOUT | EPOLLET\r\n", fd);
}

void CSocket::OnWriteEtRelay(Relay_T &rt, int fd, int epfd)
{
	PRINT("onWriteEpoll %dbytes need to write\r\n", rt.alreadyRead);
	int nTotalSend = 0;
	int nSend = 0;

	while (rt.alreadyRead)
	{
		nSend = send(fd, rt.buf + nTotalSend, rt.alreadyRead, MSG_DONTWAIT);
		if (nSend < 0)
		{
			//buffer is full/Interrupted system call
			if ((errno == EAGAIN) || (errno == EINTR))
			{
				//continue;
				break;
			}
			else
			{
				perror("send OnWriteEtRelay");
				exit(-1);
			}
		}
		else if (0 == nSend)
		{
			PRINT("fd = %d maybe closed\r\n", fd);
			epoll_event ev;
			ev.events = EPOLLRDHUP | EPOLLIN | EPOLLET;
			ev.data.fd = rt.fd;
			int ret = epoll_ctl(epfd, EPOLL_CTL_MOD, rt.fd, &ev);
			if (ret < 0)
			{
				perror("epoll_ctl mod\r\n");
			}
			PRINT("OnWriteEtRelay mod fd=%d events with EPOLLRDHUP | EPOLLIN | EPOLLET\r\n", fd);
			return;
		}
		else
		{
			PRINT("-------------------send to fd=%d %d bytes\r\n", fd, nSend);
			nTotalSend += nSend;
			rt.alreadyRead -= nSend;
		}
	}
	epoll_event ev;
	ev.data.fd = fd;
	ev.events = EPOLLIN | EPOLLET;
	epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev); //修改标识符，等待下一个循环时接收数据 
	PRINT("OnWriteEtRelay mod fd=%d events with EPOLLIN | EPOLLET\r\n", fd);
}

CSocks5::CSocks5()
{
}

CSocks5::~CSocks5()
{
}

void* CSocks5::SocksRelay(void *arg)
{
	pthread_detach(pthread_self());
	Socks5_T* ps5t = (Socks5_T*)arg;
	CSocks5* ps5 = ps5t->ps5;
	int tranFd = ps5t->fd;
	delete ps5t;

	//get real fd
	int ret = SelectMethod(tranFd);
	if (ret < 0)
	{
		PRINT("SelectMethod error\r\n");
		return NULL;
	}
	ret = AuthPassword(tranFd);
	if (ret < 0)
	{
		PRINT("AuthPassword error\r\n");
		return NULL;
	}
	int realFd = ParseCommand(tranFd);
	if (realFd <= 0)
	{
		PRINT("ParseCommand error\r\n");
		return NULL;
	}
	PRINT("++++++++++++relay is tranfd = %d,realFd = %d\r\n", tranFd, realFd);
	ps5->EpollEtRelay(tranFd, realFd);

	return 0;
}

void* CSocks5::Accept(void *arg)
{
	pthread_detach(pthread_self());
	Socks5_T* ps5t = (Socks5_T*)arg;
	CSocks5* ps5 = ps5t->ps5;
	int fd = ps5t->fd;
	delete ps5t;

	sockaddr_in sin;
	int sinlen = sizeof(sin);

	epoll_event* pEvents = new epoll_event[EPOLL_SIZE];
	epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = fd;
	int epfd = epoll_create(EPOLL_SIZE);
	if (epfd < 0)
	{
		perror("epoll create in Accept");
		exit(-1);
	}
	int ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
	if (ret < 0)
	{
		perror("epoll ctl in Accept");
		exit(-1);
	}
	while (1)
	{
		PRINT("s5 server ep wait\r\n");
		int nEvents = epoll_wait(epfd, pEvents, EPOLL_SIZE, EPOLL_TIME_WAIT);
		if (nEvents <= 0)
		{
			perror("socks server error");
			continue;
		}
		PRINT("socks server get %d connections\r\n", nEvents);
		for (int i = 0; i < nEvents; ++i)
		{
			if (pEvents[i].data.fd == fd)
			{
				while (1)
				{
					int afd = accept(fd, (sockaddr*)&sin, (socklen_t*)&sinlen);
					if (afd > 0)
					{
						PRINT("accept from %s\r\n", inet_ntoa(sin.sin_addr));
						Socks5_T* ps5t = new Socks5_T;
						ps5t->fd = afd;
						ps5t->ps5 = ps5;
						pthread_t tid;
						int ret = pthread_create(&tid, NULL, SocksRelay, (void *)ps5t);
						if (ret != 0)
						{
							delete ps5t;
							perror("pthread_create on Accept\r\n");
							exit(-1);
						}
					}
					else if ((afd < 0) && (errno == EAGAIN))
					{
						break;
					}
				}
			}
		}
	}
	delete[] pEvents;
}

void CSocks5::init(int port)
{
	int fd = 0;
	Socket(fd);
	sockaddr_in sin;
	int ret = inet_pton(AF_INET, LOCALHOST, (void *)&sin.sin_addr);
	if (ret <= 0)
	{
		perror("inet_pton on CSocks5::init");
		exit(-1);
	}
	//sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	int opt = SO_REUSEADDR;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	Bind(fd, (const sockaddr*)&sin, sizeof(sin));
	Listen(fd, LISTEN_QUE);
	Socks5_T* ps5t = new Socks5_T;
	ps5t->fd = fd;
	ps5t->ps5 = this;
	pthread_t tid;
	ret = pthread_create(&tid, NULL, Accept, (void *)ps5t);
	if (ret != 0)
	{
		delete ps5t;
		perror("pthread_create on CSocks5::init\r\n");
		exit(-1);
	}
}

CTran::CTran() : _tranPort(0), _pTranIp(NULL), _socksPort(0)
{
}

CTran::~CTran()
{
}

void CTran::setTranPort(int tranPort)
{
	_tranPort = tranPort;
}
int CTran::getTranPort() const
{
	return _tranPort;
}

void CTran::setTranIp(const char* pTranIp)
{
	_pTranIp = pTranIp;
}
const char* CTran::getTranIp() const
{
	return _pTranIp;
}

void CTran::setSocksPort(int socksPort)
{
	_socksPort = socksPort;
}
int CTran::getSocksPort() const
{
	return _socksPort;
}

void* CTran::slaveRelay(void *arg)
{
	pthread_detach(pthread_self());
	Tran_T* pt = (Tran_T *)arg;
	int fd1 = pt->fd1;
	int fd2 = pt->fd2;
	CTran* pct = pt->pt;
	delete pt;
	PRINT("slaveRelay, fd1 = %d, fd2 = %d\r\n", fd1, fd2);
	pct->EpollEtRelay(fd1, fd2);
}

void CTran::init(char **argv)
{
	setTranIp(argv[1]);
	setTranPort(atoi(argv[2]));
	setSocksPort(atoi(argv[3]));

	int tranFd = 0;
	Socket(tranFd);
	int socksFd = 0;
	Socket(socksFd);
	sockaddr_in tranAddr, socksAddr;
	int ret = inet_pton(AF_INET, getTranIp(), (void *)&tranAddr.sin_addr);
	if (ret <= 0)
	{
		perror("inet_pton on CTran::init 1");
		exit(-1);
	}
	ret = inet_pton(AF_INET, LOCALHOST, (void *)&socksAddr.sin_addr);
	if (ret <= 0)
	{
		perror("inet_pton on CTran::init 2");
		exit(-1);
	}
	tranAddr.sin_family = socksAddr.sin_family = AF_INET;
	tranAddr.sin_port = htons(getTranPort());
	socksAddr.sin_port = htons(getSocksPort());

	bool isCon = false;
	bool isCon2 = false;
	int tmpfd = tranFd;
	int tmpfd2 = socksFd;
	while (1)
	{		
		//try to connect to tran
		if (isCon)
		{
			//close(tranFd);
			PRINT("reset connection.........\r\n");
			Socket(tmpfd);
			isCon = false;
		}
		SetNonblocking(tmpfd);
		//PRINT("tmpfd = %d\r\n", tmpfd);
		ret = connect(tmpfd, (const sockaddr*)&tranAddr, sizeof(tranAddr));
		if (ret != 0)
		{
			//perror("?????????????");
			usleep(CONNECT_STEP);
			continue;
		}
		isCon = true;
		printf("222222222222OK\r\n");
		//exit(1);
		//connect to tran succeeds,then connect to socks server
		
		while (1)
		{		
			if (isCon2)
			{
				Socket(tmpfd2);
				isCon2 = false;
			}
			ret = connect(tmpfd2, (const sockaddr*)&socksAddr, sizeof(socksAddr));
			if (ret < 0)
			{
				if (errno == EINPROGRESS)
				{
					continue;
				}
				perror("connect to socks server error : ");
				return;
			}
			else if (0 == ret)
			{
				isCon2 = true;
				break;
			}
		}		
		printf("tran init ok \r\n");
		Tran_T *pt = new Tran_T;
		pt->fd1 = tmpfd;
		pt->fd2 = tmpfd2;
		pt->pt = this;
		pthread_t pid;
		ret = pthread_create(&pid, NULL, slaveRelay, (void *)pt);
		if (ret < 0)
		{
			delete pt;
			perror("pthread_create on CTran::init\r\n");
			return;
		}
	}
}

int isValue(const char *str)
{
	if ((str == NULL) || (str[0] == '-'))
	{
		return 0;
	}
	return 1;
}

void usage(const char* prog)
{
	PRINT("[-] ERROR: too few parameter...\r\n");
	PRINT("[-] Usage: %s tranIP tranPort s5port\r\n", prog);
	PRINT("[-] make log to a new file?\r\n");
	PRINT("[-] Usage: %s tranIP tranPort s5port >fileName\r\n", prog);
	PRINT("[-] append log to a file?\r\n");
	PRINT("[-] Usage: %s tranIP tranPort s5port >>fileName\r\n", prog);
}

// Select authentic method, return 0 if success, -1 if failed
int SelectMethod(int sockfd)
{
	char recv_buffer[BUFF_SIZE] = { 0 };
	char reply_buffer[2] = { 0 };
	METHOD_SELECT_REQUEST *method_request;
	METHOD_SELECT_RESPONSE *method_response;

	// recv METHOD_SELECT_REQUEST
	int ret = recv(sockfd, recv_buffer, BUFF_SIZE, 0);
	if (ret <= 0) {
		perror("Receive error!\n");
		close(sockfd);
		return -1;
	}
	PRINT("SelectMethod: receive %d bytes.\n", ret);
	// if client request a wrong version or a wrong number_method
	method_request = (METHOD_SELECT_REQUEST *)recv_buffer;
	method_response = (METHOD_SELECT_RESPONSE *)reply_buffer;
	method_response->version = VERSION;

	// if not socks5
	if ((int)method_request->version != VERSION) {
		method_response->select_method = 0xff;
		send(sockfd, method_response, sizeof(METHOD_SELECT_RESPONSE), 0);
		close(sockfd);

		return -1;
	}
	method_response->select_method = AUTH_CODE;
	if (-1 == send(sockfd, method_response, sizeof(METHOD_SELECT_RESPONSE), 0)) {
		close(sockfd);
		return -1;
	}

	return 0;
}

// test password, return 0 for success.
int AuthPassword(int sockfd)
{
	char recv_buffer[BUFF_SIZE] = { 0 };
	char reply_buffer[BUFF_SIZE] = { 0 };
	AUTH_REQUEST *auth_request;
	AUTH_RESPONSE *auth_response;

	// auth username and password
	int ret = recv(sockfd, recv_buffer, BUFF_SIZE, 0);
	if (ret <= 0) {
		perror("Receive username and password error!\n");
		close(sockfd);
		return -1;
	}
	PRINT("AuthPass: receive %d bytes.\n", ret);

	auth_request = (AUTH_REQUEST *)recv_buffer;
	memset(reply_buffer, 0, BUFF_SIZE);
	auth_response = (AUTH_RESPONSE *)reply_buffer;
	auth_response->version = 0x01;

	char recv_name[256] = { 0 };
	char recv_pass[256] = { 0 };

	// auth_request->name_len is a char, max number is 0xff
	char pwd_str[2] = { 0 };
	strncpy(pwd_str, auth_request->name + auth_request->name_len, 1);
	int pwd_len = (int)pwd_str[0];
	strncpy(recv_name, auth_request->name, auth_request->name_len);
	strncpy(recv_pass, auth_request->name + auth_request->name_len + sizeof(auth_request->pwd_len), pwd_len);
	PRINT("Username: %s\nPassword: %s\n", recv_name, recv_pass);

	// check username and password
	if ((strncmp(recv_name, USER_NAME, strlen(USER_NAME)) == 0) && (strncmp(recv_pass, PASS_WORD, strlen(PASS_WORD)) == 0)) {
		auth_response->result = 0x00;
		if (-1 == send(sockfd, auth_response, sizeof(AUTH_RESPONSE), 0)) {
			close(sockfd);
			return -1;
		}
		else
			return 0;
	}
	else {
		auth_response->result = 0x01;
		send(sockfd, auth_response, sizeof(AUTH_RESPONSE), 0);
		close(sockfd);
		return -1;
	}
}

// parse command, and try to connect real server.
// return socket for success, -1 for failed.
int ParseCommand(int sockfd)
{
	char recv_buffer[BUFF_SIZE] = { 0 };
	char reply_buffer[BUFF_SIZE] = { 0 };

	SOCKS5_REQUEST *socks5_request;
	SOCKS5_RESPONSE *socks5_response;

	// receive command
	int ret = recv(sockfd, recv_buffer, BUFF_SIZE, 0);
	if (ret <= 0) {
		perror("Receive connect command error!\n");
		close(sockfd);
		return -1;
	}

	socks5_request = (SOCKS5_REQUEST *)recv_buffer;
	if ((socks5_request->version != VERSION) || (socks5_request->cmd != CONNECT) || (socks5_request->address_type == IPV6)) {
		PRINT("Connect command error!\n");
		close(sockfd);
		return -1;
	}

	// begin process connect request
	struct sockaddr_in sin;
	memset((void *)&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;

	// get real server's ip address
	if (socks5_request->address_type == IPV4) {
		memcpy(&sin.sin_addr.s_addr, &socks5_request->address_type + sizeof(socks5_request->address_type), 4);
		memcpy(&sin.sin_port, &socks5_request->address_type + sizeof(socks5_request->address_type) + 4, 2);
		PRINT("Real Server: %s %d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
		fflush(stdout);
	}
	else if (socks5_request->address_type == DOMAIN) {
		char domain_length = *(&socks5_request->address_type + sizeof(socks5_request->address_type));
		char target_domain[256] = { 0 };
		strncpy(target_domain, &socks5_request->address_type + 2, (unsigned int)domain_length);
		PRINT("Target: %s\n", target_domain);
		struct hostent *phost = gethostbyname(target_domain);
		if (phost == NULL) {
			PRINT("Resolve %s error!\n", target_domain);
			close(sockfd);
			return -1;
		}
		memcpy(&sin.sin_addr, phost->h_addr_list[0], phost->h_length);
		memcpy(&sin.sin_port, &socks5_request->address_type + sizeof(socks5_request->address_type) + sizeof(domain_length) + domain_length, 2);
	}

	// try to connect to real server
	PRINT("try to connect to real server\r\n");
	fflush(stdout);
	int real_server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (real_server_sockfd < 0) {
		perror("Socket creation failed!\n");
		close(sockfd);
		return -1;
	}

	memset(reply_buffer, 0, sizeof(BUFF_SIZE));
	socks5_response = (SOCKS5_RESPONSE *)reply_buffer;
	socks5_response->version = VERSION;
	socks5_response->reserved = 0x00;
	socks5_response->address_type = 0x01;
	memset(socks5_response + 4, 0, 6);

	ret = connect(real_server_sockfd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	if (ret == 0) {
		PRINT("connect to real server ok. \r\n");
		socks5_response->reply = 0x00;
		if (-1 == send(sockfd, socks5_response, 10, 0)) {
			close(sockfd);
			return -1;
		}
	}
	else {
		perror("Connect to real server error!\n");
		socks5_response->reply = 0x01;
		send(sockfd, socks5_response, 10, 0);
		close(sockfd);
		return -1;
	}

	return real_server_sockfd;
}

int main(int argc, char *argv[])
{
	if (argc < 4)
	{
		usage(argv[0]);
		exit(0);
	}
	PRINT("try to connect to real server\r\n");
	CSocks5 s5;
	CTran tran;
	s5.init(atoi(argv[3]));
	PRINT("socks5 init ok \r\n");
	tran.init(argv);
	return 0;
}
