#pragma once
#ifndef KERNEL_H
#define KERNEL_H
#include "FileManager.h"
#include "BufferManager.h"
#include "FileSystem.h"
#include "INode.h"
#include "OpenFileManager.h"

#include <iostream>
using namespace std;

struct IOParameter
{
	char* m_Base;	/* ��ǰ����д�û�Ŀ��������׵�ַ */
	int m_Offset;	/* ��ǰ����д�ļ����ֽ�ƫ���� */
	int m_Count;	/* ��ǰ��ʣ��Ķ���д�ֽ����� */
};



class User;

/* ��ϵͳ�ĺ�����(�ں�)��ֻ��ʼ��һ��ʵ��*/
class Kernel
{
public:
	enum ERROR {
		NONERROR = 0,            /* û�г��� */
		ISDIR = 1,               /* ���ݷ������ļ� */
		NOTDIR = 2,               /* cd������������ļ� */
		NOENT = 3,                 /* �ļ������� */
		BADF = 4,                  /* �ļ���ʶfd���� */
		NOOUTENT = 5,               /* windows�ⲿ�ļ������� */
		NOSPACE = 6,                 /* ���̿ռ䲻�� */
		CONFLICT = 7				/*���ļ���ͻ*/
	};
	Kernel();
	~Kernel();
	static const char* DISK_IMG;
	static const int MAX_USER = 10;
	BufferManager* getBufMgr();    /* ��ȡ�ں˵ĸ��ٻ������ʵ�� */
	FileSystem* getFileSys();      /* ��ȡ�ں˵��ļ�ϵͳʵ�� */
	FileManager* getFileMgr();     /* ��ȡ�ں˵��ļ�����ʵ�� */
	InodeTable* getInodeTable();   /* ��ȡ�ں˵��ڴ�Inode�� */
	OpenFiles* getOpenFiles();     /* ��ȡ�ں˵Ĵ��ļ��������� */
	OpenFileTable* getOpenFileTable(); /* ��ȡϵͳȫ�ֵĴ��ļ��������� */
	SuperBlock* getSuperBlock();    /* ��ȡȫ�ֵ�SuperBlock�ڴ渱��*/
	static Kernel* getInstance();  /* ��ȡΨһ���ں���ʵ�� */
	User* getUser(int id);
	int getUserNum();
	User* addUser();
	bool deleteUser(int id);
private:
	
	static Kernel instance;      /* Ψһ���ں���ʵ�� */
	BufferManager* BufMgr;       /* �ں˵ĸ��ٻ������ʵ�� */
	FileSystem* fileSys;         /* �ں˵��ļ�ϵͳʵ�� */
	FileManager* fileMgr;        /* �ں˵��ļ�����ʵ�� */
	InodeTable* k_InodeTable;    /* �ں˵��ڴ�Inode�� */
	OpenFileTable* s_openFiles;  /* ϵͳȫ�ִ��ļ��������� */
	OpenFiles* k_openFiles;      /* �ں˵Ĵ��ļ��������� */
	SuperBlock* spb;              /* ȫ�ֵ�SuperBlock�ڴ渱�� */
	User* users[Kernel::MAX_USER] = {NULL}; /* �û����� */
	int userNum;
public:
	void format();               /* ��ʽ������ */
	void init();             /* �ں˳�ʼ�� */
	void inituser(int id);	/* �û���ʼ�� */
	void callInit(int id);              /* ÿ���������õĳ�ʼ������ */
	int open(char* pathname, int mode,int id); /* ���ļ� */
	int create(char* pathname, int mode,int id); /* �½��ļ� */
	void mkdir(char* pathname,int id);   /* �½�Ŀ¼ */
	void cd(char* pathname,int id);      /* �ı䵱ǰ����Ŀ¼ */
	string ls(int id);                    /* ��ʾ��ǰĿ¼�µ������ļ� */
	int fread(int readFd, char* buf, int nbytes,int id); /* ��һ���ļ���Ŀ���� */
	int fwrite(int writeFd, char* buf, int nbytes,int id); /* ����Ŀ�������ַ�дһ���ļ� */
	void fseek(int seekFd, int offset, int ptrname,int id);/* �ı��дָ���λ�� */
	void fdelete(char* pathname,int id);  /* ɾ���ļ� */
	void fmount(char* from, char* to,int id); /* ��windows�ļ�����������ĳĿ¼�� */
	int close(int fd,int id);           /* �ر��ļ� */
	void clear();                /* ϵͳ�ر�ʱ��β���� */
};

/* �û�����*/
class User {
public:
	IOParameter k_IOParam;
	int callReturn;               /* ��¼���ú����ķ���ֵ */
	char* dirp;			   	      /* ָ��·������ָ��,����FileManager::NameI���� */
	char* pathname;               /* Ŀ��·���� */
	DirectoryEntry dent;		  /* ��ǰĿ¼��Ŀ¼�� */
	Inode* cdir;		          /* ָ��ǰĿ¼��Inodeָ�� */
	Inode* pdir;                  /* ָ��ǰĿ¼��Ŀ¼��Inodeָ�� */
	bool isDir;                   /* ��ǰ�����Ƿ����Ŀ¼�ļ� */
	char dbuf[DirectoryEntry::DIRSIZ];	/* ��ǰ·������ */
	char curdir[128];            /* ��ǰ��������Ŀ¼ */
	int mode;                     /* ��¼�����ļ��ķ�ʽ��seek��ģʽ */
	int fd;                       /* ��¼�ļ���ʶ */
	char* buf;                    /* ָ���д�Ļ����� */
	int nbytes;                   /* ��¼��д���ֽ��� */
	int offset;                   /* ��¼Seek�Ķ�дָ��λ�� */
	Kernel::ERROR error;                  /* �����ʶ */
	int id;						  /* �û�id */
};

#endif
