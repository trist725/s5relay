

更新说明：

V0.2
1.使程序支持多客户端连接；
2.增加Makfile

V0.1：

0.整合了socks5服务端和htran的slave功能，并改进了数据收发模型；
1.编译命令：g++ socks5tran.cpp -o st -lpthread , st为可执行文件名；
2.执行st会有使用说明；
3.示例： ./st ip1  port1  port2 ,其中ip1、port1为欲连接的lcx的IP和端口，port2为本机socks5服务端使用的端口，可设为任意未被占用的端口号；
4.运行若提示无权限，请以root权限执行命令；
5.socks用户名和密码暂时写死为user= 111111 ,psw= 222222  。

编译命令：
make
清除命令：
make clean

可根据需要修改Makefile文件头的变量实现不同级别编译
Makefile修改参数说明：
DEBUG=1  编译时加入调试参数
DEBUG=0  编译时不加入调试参数且程序以O2级别优化
STATIC=1 优先以静态方式编译
STATIC=0 以默认动态方式编译

当以STATIC方式编译，会有警告，不推荐使用。
警告：
warning: Using 'XXXXXX' in statically linked applications requires at runtime the shared libraries from the glibc version used for linking
原因：
1.glibc这些动态库的存在本身的目的就是为了能让在一台机器上编译好的库能够比较方便的移到另外的机器上，静态编译了它反而不美，偏离初衷；
2.出现以上警告原因是由于网络编程的一些接口还是需要动态库的支持才可以运行，许多glibc的函数都存在这样的问题；
3.对一些第三方工具不友好，如valgrind内存泄露检测工具；
4.影响某些库的性能。


数据转发流程：

real server  <-->  socks server  <--> tran(slave) <--> tran(listen) <--> socks client


