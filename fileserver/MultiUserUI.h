#pragma once
#ifndef MULTIUSERUI_H
#define MULTIUSERUI_H
#include <vector>
#include <mutex>
#include <map>
#include <string>
#include "Kernel.h"


struct Msg{
	string input;
	unsigned int fd; 
};



class MultiUserUI
{
public:
	MultiUserUI();
	~MultiUserUI();
	void run();
	
private:
	enum UIError {
		UNDERCONTROL,
		MISS,
		PTR,
		INVALID
	};
	Msg * msgs[Kernel::MAX_USER] = {NULL};
	int curu = 0; /*当前用户数量*/
	const map<Kernel::ERROR, std::string> errormsg{
		//{Kernel::NONERROR,"success"},
		{Kernel::ISDIR,"this is a directory"},
		{Kernel::NOTDIR,"this isn't a directory"},
		{Kernel::NOENT,"file/dir doesn't exist"},
		{Kernel::BADF,"invalid fd"},
		{Kernel::NOOUTENT,"outside file doesn't exist"},
		{Kernel::NOSPACE,"not enough space"},
		{Kernel::CONFLICT,"file being occupied"}
	};
	const map<UIError, std::string> uierrormsg{
		{MISS,"missing operand"},
		{PTR,"invalid ptrname"},
		{INVALID,"invalid command"}
	};
private:
	void userthr(User*);
	std::vector<std::thread> tv;
	std::mutex msgs_mutex;
	UIError uierror;
};
#endif
