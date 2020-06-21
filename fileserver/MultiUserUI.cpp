#include "MultiUserUI.h"
#include "Kernel.h"
#include "Utility.h"
#include<WinSock2.h>
#include <future>

#pragma comment(lib,"ws2_32.lib")
#define SIZE 5
#define PORT 8120
#define BUFFER_SIZE 256



MultiUserUI::MultiUserUI()
{
}

MultiUserUI::~MultiUserUI()
{
}

void MultiUserUI::run() {
	Kernel* k = Kernel::getInstance();
	SOCKET sListen;
	std::vector<SOCKET> rcvd;
	WSADATA wsaData;
	sockaddr_in addr;
	WORD version = MAKEWORD(2, 0);
	if (WSAStartup(version, &wsaData)) {
		printf("WSAStartup failed\n");
		return;
	}
	/*创建套接字*/
	sListen = socket(AF_INET, SOCK_STREAM, 0);
	if (sListen == INVALID_SOCKET) {
		printf("Initial socket failed\n");
		return;
	}
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((u_short)PORT);
	
	if (::bind(sListen, (sockaddr*)&addr, sizeof(addr))) {
		printf("Bind socket failed\n");
		return;
	}
	if (::listen(sListen, 10)) {
		printf("Listen socket failed\n");
		return;
	}
	/*初始化磁盘*/
	k->init();
	fd_set readset;
	int ret;
	while (true) {
		FD_ZERO(&readset);
		FD_SET(sListen, &readset);
		for (auto sckt : rcvd) { FD_SET(sckt, &readset); }
		/*select阻塞式监控套接字*/
		ret = select(NULL, &readset, NULL, NULL, NULL);
		/*新连接*/
		if (ret > 0 && FD_ISSET(sListen, &readset)) {
			sockaddr_in newaddr;
			int len = sizeof(newaddr);
			SOCKET newfd = accept(sListen, (sockaddr*)&newaddr, &len);
			printf("new user conneted\n");
			/*在内核中创建新用户*/
			User* u = k->addUser();
			if (u == NULL) {
				printf("add user failed\n");
				continue;
			}
			u->id = newfd;
			/*在UI中创建新消息站*/
			Msg* msg = new Msg();
			msg->fd = newfd;
			msg->input = "start";
			for (auto& msg_ : this->msgs) if (msg_ == NULL) {
				printf("msg create\n");
				msg_ = msg;
				break;
			}
			printf("create user thread\n");
			rcvd.emplace_back(newfd);
			this->tv.emplace_back(std::thread(&MultiUserUI::userthr, this, u));
			continue;
		}
		/*用户发来的新消息或者断连信息*/
		for (auto sckt : rcvd) if (FD_ISSET(sckt, &readset)) {
			if (ret <= 0) {
				/*断连*/
				{
					std::lock_guard<std::mutex> lock(this->msgs_mutex);
					for (auto msg : this->msgs) {
						if (msg!=NULL&&msg->fd == sckt) {
							msg->input = "quit";
							break;
						}
					}
				}
				auto it = find(rcvd.begin(), rcvd.end(), sckt);
				if(it!=rcvd.end()) rcvd.erase(it);
			}
			else if (ret > 0) {
				printf("new msg!:");
				char buf[BUFFER_SIZE];
				memset(buf, 0, BUFFER_SIZE);
				int len = recv(sckt, buf, BUFFER_SIZE, 0);
				printf("%s\n", buf);
				/*用户退出*/
				if (len < 0) strcpy_s(buf, "quit");
				if (strcmp(buf, "quit") == 0) {
					auto it = find(rcvd.begin(), rcvd.end(), sckt);
					if (it != rcvd.end()) rcvd.erase(it);
				}
				{
					std::lock_guard<std::mutex> lock(this->msgs_mutex);
					for (auto msg : this->msgs) {
						if (msg != NULL && msg->fd == sckt) {
							msg->input = buf;
							break;
						}
					}
				}
			}
			break;
		}
	}
}

