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
		/* ����Inode */
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
* trf == 0��open����
* trf == 1��creat���ã�creat�ļ���ʱ��������ͬ�ļ������ļ�
* trf == 2��creat���ã�creat�ļ���ʱ��δ������ͬ�ļ������ļ��������ļ�����ʱ��һ������
* mode���������ļ�ģʽ����ʾ�ļ������� ����д���Ƕ�д
*/
void FileManager::Open1(Inode* pInode, int mode, int trf, User *u)
{
	Kernel* k = Kernel::getInstance();

	if (trf != 2 && (mode & File::FWRITE))
	{
		/* openȥдĿ¼�ļ��ǲ������ */
		if ((pInode->i_mode & Inode::IFMT) == Inode::IFDIR)
			u->error = Kernel::ISDIR;
	}
	if (u->error != Kernel::NONERROR) {
		k->getInodeTable()->IPut(pInode);
		return;
	}

	/* ��creat�ļ���ʱ��������ͬ�ļ������ļ����ͷŸ��ļ���ռ�ݵ������̿� */
	if (trf == 1)
		pInode->ITrunc();
		

	/* ������ļ����ƿ�File�ṹ */
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

	/* ����һ������DiskInode������������ȫ����� */
	pInode = k->getFileSys()->IAlloc();
	
	if (pInode == NULL)
		return NULL;

	pInode->i_flag |= (Inode::IACC | Inode::IUPD);
	pInode->i_mode = mode | Inode::IALLOC;
	pInode->i_nlink = 1;

	/* ����Ŀ¼����Inode��Ų��� */
	u->dent.inode = pInode->i_number;

	/* ����Ŀ¼����pathname�������� */
	for (int i = 0; i < DirectoryEntry::DIRSIZ; i++)
		u->dent.name[i] = u->dbuf[i];
	u->k_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	u->k_IOParam.m_Base = (char *)&u->dent;
	/* ��Ŀ¼��д�븸Ŀ¼�ļ� */
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
	/* ������ϵͳ���ļ�����File�����ü��� */
	k->getOpenFileTable()->CloseF(pFile);
}


void FileManager::ChDir(User* u)
{
	Inode* pInode;
	Kernel* k = Kernel::getInstance();

	pInode = this->NameI(FileManager::NextChar, FileManager::OPEN,u);
	if (pInode == NULL)
		return;
	/* ���������ļ������� */
	if ((pInode->i_mode & Inode::IFMT) != Inode::IFDIR)
	{
		u->error = Kernel::NOTDIR;
		k->getInodeTable()->IPut(pInode);
		return;
	}
	k->getInodeTable()->IPut(u->cdir);
	u->cdir = pInode;

	/* ���õ�ǰ����Ŀ¼�ַ���curdir */
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
	/* ����ǴӸ�Ŀ¼'/'��ʼ����ȡ��ԭ�й���Ŀ¼ */
	else
		Utility::StringCopy(u->pathname, u->curdir);
}


