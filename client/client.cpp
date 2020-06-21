#include <windows.h>
#include <WinSock2.h>
#include <iostream>

#define PORT 8120
#define IP "127.0.0.1"
#define BUF_SIZE 256

#pragma comment(lib, "ws2_32.lib")

void main_client()
{
    WSADATA wsaData;
    sockaddr_in addr;
    WORD version = MAKEWORD(2, 0);
    if (WSAStartup(version, &wsaData))
    {
        printf("WSAStartup failed\n");
        return;
    }
    /*创建套接字*/
    SOCKET sckt = socket(AF_INET, SOCK_STREAM, 0);
    if (INVALID_SOCKET == sckt)
    {
        printf("Initial socket failed\n");
        return;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(IP);
    addr.sin_port = htons((u_short)PORT);
    while (connect(sckt, (sockaddr *)&addr, sizeof(addr)))
    {
        printf("connect fail. reconnect\n");
    }
	char buf[BUF_SIZE];
	int len = ::recv(sckt, buf, BUF_SIZE, 0);
	if (len > 0)
	{
		std::cout<<buf;
	}else{std::cout<<"nothing new";}
    /*发送和接受*/
    while (true)
    {
        char buf[BUF_SIZE];
		std::cin.getline(buf, BUF_SIZE);
        int slen = ::send(sckt, buf, BUF_SIZE, 0);
        if (slen <= 0)
            continue;
        if (strcmp(buf, "quit") == 0)
        {
            break;
        }
		int len = ::recv(sckt, buf, BUF_SIZE, 0);
        if (len > 0)
        {
            std::cout<<buf;
        }else{std::cout<<"nothing new";}
        
    }
    closesocket(sckt);
    WSACleanup();
    return;
}

int main(){
	main_client();
	return 0;
}