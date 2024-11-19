#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "add_fs.h"
#include "buf.h"
#include "disk.h"

char last_path_name[100];
char new_path_name[100];

int find_last_inode(const char* name){

    int start = 0;
    int end, i, j;

    int last_inode = 0;
    int last_block;

    memset(last_path_name, '\0', 100);
    memset(new_path_name, '\0', 100);

    Inode* pInode = (Inode*)malloc(sizeof(Inode));
    char* pBuf = (char*)malloc(BLOCK_SIZE);

    i = 1;
    while(name[i] != '\0'){
        if(name[i] == '/'){
            end = i;
            for(j = 1; j < end - start; j++){
                last_path_name[j - 1] = name[start + j];
            }
            last_path_name[j - 1] = '\0';
            
            GetInode(last_inode, pInode);
            last_block = pInode->dirBlockPtr[0];
            BufRead(last_block, pBuf);

            // pBuf에서 last_path_name과 동일한 DirEntry 검색
            int k = 0;
            while(strcmp(((DirEntry*)pBuf)[k].name, last_path_name)){
                if(k == NUM_OF_DIRENT_PER_BLOCK){
                    puts("NO NAME");
                    return -1;
                }
                k++;
            }
            
            // DirEntry의 inode 정보를 last_inode에 저장
            last_inode = ((DirEntry*)pBuf)[k].inodeNum;
            start = end;
        }
        i++;
    }

    for(j = 1; j < 100 - start; j++){
        new_path_name[j - 1] = name[start + j];
    }
    new_path_name[j - 1] = '\0';

    free(pInode);
    free(pBuf);

    return last_inode;
}