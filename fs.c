#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buf.h"
#include "fs.h"
#include "add_fs.h"

FileDesc pFileDesc[MAX_FD_ENTRY_MAX];
FileSysInfo* pFileSysInfo;

int     CreateFile(const char* szFileName)
{
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);
    
    int new_inode = GetFreeInodeNum();

    // Bit map 설정
    SetInodeBitmap(new_inode);

    // 마지막 디렉토리 블록의 inode, block
    int last_inode = find_last_inode(szFileName);
    int last_block;
    int dir_block_index = 0;

    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[dir_block_index];
    BufRead(last_block, pBuf);

    // pBuf에서 사용하지 않는 마지막 DirEntry 검사
    int i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, "")){
        i++;

        if(i == NUM_OF_DIRENT_PER_BLOCK){
            dir_block_index++;

            if(dir_block_index == pInode->allocBlocks){
                // 새로운 블록을 할당
                DirEntry *add_dir = (DirEntry*)malloc(NUM_OF_DIRENT_PER_BLOCK * sizeof(DirEntry));
                int add_block = GetFreeBlockNum();
                for(int k = 0; k < NUM_OF_DIRENT_PER_BLOCK; k++){
                    strcpy(add_dir[k].name, "");
                    add_dir[k].inodeNum = 0;
                }
                BufWrite(add_block, (char*)add_dir);
                SetBlockBitmap(add_block);
                free(add_dir);

                // Inode의 값 변경
                pInode->allocBlocks++;
                pInode->size = pInode->allocBlocks * BLOCK_SIZE;
                pInode->dirBlockPtr[pInode->allocBlocks - 1] = add_block;
                PutInode(last_inode, pInode);

                //FileSys 변경
                FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
                BufRead(0, (char*)file_info);
                file_info->blocks++;
                file_info->numAllocBlocks++;
                file_info->numFreeBlocks--;
                BufWrite(FILESYS_INFO_BLOCK, (char *)file_info);
                free(file_info);

                // 새로운 블록의 위치
                last_block = pInode->dirBlockPtr[pInode->allocBlocks - 1];
                BufRead(last_block, pBuf);
                i = 0;
                break;
            }
            else{
                last_block = pInode->dirBlockPtr[dir_block_index];
                BufRead(last_block, pBuf);
                i = 0;
            }
        }
    }

    // DirEntry에 이름과 inode 저장
    strcpy(((DirEntry*)pBuf)[i].name, new_path_name);
    ((DirEntry*)pBuf)[i].inodeNum = new_inode;
    BufWrite(last_block, pBuf);

    // 새로운 Inode 정보 저장
    GetInode(new_inode, pInode);
    pInode->allocBlocks = 0;
    pInode->type = FILE_TYPE_FILE;
    pInode->size = BLOCK_SIZE * (pInode->allocBlocks);
    pInode->dirBlockPtr[0] = 0;
    PutInode(new_inode, pInode);

    // FileSysInfo 변경
    FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
    BufRead(0, (char*)file_info);
    file_info->numAllocInodes++;
    BufWrite(FILESYS_INFO_BLOCK, (char *)file_info);

    // File 정보 설정
    File* new_file = (File*)malloc(sizeof(File));
    new_file->fileOffset = 0;
    new_file->inodeNum = new_inode;

    // File Descriptor table 설정
    int fd;
    for(fd = 0; fd < MAX_FD_ENTRY_MAX; fd++){
        if(pFileDesc[fd].bUsed == FALSE)
            break;
    }

    if(fd == MAX_FD_ENTRY_MAX){
        printf("FULL FILE DESCRIPTOR TABLE");
        return -1;
    }
    else{
        pFileDesc[fd].bUsed = TRUE;
        pFileDesc[fd].pOpenFile = new_file;
    }

    // 메모리 재할당
    free(pInode);
    free(pBuf);
    free(file_info);

    return fd;
}

