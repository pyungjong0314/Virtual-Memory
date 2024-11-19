#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "buf.h"
#include "fs.h"

void SetInodeBitmap(int inodeno)
{
    unsigned char* pBuf = (unsigned char*)malloc(BLOCK_SIZE);
    BufRead(INODE_BITMAP_BLOCK_NUM, pBuf);

    int bit_inode = inodeno / 8;
    int bit_index = inodeno % 8;
    unsigned char bit = 1;

    for(int i = 7 - bit_index; i > 0; i--){
        bit *= 2;
    }
    pBuf[bit_inode] += bit;

    BufWrite(INODE_BITMAP_BLOCK_NUM, pBuf);
    free(pBuf);
}


void ResetInodeBitmap(int inodeno)
{
    unsigned char* pBuf = (unsigned char*)malloc(BLOCK_SIZE);
    BufRead(INODE_BITMAP_BLOCK_NUM, pBuf);

    int bit_inode = inodeno / 8;
    int bit_index = inodeno % 8;
    unsigned char bit = 1;

    for(int i = 7 - bit_index; i > 0; i--){
        bit *= 2;
    }
    pBuf[bit_inode] -= bit;

    BufWrite(INODE_BITMAP_BLOCK_NUM, pBuf);
    free(pBuf);
}


void SetBlockBitmap(int blkno)
{
    unsigned char* pBuf = (unsigned char*)malloc(BLOCK_SIZE);
    BufRead(BLOCK_BITMAP_BLOCK_NUM, pBuf);

    int bit_block = blkno / 8;
    int bit_index = blkno % 8;
    unsigned char bit = 1;

    for(int i = 7 - bit_index; i > 0; i--){
        bit *= 2;
    }
    pBuf[bit_block] += bit;
    
    BufWrite(BLOCK_BITMAP_BLOCK_NUM, pBuf);

    free(pBuf);
}


void ResetBlockBitmap(int blkno)
{
    unsigned char* pBuf = (unsigned char*)malloc(BLOCK_SIZE);
    BufRead(BLOCK_BITMAP_BLOCK_NUM, pBuf);

    int bit_block = blkno / 8;
    int bit_index = blkno % 8;
    unsigned char bit = 1;

    for(int i = 7 - bit_index; i > 0; i--){
        bit *= 2;
    }
    pBuf[bit_block] -= bit;

    BufWrite(BLOCK_BITMAP_BLOCK_NUM, pBuf);
    free(pBuf);
}


void PutInode(int inodeno, Inode* pInode)
{
    int inodeno_list = inodeno / NUM_OF_INODE_PER_BLOCK;
    int inodeno_index = inodeno % NUM_OF_INODE_PER_BLOCK;

    char* pBuf = (char*)malloc(BLOCK_SIZE);
    // Inodeno가 있는 Inode List를 pBuf로 가져온다.
    BufRead(INODELIST_BLOCK_FIRST + inodeno_list, pBuf);
    // pInode의 값을 pBuf에 있는 inodeno 위치에 복사
    memcpy(((Inode*)pBuf +inodeno_index), pInode, sizeof(Inode));
    // pBuf의 값을 Inode List에 저장
    BufWrite(INODELIST_BLOCK_FIRST + inodeno_list, pBuf);
    free(pBuf);
}


void GetInode(int inodeno, Inode* pInode)
{
    int inodeno_list = inodeno / NUM_OF_INODE_PER_BLOCK;
    int inodeno_index = inodeno % NUM_OF_INODE_PER_BLOCK;
    
    char* pBuf = (char*)malloc(BLOCK_SIZE);
    // Inodeno가 있는 Inode List를 pBuf로 가져온다.
    BufRead(INODELIST_BLOCK_FIRST + inodeno_list, pBuf);
    // Inode List에서 inodeno의 정보를 pInode에 복사
    memcpy(pInode, ((Inode*)pBuf +inodeno_index), sizeof(Inode));
    free(pBuf);
}


int GetFreeInodeNum(void)
{
    unsigned char* pBuf = (unsigned char*)malloc(BLOCK_SIZE);
    BufRead(INODE_BITMAP_BLOCK_NUM, pBuf);

    for(int i = 0; i < BLOCK_SIZE; i++){
        // free bit 존재
        if(pBuf[i] < 255){
            unsigned char bit_inode = pBuf[i];
            int free_bit;
            for(int j = 7; j >= 0; j--){
                if(bit_inode % 2 == 0){
                    free_bit = j;
                }
                bit_inode = bit_inode / 2;
            }
            return 8 * i + free_bit;
        }
    }

    printf("FULL INODE LIST");
    free(pBuf);
    return -1;
}


int GetFreeBlockNum(void)
{
    unsigned char* pBuf = (unsigned char*)malloc(BLOCK_SIZE);
    BufRead(BLOCK_BITMAP_BLOCK_NUM, pBuf);
    for(int i = 0; i < BLOCK_SIZE; i++){
        // free bit 존재
        if(pBuf[i] < 255){
            unsigned char bit_block = pBuf[i];
            int free_bit;
            for(int j = 7; j >= 0; j--){
                if(bit_block % 2 == 0){
                    free_bit = j;
                }
                bit_block = bit_block / 2;
            }
            return 8 * i + free_bit;
        }
    }
    printf("FULL BLOCK LIST");
    free(pBuf);
    return -1;
}