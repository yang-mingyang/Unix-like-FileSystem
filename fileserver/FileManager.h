#pragma once
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "OpenFileManager.h"
class User;
class Inode;

/* 文件管理类的定义 */
class FileManager
{
public:
	/* 目录搜索模式，用于NameI()函数 */
	enum DirectorySearchMode
	{
		OPEN = 0,		/* 以打开文件方式搜索目录 */
		CREATE = 1,		/* 以新建文件方式搜索目录 */
		DELETE = 2		/* 以删除文件方式搜索目录 */
	};

public:
	FileManager() {};
	~FileManager() {};
	Inode* NameI(char(*func)(User*), enum DirectorySearchMode mode, User *u); /* 路径搜索 */
	inline static char NextChar(User*);  /* 获取路径中的下一个字符 */
	void Open(User* u);			 /* 打开一个文件 */
	void Creat(User* u);             /* 新建一个文件 */
	void Close(User* u);             /* 关闭一个文件 */
	void ChDir(User* u);             /* 改变当前工作目录 */
	Inode* MakNode(unsigned int mode, User* u); /* 新建一个文件时，分配资源 */
	void Open1(Inode* pInode, int mode, int trf, User* u); /* Open和Create方法的公共部分 */
	void Read(User *u);          /* 读文件 */
	void Write(User* u);         /* 写文件 */
	void Seek(User* u);          /* 改变当前读写指针的位置 */
	void Delete(User* u);        /* 删除文件 */
	void Rdwr(enum File::FileFlags mode, User* u);  /* Read和Write方法的公共部分 */
public:
	Inode* rootDirInode; /* 根目录内存Inode */

};

class DirectoryEntry
{
public:
	static const int DIRSIZ = 28;	/* 目录项中路径部分的最大字符串长度 */
public:
	DirectoryEntry() {};
	~DirectoryEntry() {};
public:
	int inode;		        /* 目录项中Inode编号部分 */
	char name[DIRSIZ];	    /* 目录项中路径名部分 */
};


#endif