int     OpenFile(const char* szFileName)
{
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);
    
    // 마지막 디렉토리 블록의 inode, block
    int last_inode = find_last_inode(szFileName);
    int last_block;
    int dir_block_index = 0;

    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[dir_block_index];
    BufRead(last_block, pBuf);
    
    // 동일한 이름의 file 찾기
    int i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, new_path_name)){
        i++;
        
        if(i == NUM_OF_DIRENT_PER_BLOCK){
            dir_block_index++;

            if(dir_block_index == pInode->allocBlocks){
                puts("CAN NOT OPEN FILE");
                return -1;
            }

            last_block = pInode->dirBlockPtr[dir_block_index];
            BufRead(last_block, pBuf);
            i = 0;
        }
    }
    
    // FILE의 inode num을 File Descriptor table에서 검사
    int fd;
    for(fd = 0; fd < MAX_FD_ENTRY_MAX; fd++){
        // FILE이 OPEN 되어 있는 경우
        if(pFileDesc[fd].bUsed == TRUE){
            if(pFileDesc[fd].pOpenFile->inodeNum == ((DirEntry*)pBuf)[i].inodeNum)
                return fd;
        }
    }

    // File이 OPEN 되어 있지 않은 경우
    for(fd = 0; fd < MAX_FD_ENTRY_MAX; fd++){
        // FILE이 OPEN 되어 있는 경우
        if(pFileDesc[fd].bUsed == FALSE){
            File* new_file = (File*)malloc(sizeof(File));
            new_file->fileOffset = 0;
            new_file->inodeNum = ((DirEntry*)pBuf)[i].inodeNum;

            pFileDesc[fd].bUsed = TRUE;
            pFileDesc[fd].pOpenFile = new_file;
            return fd;
        }
    }

    puts("OPEN FILE ERROR");
    return -1;
}


int     WriteFile(int fileDesc, char* pBuffer, int length)
{
    char* pBuf = (char*)malloc(BLOCK_SIZE);
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
    BufRead(FILESYS_INFO_BLOCK, file_info);

    // File inode의 정보
    int file_inode;
    if(pFileDesc[fileDesc].bUsed == FALSE){
        printf("NOT USED FILEDESC");
        return -1;
    }
    file_inode = pFileDesc[fileDesc].pOpenFile->inodeNum;
    GetInode(file_inode, pInode);
    
    // 작성할 블록의 개수
    int write_block_count = length / BLOCK_SIZE;
    int rest_length = length % BLOCK_SIZE;
    if(rest_length != 0){
        write_block_count++;
    }

    // FILE OFFSET
    int write_block;
    int file_offset = pFileDesc[fileDesc].pOpenFile->fileOffset;
    for(int i = 0; i < write_block_count; i++){
        if(pInode->dirBlockPtr[file_offset / BLOCK_SIZE] != 0){
            //OVERWRITTEN
            write_block = pInode->dirBlockPtr[file_offset / BLOCK_SIZE];
            BufRead(write_block, pBuf);
            memcpy(pBuf, &pBuffer[file_offset], BLOCK_SIZE);
            BufWrite(write_block, pBuf);
        }
        else{
            char* write_new_block = (char *)malloc(BLOCK_SIZE);
            write_block = GetFreeBlockNum();
            SetBlockBitmap(write_block);
            BufRead(write_block, write_new_block);
            memcpy(write_new_block, &pBuffer[file_offset], BLOCK_SIZE);
            BufWrite(write_block, write_new_block);

            pInode->dirBlockPtr[file_offset / BLOCK_SIZE] = write_block;
            pInode->allocBlocks++;
            pInode->size = pInode->allocBlocks * BLOCK_SIZE;

            // FileSys 정보 변경
            file_info->blocks++;
            file_info->numAllocBlocks++;
            file_info->numFreeBlocks--;
        }
        
        file_offset += BLOCK_SIZE;
    }

    // 변경된 정보 저장
    PutInode(file_inode, pInode);    
    BufWrite(FILESYS_INFO_BLOCK, (char*)file_info);

    // FILE DESCRIPTOR TABLE
    pFileDesc[fileDesc].pOpenFile->fileOffset += length;

    // 메모리 재할당
    free(pBuf);
    free(pInode);
    free(file_info);

    return fileDesc;
}

