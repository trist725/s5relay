#ifndef SOCKS5TRAN_H_
#define SOCKS5TRAN_H_

#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define SOCKS_VERSION 0x05
#define CONNECT         0x01
#define IPV4            0x01
#define IPV6            0x04
#define DOMAIN          0x03
#define BUFF_SIZE (1024*16)
#define VERSION         0x05
#define AUTH_CODE       0x02
#define TIME_OUT        6000000

#define USER_NAME       "111111"
#define PASS_WORD       "222222"

// socks服务器对代理请求协商信息头的应答 05 00
typedef struct _METHOD_SELECT_RESPONSE
{
	char version;           // 服务器支持的socks版本，0x04（socks4）或者0x05（socks5）
	char select_method;     // 服务器选择的方法，0x00为匿名，0x02为密码认证
} METHOD_SELECT_RESPONSE;

// socks客户端发出的代理请求协商信息头 05 01 00
typedef struct _METHOD_SELECT_REQUEST
{
	char version;           // 客户端支持的版本，0x04或者0x05
	char number_methods;    // 客户端支持的方法的数量
	char methods[255];      // 客户端支持的方法类型，最多255个，0x00为匿名，0x02为密码认证
} METHOD_SELECT_REQUEST;

// 用户密码认证服务端响应 01 00
typedef struct _AUTH_RESPONSE
{
	char version;           // 版本，此处恒定为0x01
	char result;            // 服务端认证结果，0x00为成功，其他均为失败
} AUTH_RESPONSE;

//用户密码认证客户端请求
typedef struct _AUTH_REQUEST
{
	char version;           // 版本，此处恒定为0x01
	char name_len;          // 第三个字段用户名的长度，一个字节，最长为0xff
	char name[255];         // 用户名
	char pwd_len;           // 第四个字段密码的长度，一个字节，最长为0xff
	char pwd[255];          // 密码
} AUTH_REQUEST;

// 连接真实主机，Socks代理服务器响应
typedef struct _SOCKS5_RESPONSE
{
	char version;           // 服务器支持的Socks版本，0x04或者0x05
	char reply;             // 代理服务器连接真实主机的结果，0x00成功
	char reserved;          // 保留位，恒定为0x00
	char address_type;      // Socks代理服务器绑定的地址类型，IPv4为0x01，IPv6为0x04，域名为0x03
	char address_port[1];   // 如果address_type为域名，此处第一字节为域名长度，其后为域名本身，无0字符结尾，域名后为Socks代理服务器绑定端口
} SOCKS5_RESPONSE;

// 客户端请求连接真实主机
typedef struct _SOCKS5_REQUEST
{
	char version;           // 客户端支持的Socks版本，0x04或者0x05
	char cmd;               // 客户端命令，CONNECT为0x01，BIND为0x02，UDP为0x03，一般为0x01
	char reserved;          // 保留位，恒定为0x00
	char address_type;      // 客户端请求的真实主机的地址类型，IPv4为0x00，IPv6为0x04，DOMAIN为0x03
	char address_port[1];   // 如果address_type为域名，此处第一字节为域名长度，其后为域名本身，无0字符结尾，域名后为真实主机绑定端口
} SOCKS5_REQUEST;

struct Relay_T
{
	int fd;
	int alreadyRead;
	char buf[BUFF_SIZE];
};

class CSocket
{
public:
	CSocket();
	~CSocket();

	void Socket(int &_outFd) const;
	void Bind(int fd, const sockaddr *addr, int addrlen) const;
	void Listen(int fd, const int backlog) const;
	void Setsockopt(int fd, int optname, const void* optval, socklen_t optlen) const;
	void Connect(int fd, const sockaddr *addr, socklen_t optlen) const;
	void SetNonblocking(int fd) const;
	

	//void OnAcceptHttpEpoll();
	//void OnAcceptSockEpoll();
	//void OnTimer();
	
	void EpollEtRelay(int fd1, int fd2);
	//read rt.fd data to rt.buf,set fd out
	void OnReadEtRelay(Relay_T &rt, int fd, int epfd);
	//write rt.buf to fd, set fd in
	void OnWriteEtRelay(Relay_T &rt, int fd, int epfd);

private:

};

class CSocks5 : public CSocket
{
public:
	CSocks5();
	~CSocks5();

	void init(int port);

	static void* Accept(void *arg);
	static void* SocksRelay(void *arg);
private:

};

class CTran : public CSocket
{
public:
	CTran();
	~CTran();

	void setTranPort(int tranPort);
	int getTranPort() const;
	void setTranIp(const char* pTranIp);
	const char* getTranIp() const;
	void setSocksPort(int socksPort);
	int getSocksPort() const;

	void init(char **argv);

	static void* slaveRelay(void *arg);
private:
	//与另一端tran通信的port
	int _tranPort;
	int _socksPort;
	const char* _pTranIp;
};

struct Socks5_T
{
	CSocks5 *ps5;
	int fd;
};

struct Tran_T
{
	CTran *pt;
	int fd1;
	int fd2;
};

int SelectMethod(int sockfd);
int AuthPassword(int sockfd);
int ParseCommand(int sockfd);

#endif
