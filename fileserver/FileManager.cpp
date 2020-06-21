#include "FileManager.h"
#include "Utility.h"
#include "Kernel.h"
void FileManager::Open(User* u)
{
	Inode* pInode;
	Kernel* k = Kernel::getInstance();

	pInode = this->NameI(NextChar, FileManager::OPEN, u);
	if (pInode == NULL)
		return;
	this->Open1(pInode, u->mode, 0, u);
}

void FileManager::Creat(User *u)
{
	Inode* pInode;
	Kernel* k = Kernel::getInstance();
	unsigned int newACCMode = u->mode & (Inode::IRWXU | Inode::IRWXG | Inode::IRWXO);
	pInode = this->NameI(NextChar, FileManager::CREATE,u);

	if (pInode == NULL)
	{
		if (u->error != Kernel::NONERROR) 
			return;
		/* 创建Inode */
		pInode = this->MakNode(newACCMode & (~Inode::ISVTX),u);
		if (pInode == NULL)
			return;
		this->Open1(pInode, File::FWRITE, 2,u);
	}
	else
	{
		this->Open1(pInode, File::FWRITE, 1,u);
		pInode->i_mode |= newACCMode;
	}
}

/*
* trf == 0由open调用
* trf == 1由creat调用，creat文件的时候搜索到同文件名的文件
* trf == 2由creat调用，creat文件的时候未搜索到同文件名的文件，这是文件创建时更一般的情况
* mode参数：打开文件模式，表示文件操作是 读、写还是读写
*/
void FileManager::Open1(Inode* pInode, int mode, int trf, User *u)
{
	Kernel* k = Kernel::getInstance();

	if (trf != 2 && (mode & File::FWRITE))
	{
		/* open去写目录文件是不允许的 */
		if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR)
			u->error = Kernel::ISDIR;
	}
	if (u->error != Kernel::NONERROR) {
		k->getInodeTable()->IPut(pInode);
		return;
	}

	/* 在creat文件的时候搜索到同文件名的文件，释放该文件所占据的所有盘块 */
	if (trf == 1)
		pInode->ITrunc();
		

	/* 分配打开文件控制块File结构 */
	File* pFile = k->getOpenFileTable()->FAlloc(u,pInode,mode,trf);
	if (pFile == NULL)
	{
		k->getInodeTable()->IPut(pInode);
		return;
	}
	return;
}

Inode* FileManager::MakNode(unsigned int mode, User* u)
{
	Inode* pInode;
	Kernel* k = Kernel::getInstance();

	/* 分配一个空闲DiskInode，里面内容已全部清空 */
	pInode = k->getFileSys()->IAlloc();
	
	if (pInode == NULL)
		return NULL;

	pInode->i_flag |= (Inode::IACC | Inode::IUPD);
	pInode->i_mode = mode | Inode::IALLOC;
	pInode->i_nlink = 1;

	/* 设置目录项中Inode编号部分 */
	u->dent.inode = pInode->i_number;

	/* 设置目录项中pathname分量部分 */
	for (int i = 0; i < DirectoryEntry::DIRSIZ; i++)
		u->dent.name[i] = u->dbuf[i];
	u->k_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	u->k_IOParam.m_Base = (char *)&u->dent;
	/* 将目录项写入父目录文件 */
	u->pdir->WriteI(u);
	k->getInodeTable()->IPut(u->pdir);
	return pInode;
}

void FileManager::Close(User* u)
{
	Kernel* k = Kernel::getInstance();
	File* pFile = k->getOpenFiles()->GetF(u->fd,u);
	if (pFile == NULL)
		return;
	k->getOpenFiles()->SetF(u->fd, NULL);
	/* 减少在系统打开文件表中File的引用计数 */
	k->getOpenFileTable()->CloseF(pFile);
}


void FileManager::ChDir(User* u)
{
	Inode* pInode;
	Kernel* k = Kernel::getInstance();

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN,u);
	if (pInode == NULL)
		return;
	/* 操纵数据文件，出错 */
	if ((pInode->i_mode & Inode::IFMT) != Inode::IFDIR)
	{
		u->error = Kernel::NOTDIR;
		k->getInodeTable()->IPut(pInode);
		return;
	}
	k->getInodeTable()->IPut(u->cdir);
	u->cdir = pInode;

	/* 设置当前工作目录字符串curdir */
	if (u->pathname[0] != '/')
	{
		int length = Utility::strlen(u->curdir);
		if (u->curdir[length - 1] != '/')
		{
			u->curdir[length] = '/';
			length++;
		}
		Utility::StringCopy(u->pathname, u->curdir + length);
	}
	/* 如果是从根目录'/'开始，则取代原有工作目录 */
	else
		Utility::StringCopy(u->pathname, u->curdir);
}