int     ReadFile(int fileDesc, char* pBuffer, int length)
{   
    if(pFileDesc[fileDesc].bUsed == FALSE){
        printf("INVALID FILE");
        return -1;
    }

    // inode의 정보
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    GetInode(pFileDesc[fileDesc].pOpenFile->inodeNum, pInode);
    int read_block = pInode->dirBlockPtr[pFileDesc[fileDesc].pOpenFile->fileOffset / BLOCK_SIZE];

    // Block 정보 읽기
    char* pBuf = (char*)malloc(BLOCK_SIZE);
    BufRead(read_block, pBuf);
    memcpy(pBuffer, pBuf, length);

    // offset 변경
    pFileDesc[fileDesc].pOpenFile->fileOffset += length;
    
    // 메모리 재할당
    free(pBuf);
    free(pInode);
}


int     CloseFile(int fileDesc)
{
    BufSync();

    pFileDesc[fileDesc].bUsed = FALSE;
    free(pFileDesc[fileDesc].pOpenFile);

    return 1;
}

int     RemoveFile(const char* szFileName)
{
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);

    // 마지막 디렉토리 블록의 inode, block
    int last_inode = find_last_inode(szFileName);
    int last_block;
    int dir_block_index = 0;

    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[dir_block_index];
    BufRead(last_block, pBuf);

    // 동일한 이름의 file 찾기
    int i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, new_path_name)){
        i++;

        if(i == NUM_OF_DIRENT_PER_BLOCK){
            dir_block_index++;
            if(dir_block_index == pInode->allocBlocks){
                printf("NOT EXIST FILE NAME");
                return -1;
            }

            last_block = pInode->dirBlockPtr[dir_block_index];
            BufRead(last_block, pBuf);
            i = 0;
            continue;
        }
    }

    // DirEntry 초기화
    strcpy(((DirEntry*)pBuf)[i].name, "");
    int file_inode = ((DirEntry*)pBuf)[i].inodeNum;
    ((DirEntry*)pBuf)[i].inodeNum = 0;
    BufWrite(last_block, pBuf);
    
    // Descriptor table 삭제
    int k = 0;
    while(pFileDesc[k].bUsed){
        if(k == NUM_OF_DIRECT_BLOCK_PTR){
            printf("CANNOT REMOVE FILE");
            return -1;
        }
        // Descriptor table에 존재한다면
        if(pFileDesc[k].pOpenFile->inodeNum == file_inode){
            pFileDesc[k].bUsed = FALSE;
            free(pFileDesc[k].pOpenFile);
        }
        k++;
    }

    // inode 초기화
    GetInode(file_inode, pInode);
    int used_block = pInode->allocBlocks;
    pInode->allocBlocks = 0;
    pInode->size = 0;
    for(int i = 0; i < used_block; i++){
        ResetBlockBitmap(pInode->dirBlockPtr[i]);
    }
    PutInode(file_inode, pInode);
    ResetInodeBitmap(file_inode);

    // FileSys 변경
    FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
    BufRead(FILESYS_INFO_BLOCK, (char*)file_info);
    file_info->blocks -= used_block;
    file_info->numAllocBlocks -= used_block;
    file_info->numFreeBlocks += used_block;
    file_info->numAllocInodes++;
    BufWrite(FILESYS_INFO_BLOCK, (char*)file_info);

    free(pInode);
    free(pBuf);
    free(file_info);
}