Inode* FileManager::NameI(char(*func)(User*), enum DirectorySearchMode mode, User* u)
{
	Inode* pInode;
	Buf* pBuf;
	char curchar;
	char* pChar;
	int freeEntryOffset;	         /* �Դ����ļ�ģʽ����Ŀ¼ʱ����¼����Ŀ¼���ƫ���� */
	Kernel* k = Kernel::getInstance();
	BufferManager* bufMgr = k->getBufMgr();

	pInode = u->cdir;
	if ('/' == (curchar = (*func)(u)))
		pInode = this->rootDirInode;
	k->getInodeTable()->IGet(pInode->i_number);

	/* �������////a//b ����·�� ����·���ȼ���/a/b */
	while ('/' == curchar)
		curchar = (*func)(u);
	
	/* �����ͼ���ĺ�ɾ����ǰĿ¼�ļ������ */
	if ('\0' == curchar && mode != FileManager::OPEN)
		goto out;
		
	/* ���ѭ��ÿ�δ���pathname��һ��·������ */
	while (true)
	{
		if (u->error != Kernel::NONERROR)
			break;
		/* ����·��������ϣ�������ӦInodeָ�롣Ŀ¼�����ɹ����ء� */
		if ('\0' == curchar)
			return pInode;
		/* ��Pathname�е�ǰ׼������ƥ���·������������Kernel��dbuf[]�� */
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
		/* ��dbufʣ��Ĳ������Ϊ'\0' */
		while (pChar < &(u->dbuf[DirectoryEntry::DIRSIZ]))
		{
			*pChar = '\0';
			pChar++;
		}

		/* �������////a//b ����·�� ����·���ȼ���/a/b */
		while ('/' == curchar)
			curchar = (*func)(u);
		if (u->error != Kernel::NONERROR)
			return NULL;

		/* �ڲ�ѭ�����ֶ���dbuf[]�е�·���������������Ѱƥ���Ŀ¼�� */
		u->k_IOParam.m_Offset = 0;
		/* ����ΪĿ¼����� �����հ׵�Ŀ¼��*/
		u->k_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0;
		pBuf = NULL;
		while (true)
		{
			
			/* ��Ŀ¼���Ѿ�������� */
			if (u->k_IOParam.m_Count == 0)
			{
				if (pBuf != NULL)
					bufMgr->Brelse(pBuf);
				/* ����Ǵ������ļ� */
				if (FileManager::CREATE == mode && curchar == '\0')
				{
					/* ����Ŀ¼Inodeָ�뱣���������Ժ�дĿ¼��WriteDir()�������õ� */
					u->pdir = pInode;
					if (freeEntryOffset)	/* �˱�������˿���Ŀ¼��λ��Ŀ¼�ļ��е�ƫ���� */

						/* ������Ŀ¼��ƫ��������u���У�дĿ¼��WriteDir()���õ� */
						u->k_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);

					else  
						pInode->i_flag |= Inode::IUPD;
					/* �ҵ�����д��Ŀ���Ŀ¼��λ�ã�NameI()�������� */
					return NULL;
				}
				/* Ŀ¼��������϶�û���ҵ�ƥ����ͷ����Inode��Դ */
				u->error = Kernel::NOENT;
				goto out;
			}

			/* ��ʼ����һ�̿�/�Ѷ���Ŀ¼�ļ��ĵ�ǰ�̿飬��Ҫ������һĿ¼�������̿� */
			if (u->k_IOParam.m_Offset % Inode::BLOCK_SIZE == 0)
			{
				
				if (pBuf != NULL)
					bufMgr->Brelse(pBuf);
				/* ����Ҫ���������̿�� */
				int phyBlkno = pInode->Bmap(u->k_IOParam.m_Offset / Inode::BLOCK_SIZE,u);
				pBuf = bufMgr->Bread(phyBlkno);
			}
			
			/* û�ж��굱ǰĿ¼���̿飬���ȡ��һĿ¼����dent */
			int* src = (int *)(pBuf->b_addr + (u->k_IOParam.m_Offset % Inode::BLOCK_SIZE));
			Utility::copy<int>(src, (int *)&u->dent, sizeof(DirectoryEntry) / sizeof(int));

			u->k_IOParam.m_Offset += (DirectoryEntry::DIRSIZ + 4);
			u->k_IOParam.m_Count--;

			/* ����ǿ���Ŀ¼���¼����λ��Ŀ¼�ļ���ƫ���� */
			if (u->dent.inode == 0)
			{
				if (freeEntryOffset == 0)
					freeEntryOffset = u->k_IOParam.m_Offset;
				/* ��������Ŀ¼������Ƚ���һĿ¼�� */
				continue;
			}

			int i;
			for (i = 0; i < DirectoryEntry::DIRSIZ; i++)
				if (u->dbuf[i] != u->dent.name[i])
					break;	/* ƥ����ĳһ�ַ�����������forѭ�� */

			if (i < DirectoryEntry::DIRSIZ)
				/* ����Ҫ������Ŀ¼�����ƥ����һĿ¼�� */
				continue;
			else
				/* Ŀ¼��ƥ��ɹ����ص����While(true)ѭ�� */
				break;
		}

		if (pBuf != NULL)
			bufMgr->Brelse(pBuf);

		/* �����ɾ���������򷵻ظ�Ŀ¼Inode����Ҫɾ���ļ���Inode����dent.inode�� */
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

	/* ���seekģʽ��3 ~ 5֮�䣬�򳤶ȵ�λ���ֽڱ�Ϊ512�ֽ� */
	if (u->mode > 2)
	{
		offset = offset << 9;
		u->mode -= 3;
	}

	switch (u->mode)
	{
		/* ��дλ������Ϊoffset */
	case 0:
		pFile->f_offset = offset;
		break;
		/* ��дλ�ü�offset(�����ɸ�) */
	case 1:
		pFile->f_offset += offset;
		break;
		/* ��дλ�õ���Ϊ�ļ����ȼ�offset */
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