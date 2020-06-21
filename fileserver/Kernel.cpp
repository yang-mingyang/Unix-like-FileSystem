#include "Kernel.h"
#include "Utility.h"
#include <fstream>

Kernel Kernel::instance;

Kernel::Kernel()
{
	Kernel::DISK_IMG = "myDisk.img";
	this->BufMgr = new BufferManager;
	this->fileSys = new FileSystem;
	this->fileMgr = new FileManager;
	this->k_InodeTable = new InodeTable;
	this->s_openFiles = new OpenFileTable;
	this->k_openFiles = new OpenFiles;
	this->spb = new SuperBlock;
	this->userNum = 0;
}

Kernel::~Kernel()
{
	this->clear();
}

void Kernel::clear()
{		
	delete this->BufMgr;
	delete this->fileSys;
	delete this->fileMgr;
	delete this->k_InodeTable;
	delete this->s_openFiles;
	delete this->k_openFiles;
	delete this->spb;
}

void Kernel::init()
{
	this->fileSys->LoadSuperBlock();
	this->fileMgr->rootDirInode = this->k_InodeTable->IGet(FileSystem::ROOTINO);
}

void Kernel::inituser(int id) {
	User* u = this->getUser(id);
	u->cdir = this->fileMgr->rootDirInode;
	Utility::StringCopy("/", u->curdir);
}

void Kernel::callInit(int id)
{
	User* u = this->getUser(id);
	this->fileMgr->rootDirInode = this->k_InodeTable->IGet(FileSystem::ROOTINO);
	u->callReturn = -1;
	u->mode = 0;
	u->error = NONERROR;
}

Kernel* Kernel::getInstance()
{
	return &instance;
}

User * Kernel::getUser(int id)
{
	for (auto& u : users) if (u!=NULL && u->id == id) return u;
	return NULL;
}

int Kernel::getUserNum()
{
	return this->userNum;
}

User * Kernel::addUser()
{
	for (auto& u : users) if (u == NULL) {
		u = new User();
		return u;
	}
	return NULL;
}

bool Kernel::deleteUser(int id)
{
	for (auto& u : users)if (u != NULL && u->id == id) {
		delete u;
		u = NULL;
		return true;
	}
	return false;
}

BufferManager* Kernel::getBufMgr()
{
	return this->BufMgr;
}

FileSystem* Kernel::getFileSys()
{
	return this->fileSys;
}

FileManager* Kernel::getFileMgr()
{
	return this->fileMgr;
}

InodeTable* Kernel::getInodeTable()
{
	return this->k_InodeTable;
}

OpenFiles* Kernel::getOpenFiles()
{
	return this->k_openFiles;
}

OpenFileTable* Kernel::getOpenFileTable()
{
	return this->s_openFiles;
}

SuperBlock* Kernel::getSuperBlock()
{
	return this->spb;
}


void Kernel::format()
{
	/* 格式化磁盘 */
	fstream f(Kernel::DISK_IMG, ios::out | ios::binary);
	for (int i = 0; i <= this->getFileSys()->DATA_ZONE_END_SECTOR; i++)
		for (int j = 0; j < this->getBufMgr()->BUFFER_SIZE; j++)
			f.write((char*)" ", 1);
	f.close();
	/* 格式化SuperBlock */
	Buf* pBuf;
	SuperBlock &spb = (*this->spb);
	spb.s_isize = FileSystem::INODE_ZONE_SIZE;
	spb.s_fsize = FileSystem::DATA_ZONE_SIZE;
	spb.s_ninode = 100;
	spb.s_nfree = 0;
	/*
	spb.s_nfree = spb.s_ninode = 100;
	spb.s_fmod = 0;
	
	for (int i = 0; i < 100; i++)
	{
		spb.s_inode[99-i] = i + 1;
		spb.s_free[99-i] = FileSystem::DATA_ZONE_START_SECTOR + i;
	}
	*/
	for (int i = 0; i < spb.s_ninode; i++)
	{
		spb.s_inode[99 - i] = i + 1;
	}
	
	for (int i = FileSystem::DATA_ZONE_END_SECTOR; i >= FileSystem::DATA_ZONE_START_SECTOR; i--)
	{
		this->fileSys->Free(i);
	}
	//this->fileSys->Update();
	
	for (int i = 0; i < 2; i++)
	{
		int* p = (int *)&spb + i * 128;
		pBuf = this->BufMgr->GetBlk(FileSystem::SUPER_BLOCK_SECTOR_NUMBER + i);
		Utility::copy<int>(p, (int *)pBuf->b_addr, 128);
		this->BufMgr->Bwrite(pBuf);
	}
	
	/* 格式化Inode区 */
	for (int i = 0; i < FileSystem::INODE_ZONE_SIZE; i++)
	{
		pBuf = this->BufMgr->GetBlk(FileSystem::ROOTINO + FileSystem::INODE_ZONE_START_SECTOR + i);
		DiskInode DiskInode[FileSystem::INODE_NUMBER_PER_SECTOR];
		for (int j = 0; j < FileSystem::INODE_NUMBER_PER_SECTOR; j++)
		{
			DiskInode[j].d_mode = DiskInode[j].d_nlink = DiskInode[j].d_size = 0;
			for (int k = 0; k < 10; k++) 
				DiskInode[j].d_addr[k] = 0;
		}
		/* 为根目录增加目录标志 */
		if (i == 0) 
			DiskInode[0].d_mode |= Inode::IFDIR;
		Utility::copy<int>((int*)&DiskInode, (int*)pBuf->b_addr, 128);
		this->BufMgr->Bwrite(pBuf);
	}
	this->clear();
	this->BufMgr = new BufferManager;
	this->fileSys = new FileSystem;
	this->fileMgr = new FileManager;
	this->k_InodeTable = new InodeTable;
	this->s_openFiles = new OpenFileTable;
	this->k_openFiles = new OpenFiles;
	this->spb = new SuperBlock;
}