int      GetFileStatus(const char* szPathName, FileStatus* pStatus)
{
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);

    // 마지막 디렉토리 블록의 inode, block
    int last_inode = find_last_inode(szPathName);
    int last_block;
    int dir_block_index = 0;

    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[dir_block_index];
    BufRead(last_block, pBuf);

    // 동일한 이름의 file 찾기
    int i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, new_path_name)){
        i++;

        if(i == NUM_OF_DIRENT_PER_BLOCK){
            dir_block_index++;
            if(dir_block_index == pInode->allocBlocks){
                puts("CAN NOT File STATUS");
                return -1;
            }

            last_block = pInode->dirBlockPtr[dir_block_index];
            BufRead(last_block, pBuf);
            i = 0;
            continue;
        }
    }

    int file_inode = ((DirEntry*)pBuf)[i].inodeNum;
    GetInode(file_inode, pInode);
    pStatus->allocBlocks = pInode->allocBlocks;
    pStatus->size = pInode->size;
    pStatus->type = pInode->type;
    for(int j = 0; j < NUM_OF_DIRECT_BLOCK_PTR; j++){
        pStatus->dirBlockPtr[j] = pInode->dirBlockPtr[j];
    }

    free(pInode);
    free(pBuf);
}


int     MakeDir(const char* szDirName)
{
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);
    
    int new_inode = GetFreeInodeNum();
    int new_block = GetFreeBlockNum();
    
    // Bit map 설정
    SetInodeBitmap(new_inode);
    SetBlockBitmap(new_block);

    // 마지막 디렉토리 블록의 inode, block
    int last_inode = find_last_inode(szDirName);
    int last_block;
    int dir_block_index = 0;
    
    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[dir_block_index];
    BufRead(last_block, pBuf);

    // pBuf에서 사용하지 않는 마지막 DirEntry 검사
    int i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, "")){
        i++;

        if(i == NUM_OF_DIRENT_PER_BLOCK){
            dir_block_index++;

            if(dir_block_index == pInode->allocBlocks){
                // 새로운 블록을 할당
                DirEntry *add_dir = (DirEntry*)malloc(NUM_OF_DIRENT_PER_BLOCK * sizeof(DirEntry));
                int add_block = GetFreeBlockNum();
                for(int k = 0; k < NUM_OF_DIRENT_PER_BLOCK; k++){
                    strcpy(add_dir[k].name, "");
                    add_dir[k].inodeNum = 0;
                }
                BufWrite(add_block, (char*)add_dir);
                SetBlockBitmap(add_block);
                free(add_dir);

                // Inode의 값 변경
                pInode->allocBlocks++;
                pInode->size = pInode->allocBlocks * BLOCK_SIZE;
                pInode->dirBlockPtr[pInode->allocBlocks - 1] = add_block;
                PutInode(last_inode, pInode);

                //FileSys 변경
                FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
                BufRead(0, (char*)file_info);
                file_info->blocks++;
                file_info->numAllocBlocks++;
                file_info->numFreeBlocks--;
                BufWrite(FILESYS_INFO_BLOCK, (char *)file_info);
                free(file_info);

                // 새로운 블록의 위치
                last_block = pInode->dirBlockPtr[pInode->allocBlocks - 1];
                BufRead(last_block, pBuf);
                i = 0;
                break;
            }
            else{
                last_block = pInode->dirBlockPtr[dir_block_index];
                BufRead(last_block, pBuf);
                i = 0;
            }
        }
    }

    // DirEntry에 이름과 inode 저장
    strcpy(((DirEntry*)pBuf)[i].name, new_path_name);
    ((DirEntry*)pBuf)[i].inodeNum = new_inode;
    BufWrite(last_block, pBuf);

    // 새로운 Block을 할당
    DirEntry *new_dir = (DirEntry*)malloc(NUM_OF_DIRENT_PER_BLOCK * sizeof(DirEntry));
    char name1[MAX_NAME_LEN] = ".";
    strcpy(new_dir[0].name, name1);
    new_dir[0].inodeNum = new_inode;
    char name2[MAX_NAME_LEN] = "..";
    strcpy(new_dir[1].name, name2);
    new_dir[1].inodeNum = last_inode;
    for(int k = 2; k < NUM_OF_DIRENT_PER_BLOCK; k++){
        strcpy(new_dir[k].name, "");
    }
    BufWrite(new_block, (char*)new_dir);

    // 새로운 Inode 정보 저장
    GetInode(new_inode, pInode);
    pInode->allocBlocks = 1;
    pInode->type = FILE_TYPE_DIR;
    pInode->size = BLOCK_SIZE * (pInode->allocBlocks);
    pInode->dirBlockPtr[0] = new_block;
    PutInode(new_inode, pInode);

    // FileSysInfo 변경
    FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
    BufRead(0, (char*)file_info);
    file_info->blocks++;
    file_info->numAllocBlocks++;
    file_info->numFreeBlocks--;
    file_info->numAllocInodes++;
    BufWrite(FILESYS_INFO_BLOCK, (char *)file_info);

    // 메모리 재할당
    free(pInode);
    free(pBuf);
    free(new_dir);
    free(file_info);

    return new_inode;
}