void MultiUserUI::userthr(User* u)
{
	printf("user thread started\n");
	Msg* msg = NULL;
	int id = u->id;
	Kernel* k = Kernel::getInstance();
	for (auto msg_ : this->msgs) if (msg_!=NULL && msg_->fd == u->id) {
		msg = msg_;
		break;
	}
	if (msg == NULL) return;
	k->inituser(id);
	while (true) {
		printf("user thread:go looping\n");
		std::string output = "";
		this->uierror = UNDERCONTROL;
		while (msg->input.empty()); 
		std::string command = msg->input;
		msg->input.clear();
		vector<char*> result = Utility::parseCmd(command);
		
		if (result.size() > 0)
		{
			if (strcmp(result[0], "cd") == 0)
			{
				if (result.size() > 1) k->cd(result[1],id);
				else k->cd((char*)"/",id);
			}
			else if (strcmp(result[0], "fformat") == 0)
			{
				k->format();
				k->init();
				k->inituser(id);
			}
			else if (strcmp(result[0], "mkdir") == 0)
			{
				if (result.size() > 1) k->mkdir(result[1],id);
				else this->uierror = MISS;
			}
			else if (strcmp(result[0], "ls") == 0)
			{
				output = k->ls(id);
			}
			else if (strcmp(result[0], "fopen") == 0)
			{
				int fd;
				if (result.size() > 1)
				{
					fd = k->open(result[1], 511,id);
					if (u->error == Kernel::NONERROR)
						output += "fd = " + std::to_string(fd);
				}
				else this->uierror = MISS;
			}
			else if (strcmp(result[0], "fcreate") == 0)
			{
				int fd;
				if (result.size() > 1)
				{
					fd = k->create(result[1], 511,id);
					if (u->error == Kernel::NONERROR)
						output += "fd = " + std::to_string(fd);
				}
				else this->uierror = MISS;
			}
			else if (strcmp(result[0], "fclose") == 0)
			{
				if (result.size() > 1)
					k->close(atoi(result[1]), id);
				else
					this->uierror = MISS;
			}
			else if (strcmp(result[0], "fread") == 0)
			{
				int actual;
				if (result.size() > 2) {
					char* buf;
					buf = new char[atoi(result[2])];
					buf[0] = '\0';
					actual = k->fread(atoi(result[1]), buf, atoi(result[2]), id);
					if (actual == -1) {}
					else
					{
						if (actual > 0)
						{
							buf[actual] = '\0';
							output += buf;
						}
						output += "\nread " + std::to_string(actual);
						output += " bytes";
					}
					delete buf;
				}
				else
					this->uierror = MISS;
			}
			else if (strcmp(result[0], "fwrite") == 0)
			{
				int actual;
				if (result.size() > 3)
				{
					if (atoi(result[2]) > strlen(result[3]))
					{
						output += "nbytes out of limit";
					}
					else {
						actual = k->fwrite(atoi(result[1]), result[3], atoi(result[2]), id);
						if (actual == -1) {}
						else {
							output += "write" + std::to_string(actual);
							output += " bytes";
						}
					}
				}
				else
				{
					this->uierror = MISS;
				}
			}
			else if (strcmp(result[0], "fseek") == 0)
			{
				if (result.size() > 3)
				{
					if (atoi(result[3]) >= 0 && atoi(result[3]) <= 5)
						k->fseek(atoi(result[1]), atoi(result[2]), atoi(result[3]), id);
					else
						this->uierror = PTR;
				}
				else
					this->uierror = MISS;
			}
			else if (strcmp(result[0], "fdelete") == 0)
			{
				if (result.size() > 1)
					k->fdelete(result[1],id);
				else this->uierror = MISS;
			}
			else if (strcmp(result[0], "fmount") == 0)
			{
				if (result.size() > 2)
				{
					k->fmount(result[1], result[2],id);
				}
				else this->uierror = MISS;
			}
			else if (strcmp(result[0], "quit") == 0)
			{
				k->deleteUser(id);
				{
					std::lock_guard<std::mutex> lock(this->msgs_mutex);
					for (auto& msg_ : msgs) if (msg_ == msg) {
						delete msg_;
						msg_ = NULL;
						break;
					}
				}
				break;
			}
			else if (strcmp(result[0], "start") == 0) {}
			else {
				this->uierror = INVALID;
			}
		}
		if (!this->uierror == UNDERCONTROL) {
			output += uierrormsg.at(this->uierror);
		}
		else if(!u->error==Kernel::NONERROR){
			output += errormsg.at(u->error);
		}
		output += "\n";
		output += u->curdir;
		output += ":";
		::send(
			u->id,
			output.data(),
			output.length()+1,
			0
		);
	}

	return;
}