int Kernel::open(char* pathname, int mode,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->mode = mode;
	u->pathname = u->dirp = pathname;
	this->fileMgr->Open(u);
	return u->callReturn;
}

int Kernel::create(char* pathname, int mode,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->isDir = false;
	u->mode = mode;
	u->pathname = u->dirp = pathname;
	this->fileMgr->Creat(u);
	this->fileSys->Update();
	return u->callReturn;
}

void Kernel::mkdir(char* pathname,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->isDir = true;
	u->mode = 511;
	u->pathname = u->dirp = pathname;
	this->fileMgr->Creat(u);
	this->fileSys->Update();
	if (u->callReturn != -1)
		this->close(u->callReturn,id);
}

int Kernel::close(int fd,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->fd = fd;
	this->fileMgr->Close(u);
	this->fileSys->Update();
	return u->callReturn;
}

void Kernel::cd(char* pathname, int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->pathname = u->dirp = pathname;
	this->fileMgr->ChDir(u);
}

int Kernel::fread(int readFd, char* buf, int nbytes,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->fd = readFd;
	u->buf = buf;
	u->nbytes = nbytes;
	this->fileMgr->Read(u);
	return u->callReturn;
}

int Kernel::fwrite(int writeFd, char* buf, int nbytes,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->fd = writeFd;
	u->buf = buf;
	u->nbytes = nbytes;
	this->getFileMgr()->Write(u);
	return u->callReturn;
}

string Kernel::ls(int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->k_IOParam.m_Offset = 0;
	u->k_IOParam.m_Count = u->cdir->i_size / (DirectoryEntry::DIRSIZ + 4);
	Buf* pBuf = NULL;
	string ret;
	while (true)
	{
		/* 对目录项已经搜索完毕 */
		if (u->k_IOParam.m_Count == 0)
		{
			if (pBuf != NULL)
				this->getBufMgr()->Brelse(pBuf);
			break;
		}

		/* 已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
		if (u->k_IOParam.m_Offset % Inode::BLOCK_SIZE == 0)
		{
			if (pBuf != NULL)
				this->getBufMgr()->Brelse(pBuf);
			int phyBlkno = u->cdir->Bmap(u->k_IOParam.m_Offset / Inode::BLOCK_SIZE,u);
			pBuf = this->getBufMgr()->Bread(phyBlkno);
		}

		/* 没有读完当前目录项盘块，则读取下一目录项至dent */
		int* src = (int *)(pBuf->b_addr + (u->k_IOParam.m_Offset % Inode::BLOCK_SIZE));
		Utility::copy<int>(src, (int *)&u->dent, sizeof(DirectoryEntry) / sizeof(int));
		u->k_IOParam.m_Offset += (DirectoryEntry::DIRSIZ + 4);
		u->k_IOParam.m_Count--;
		if (u->dent.inode == 0)
			continue;
		
		ret+= u->dent.name;
		ret += " ";
	}
	ret += "\n";
	return ret;
}

void Kernel::fseek(int seekFd, int offset, int ptrname,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->fd = seekFd;
	u->offset = offset;
	u->mode = ptrname;
	this->fileMgr->Seek(u);
}

void Kernel::fdelete(char* pathname,int id)
{
	User* u = this->getUser(id);
	this->callInit(id);
	u->dirp = pathname;
	this->fileMgr->Delete(u);
}

void Kernel::fmount(char* from, char* to,int id)
{
	User* u = this->getUser(id);
	fstream f(from, ios::in | ios::binary);
	if (f)
	{
		
		f.seekg(0, f.end);
		int length = f.tellg();
		f.seekg(0, f.beg);
		char* tmpBuffer = new char[length];
		f.read(tmpBuffer, length);
		int tmpFd = this->open(to, 511,id);
		if (u->error != NONERROR)
			goto end;
		this->fwrite(tmpFd, tmpBuffer, length,id);
		if (u->error != NONERROR)
			goto end;
		this->close(tmpFd,id);
	end:
		f.close();
		delete tmpBuffer;
		return;
	}
	else
	{
		u->error = NOOUTENT;
		return;
	}
}