int     RemoveDir(const char* szDirName)
{
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);

    // 마지막 디렉토리 블록의 inode, block
    int last_inode = find_last_inode(szDirName);
    int last_block;
    int dir_block_index = 0;
    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[dir_block_index];
    BufRead(last_block, pBuf);

    // pBuf에서 szDirName과 동일한 DirEntry 검사
    int i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, new_path_name)){
        i++;

        if(i == NUM_OF_DIRENT_PER_BLOCK){
            dir_block_index++;
            if(dir_block_index == pInode->allocBlocks){
                puts("NOT EXIST DIRECTORY NAME");
                return -1;
            }

            last_block = pInode->dirBlockPtr[dir_block_index];
            BufRead(last_block, pBuf);
            i = 0;
            continue;
        }
    }

    int remove_inode = ((DirEntry*)pBuf)[i].inodeNum;
    GetInode(remove_inode, pInode);
    int remove_block = pInode->dirBlockPtr[0];
    BufRead(remove_block, pBuf);

    i = 0;
    while(i < NUM_OF_DIRENT_PER_BLOCK){
        if(strcmp(((DirEntry*)pBuf)[i].name, ".") || strcmp(((DirEntry*)pBuf)[i].name, "..")){
            i++;
            continue;
        }
        else if(strcmp(((DirEntry*)pBuf)[i].name, "")){
            printf("DIRECTORY NOT EMPTY");
            return -1;
        }
        i++;
    }
    
    // szDirName inode, block 제거
    ResetInodeBitmap(remove_inode);
    ResetBlockBitmap(remove_block);

    // DirEntry 제거
    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[dir_block_index];
    BufRead(last_block, pBuf);
    i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, new_path_name)){
        i++;
    }
    strcpy(((DirEntry*)pBuf)[i].name, "");
    ((DirEntry*)pBuf)[i].inodeNum = 0;
    BufWrite(last_block, pBuf);

    // FileSysInfo 변경
    FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
    BufRead(FILESYS_INFO_BLOCK, (char*)file_info);
    file_info->blocks--;
    file_info->numAllocBlocks--;
    file_info->numFreeBlocks++;
    file_info->numAllocInodes--;

    // 추가된 dirBlock이 전부 비면
    if(dir_block_index > 0){
        int not_dir_block;
        for(not_dir_block = 0; not_dir_block < NUM_OF_DIRENT_PER_BLOCK; not_dir_block++){
            if(strcmp(((DirEntry*)pBuf)[not_dir_block].name, "")){
                break;
            }
        }

        // 추가된 dirBlock이 모두 삭제 된 경우
        if(not_dir_block == NUM_OF_DIRENT_PER_BLOCK){
            // Block Reset
            ResetBlockBitmap(last_block);

            // pInode 정보 변경
            pInode->dirBlockPtr[dir_block_index] = 0;
            pInode->allocBlocks--;
            pInode->size = BLOCK_SIZE * pInode->allocBlocks;
            PutInode(last_inode, pInode);

            // FileSys 변경
            file_info->blocks--;
            file_info->numAllocBlocks--;
            file_info->numFreeBlocks++;
        }
    }


    BufWrite(FILESYS_INFO_BLOCK, (char *)file_info);

    return 1;
}