Inode* FileManager::NameI(char(*func)(User*), enum DirectorySearchMode mode, User* u)
{
	Inode* pInode;
	Buf* pBuf;
	char curchar;
	char* pChar;
	int freeEntryOffset;	         /* 以创建文件模式搜索目录时，记录空闲目录项的偏移量 */
	Kernel* k = Kernel::getInstance();
	BufferManager* bufMgr = k->getBufMgr();

	pInode = u->cdir;
	if ('/' == (curchar = (*func)(u)))
		pInode = this->rootDirInode;
	k->getInodeTable()->IGet(pInode->i_number);

	/* 允许出现////a//b 这种路径 这种路径等价于/a/b */
	while ('/' == curchar)
		curchar = (*func)(u);
	
	/* 如果试图更改和删除当前目录文件则出错 */
	if ('\0' == curchar && mode != FileManager::OPEN)
		goto out;
		
	/* 外层循环每次处理pathname中一段路径分量 */
	while (true)
	{
		if (u->error != Kernel::NONERROR)
			break;
		/* 整个路径搜索完毕，返回相应Inode指针。目录搜索成功返回。 */
		if ('\0' == curchar)
			return pInode;
		/* 将Pathname中当前准备进行匹配的路径分量拷贝到Kernel的dbuf[]中 */
		pChar = &(u->dbuf[0]);
		while ('/' != curchar && '\0' != curchar && u->error == Kernel::NONERROR)
		{
			if (pChar < &(u->dbuf[DirectoryEntry::DIRSIZ]))
			{
				*pChar = curchar;
				pChar++;
			}
			curchar = (*func)(u);
		}
		/* 将dbuf剩余的部分填充为'\0' */
		while (pChar < &(u->dbuf[DirectoryEntry::DIRSIZ]))
		{
			*pChar = '\0';
			pChar++;
		}

		/* 允许出现////a//b 这种路径 这种路径等价于/a/b */
		while ('/' == curchar)
			curchar = (*func)(u);
		if (u->error != Kernel::NONERROR)
			return NULL;

		/* 内层循环部分对于dbuf[]中的路径名分量，逐个搜寻匹配的目录项 */
		u->k_IOParam.m_Offset = 0;
		/* 设置为目录项个数 ，含空白的目录项*/
		u->k_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0;
		pBuf = NULL;
		while (true)
		{
			
			/* 对目录项已经搜索完毕 */
			if (u->k_IOParam.m_Count == 0)
			{
				if (pBuf != NULL)
					bufMgr->Brelse(pBuf);
				/* 如果是创建新文件 */
				if (FileManager::CREATE == mode && curchar == '\0')
				{
					/* 将父目录Inode指针保存起来，以后写目录项WriteDir()函数会用到 */
					u->pdir = pInode;
					if (freeEntryOffset)	/* 此变量存放了空闲目录项位于目录文件中的偏移量 */

						/* 将空闲目录项偏移量存入u区中，写目录项WriteDir()会用到 */
						u->k_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);

					else  
						pInode->i_flag |= Inode::IUPD;
					/* 找到可以写入的空闲目录项位置，NameI()函数返回 */
					return NULL;
				}
				/* 目录项搜索完毕而没有找到匹配项，释放相关Inode资源 */
				u->error = Kernel::NOENT;
				goto out;
			}

			/* 开始读第一盘块/已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
			if (u->k_IOParam.m_Offset % Inode::BLOCK_SIZE == 0)
			{
				
				if (pBuf != NULL)
					bufMgr->Brelse(pBuf);
				/* 计算要读的物理盘块号 */
				int phyBlkno = pInode->Bmap(u->k_IOParam.m_Offset / Inode::BLOCK_SIZE,u);
				pBuf = bufMgr->Bread(phyBlkno);
			}
			
			/* 没有读完当前目录项盘块，则读取下一目录项至dent */
			int* src = (int *)(pBuf->b_addr + (u->k_IOParam.m_Offset % Inode::BLOCK_SIZE));
			Utility::copy<int>(src, (int *)&u->dent, sizeof(DirectoryEntry) / sizeof(int));

			u->k_IOParam.m_Offset += (DirectoryEntry::DIRSIZ + 4);
			u->k_IOParam.m_Count--;

			/* 如果是空闲目录项，记录该项位于目录文件中偏移量 */
			if (u->dent.inode == 0)
			{
				if (freeEntryOffset == 0)
					freeEntryOffset = u->k_IOParam.m_Offset;
				/* 跳过空闲目录项，继续比较下一目录项 */
				continue;
			}

			int i;
			for (i = 0; i < DirectoryEntry::DIRSIZ; i++)
				if (u->dbuf[i] != u->dent.name[i])
					break;	/* 匹配至某一字符不符，跳出for循环 */

			if (i < DirectoryEntry::DIRSIZ)
				/* 不是要搜索的目录项，继续匹配下一目录项 */
				continue;
			else
				/* 目录项匹配成功，回到外层While(true)循环 */
				break;
		}

		if (pBuf != NULL)
			bufMgr->Brelse(pBuf);

		/* 如果是删除操作，则返回父目录Inode，而要删除文件的Inode号在dent.inode中 */
		if (FileManager::DELETE == mode && '\0' == curchar)
			return pInode;

		k->getInodeTable()->IPut(pInode);
		pInode = k->getInodeTable()->IGet(u->dent.inode);

		if (pInode == NULL)	
			return NULL;
	}
