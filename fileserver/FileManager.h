#pragma once
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "OpenFileManager.h"
class User;
class Inode;

/* �ļ�������Ķ��� */
class FileManager
{
public:
	/* Ŀ¼����ģʽ������NameI()���� */
	enum DirectorySearchMode
	{
		OPEN = 0,		/* �Դ��ļ���ʽ����Ŀ¼ */
		CREATE = 1,		/* ���½��ļ���ʽ����Ŀ¼ */
		DELETE = 2		/* ��ɾ���ļ���ʽ����Ŀ¼ */
	};

public:
	FileManager() {};
	~FileManager() {};
	Inode* NameI(char(*func)(User*), enum DirectorySearchMode mode, User *u); /* ·������ */
	inline static char NextChar(User*);  /* ��ȡ·���е���һ���ַ� */
	void Open(User* u);			 /* ��һ���ļ� */
	void Creat(User* u);             /* �½�һ���ļ� */
	void Close(User* u);             /* �ر�һ���ļ� */
	void ChDir(User* u);             /* �ı䵱ǰ����Ŀ¼ */
	Inode* MakNode(unsigned int mode, User* u); /* �½�һ���ļ�ʱ��������Դ */
	void Open1(Inode* pInode, int mode, int trf, User* u); /* Open��Create�����Ĺ������� */
	void Read(User *u);          /* ���ļ� */
	void Write(User* u);         /* д�ļ� */
	void Seek(User* u);          /* �ı䵱ǰ��дָ���λ�� */
	void Delete(User* u);        /* ɾ���ļ� */
	void Rdwr(enum File::FileFlags mode, User* u);  /* Read��Write�����Ĺ������� */
public:
	Inode* rootDirInode; /* ��Ŀ¼�ڴ�Inode */

};

class DirectoryEntry
{
public:
	static const int DIRSIZ = 28;	/* Ŀ¼����·�����ֵ�����ַ������� */
public:
	DirectoryEntry() {};
	~DirectoryEntry() {};
public:
	int inode;		        /* Ŀ¼����Inode��Ų��� */
	char name[DIRSIZ];	    /* Ŀ¼����·�������� */
};


#endif