int   EnumerateDirStatus(const char* szDirName, DirEntryInfo* pDirEntry, int dirEntrys)
{
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);

    // 마지막 디렉토리 블록의 inode, block
    int last_inode = find_last_inode(szDirName);
    int last_block;
    int used_dir_block = 0;
    GetInode(last_inode, pInode);
    last_block = pInode->dirBlockPtr[used_dir_block];
    BufRead(last_block, pBuf);

    // szDirName의 Block
    int i = 0;
    while(strcmp(((DirEntry*)pBuf)[i].name, new_path_name)){
        if(i == NUM_OF_DIRENT_PER_BLOCK){
            puts("NOT In REMOVE Dir NAME\n");
            return -1;
        }
        i++;
    }

    int find_inode = ((DirEntry*)pBuf)[i].inodeNum;
    GetInode(find_inode, pInode);

    // dirEntry의 갯수, pDirEntry 복사
    int count = 0;
    int dir_block_index;
    Inode* pDir_Inode = (Inode*)malloc(sizeof(Inode));

    for(dir_block_index = 0; dir_block_index < pInode->allocBlocks; dir_block_index++){
        // 처음 블록부터 정보 저장
        int find_block = pInode->dirBlockPtr[dir_block_index];
        BufRead(find_block, pBuf);

        for(int dir_block_num = 0; dir_block_num < NUM_OF_DIRENT_PER_BLOCK; dir_block_num++){
            // DirEntry가 존재하는 경우
            if(strcmp(((DirEntry*)pBuf)[dir_block_num].name, "")){
                strcpy(pDirEntry[count].name, ((DirEntry*)pBuf)[dir_block_num].name);
                pDirEntry[count].inodeNum = ((DirEntry*)pBuf)[dir_block_num].inodeNum;
                GetInode(((DirEntry*)pBuf)[dir_block_num].inodeNum, pDir_Inode);
                pDirEntry[count].type = pDir_Inode->type;
                
                count++;
            }
        }
    }


    free(pInode);
    free(pBuf);
    free(pDir_Inode);

    return count;
}


void    CreateFileSystem()
{
    DevCreateDisk();
    BufInit();
    FileSysInit();
    // root directory 생성
    int root_block = GetFreeBlockNum();
    int root_inode = GetFreeInodeNum();

    // root dir의 이름과 inode 설정
    DirEntry *root_dir = (DirEntry*)malloc(NUM_OF_DIRENT_PER_BLOCK * sizeof(DirEntry));
    char root_name[MAX_NAME_LEN] = ".";
    strcpy(root_dir[0].name, root_name);
    root_dir[0].inodeNum = root_inode;
    for(int k = 1; k < NUM_OF_DIRENT_PER_BLOCK; k++){
        strcpy(root_dir[k].name, "");
    }
    BufWrite(root_block, (char*)root_dir);

    // FileSys 변경
    FileSysInfo *file_info = (FileSysInfo*)malloc(BLOCK_SIZE);
    BufRead(0, (char*)file_info);
    file_info->blocks++;
    file_info->rootInodeNum = root_inode;
    file_info->numAllocBlocks++;
    file_info->numFreeBlocks--;
    file_info->numAllocInodes++;
    BufWrite(FILESYS_INFO_BLOCK, (char *)file_info);

    SetBlockBitmap(root_block);
    SetInodeBitmap(root_inode);

    // root_dir의 inode 변경
    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    GetInode(root_inode, pInode);
    pInode->allocBlocks = 1;
    pInode->dirBlockPtr[0] = root_block;
    pInode->type = FILE_TYPE_DIR;
    pInode->size = BLOCK_SIZE * pInode->allocBlocks;
    PutInode(root_inode, pInode);

    // File Descriptor table 초기화
    for(int i = 0; i < MAX_FD_ENTRY_MAX; i++){
        pFileDesc[i].bUsed = FALSE;
        pFileDesc[i].pOpenFile = NULL;
    }

    free(root_dir);
    free(file_info);
    free(pInode);
}