out:
	k->getInodeTable()->IPut(pInode);
	return NULL;
}


inline char FileManager::NextChar(User* u)
{
	return *u->dirp++;
}

void FileManager::Read(User* u)
{
	this->Rdwr(File::FREAD,u);
}

void FileManager::Write(User* u)
{
	this->Rdwr(File::FWRITE,u);
}

void FileManager::Rdwr(enum File::FileFlags mode,User* u)
{
	File* pFile; 
	Kernel* k = Kernel::getInstance();
	pFile = k->getOpenFiles()->GetF(u->fd,u);	
	if (pFile == NULL)
		return;

	u->k_IOParam.m_Base = (char *)u->buf;	
	u->k_IOParam.m_Count = u->nbytes;		
	u->k_IOParam.m_Offset = pFile->f_offset;
	if (File::FREAD == mode)
		pFile->f_inode->ReadI(u);
	else
		pFile->f_inode->WriteI(u);
	pFile->f_offset += (u->nbytes - u->k_IOParam.m_Count);
	u->callReturn = u->nbytes - u->k_IOParam.m_Count;
}

void FileManager::Seek(User* u)
{
	File* pFile;
	Kernel* k = Kernel::getInstance();
	int fd = u->fd;
	pFile = k->getOpenFiles()->GetF(fd,u);
	if (NULL == pFile)
		return;  

	int offset = u->offset;

	/* 如果seek模式在3 ~ 5之间，则长度单位由字节变为512字节 */
	if (u->mode > 2)
	{
		offset = offset << 9;
		u->mode -= 3;
	}

	switch (u->mode)
	{
		/* 读写位置设置为offset */
	case 0:
		pFile->f_offset = offset;
		break;
		/* 读写位置加offset(可正可负) */
	case 1:
		pFile->f_offset += offset;
		break;
		/* 读写位置调整为文件长度加offset */
	case 2:
		pFile->f_offset = pFile->f_inode->i_size + offset;
		break;
	}
}

void FileManager::Delete(User* u)
{
	Inode* pInode;
	Inode* pDeleteInode;
	Kernel* k = Kernel::getInstance();
	pDeleteInode = this->NameI(FileManager::NextChar, FileManager::DELETE,u);
	if (pDeleteInode == NULL)
		return;
	pInode = k->getInodeTable()->IGet(u->dent.inode);
	u->k_IOParam.m_Offset -= (DirectoryEntry::DIRSIZ + 4);
	u->k_IOParam.m_Base = (char *)&u->dent;
	u->k_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	u->dent.inode = 0;
	pDeleteInode->WriteI(u);
	pInode->i_nlink--;
	pInode->i_flag |= Inode::IUPD;
	k->getInodeTable()->IPut(pDeleteInode);
	k->getInodeTable()->IPut(pInode);
}