void    OpenFileSystem()
{   
    DevOpenDisk();
    BufInit();

    // File Descriptor table 초기화
    for(int i = 0; i < MAX_FD_ENTRY_MAX; i++){
        pFileDesc[i].bUsed = FALSE;
        pFileDesc[i].pOpenFile = NULL;
    }
}


void     CloseFileSystem()
{
    // File Descriptor table 초기화
    for(int i = 0; i < MAX_FD_ENTRY_MAX; i++){
        if(pFileDesc[i].bUsed = TRUE){
            pFileDesc[i].bUsed = FALSE;
        }
    }
}

void FileSysInit(void)
{
    // FileSysInfo 초기화
    FileSysInfo *init_file = (FileSysInfo*)malloc(BLOCK_SIZE);
    init_file->blocks = 7;
    init_file->rootInodeNum = 0;
    init_file->diskCapacity = FS_DISK_CAPACITY;
    init_file->numAllocBlocks = 7;
    init_file->numFreeBlocks = BLOCK_SIZE - 7;
    init_file->numAllocInodes = 0;
    init_file->blockBitmapBlock = BLOCK_BITMAP_BLOCK_NUM;
    init_file->inodeBitmapBlock = INODE_BITMAP_BLOCK_NUM;
    init_file->inodeListBlock = INODELIST_BLOCK_FIRST;
    init_file->dataRegionBlock = 7;
    BufWrite(FILESYS_INFO_BLOCK, (char*)init_file);
    
    // Inode_bitmap 초기화
    unsigned char* init_bitmap = (unsigned char*)malloc(BLOCK_SIZE);
    for(int i = 0; i < BLOCK_SIZE; i++){
        init_bitmap[i] = 0;
    }
    BufWrite(INODE_BITMAP_BLOCK_NUM, init_bitmap);
    // Block_bitmap 초기화
    init_bitmap[0] = (unsigned char)254;
    BufWrite(BLOCK_BITMAP_BLOCK_NUM, init_bitmap);

    // Inode_List 초기화
    Inode* init_inode_list = (Inode *)malloc(NUM_OF_INODE_PER_BLOCK * sizeof(Inode));
    for(int i = 0; i < NUM_OF_INODE_PER_BLOCK; i++){
        init_inode_list[i].allocBlocks = 0;
        init_inode_list[i].size = 0;
        init_inode_list[i].type = 0;
        for(int j = 0; j < NUM_OF_DIRECT_BLOCK_PTR; j++){
            init_inode_list[i].dirBlockPtr[j] = 0; 
        }
    }
    for(int i = 0; i < INODELIST_BLOCKS; i++){
        BufWrite(INODELIST_BLOCK_FIRST + i, (char*)init_inode_list);
    }

    // 모든 블록 0으로 초기화
    char* init_data = (char*)malloc(BLOCK_SIZE);
    for(int i = 0; i < BLOCK_SIZE; i++){
        init_data[i] = '0';
    }
    for(int i = init_file->dataRegionBlock; i < BLOCK_SIZE; i++){
        BufWrite(i, init_data);
    }
    
    // 메모리 재할당
    free(init_file);
    free(init_bitmap);
    free(init_inode_list);
    free(init_data);
}

void Sync(){
    BufSync();
}