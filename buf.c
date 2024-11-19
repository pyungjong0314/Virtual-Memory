#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf.h"
#include "disk.h"
#include "buf_list.h"

struct bufList      bufList[MAX_BUFLIST_NUM];
struct stateList    stateList[MAX_BUF_STATE_NUM];
struct freeList     freeListHead;
struct lruList      lruListHead;

void BufInit(void)
{
    // bufList 초기화
    for (int i = 0; i < MAX_BUFLIST_NUM; i++){
        CIRCLEQ_INIT(&bufList[i]);
    }

    // stateList 초기화
    for (int i = 0; i < MAX_BUF_STATE_NUM; i++){
        CIRCLEQ_INIT(&stateList[i]);
    }

    // lruList 초기화
    CIRCLEQ_INIT(&lruListHead);

    // freeList 초기화
    CIRCLEQ_INIT(&freeListHead);

    // Buf MAX_BUF_NUM 할당
    for(int i = 0; i < MAX_BUF_NUM; i++){
        Buf* pBuf = (Buf*)malloc(sizeof(Buf));
        pBuf->blkno = BLKNO_INVALID;
        pBuf->state = BLKNO_INVALID;
        pBuf->pMem = malloc(BLOCK_SIZE);
        CIRCLEQ_INSERT_TAIL(&freeListHead, pBuf, flist);
    }
}

Buf* BufFind(int blkno)
{
    int blk_hash = HashFun(blkno, MAX_BUFLIST_NUM);
    // Buflist가 비어있으면 NULL 반환
    if(CIRCLEQ_EMPTY(&bufList[blk_hash])){
        return NULL;
    }

    // bufList를 탐색할 cursor 선언
    Buf* cursor = CIRCLEQ_FIRST(&bufList[blk_hash]);
    while(1){
        if(cursor->state == BLKNO_INVALID){
            printf("BufFind ERROR");
            return NULL;
        }

        // pBuf와 동일한 첫번째 Buf
        if(cursor->blkno == blkno){
            return cursor;
        }
        // 존재하지 않는 경우 NULL 반환
        else if(cursor == CIRCLEQ_LAST(&bufList[blk_hash])){
            return NULL;
        }
        // 다음 Buf 탐색
        cursor = CIRCLEQ_NEXT(cursor, blist);
    }
}


void BufRead(int blkno, char* pData)
{
    Buf* pBuf = BufFind(blkno);
    // 버퍼에 blkno 존재하지 않는 경우
    if(pBuf == NULL){
        if(CIRCLEQ_EMPTY(&freeListHead)){
            ChangelruList();
        }

        // Free List에서 제거하여 Buf List에 CLEAN 상태로 저장
        pBuf = CIRCLEQ_FIRST(&freeListHead);
        DeleteFreeList(pBuf);
        InsertBufToHead(pBuf, blkno, BUF_STATE_CLEAN);

        // 디스크 내용을 읽어서 Buf에 저장
        DevReadBlock(blkno, pBuf->pMem);
    }
    // Buf에서 blkno 존재 (pData에 값 저장)
    memcpy(pData, pBuf->pMem, BLOCK_SIZE);

    // lru List 저장
    if(!FindlruList(pBuf)){
        InsertlruList(pBuf);
    }
    else{
        // lru에 존재하면 위치 변경
        DeletelruList(pBuf);
        InsertlruList(pBuf);
    }
}

void BufWrite(int blkno, char* pData)
{
    Buf* pBuf = BufFind(blkno);
    // 버퍼에 blkno 존재하지 않는 경우
    if(pBuf == NULL){
        if(CIRCLEQ_EMPTY(&freeListHead)){
            ChangelruList();
        }

        // Free List에서 제거하여 Buf List에 CLEAN 상태로 저장
        pBuf = CIRCLEQ_FIRST(&freeListHead);
        DeleteFreeList(pBuf);
        InsertBufToHead(pBuf, blkno, BUF_STATE_DIRTY);

        // 디스크 내용을 읽어서 Buf에 저장
        //DevReadBlock(blkno, pBuf->pMem);
    }
    // pBuf의 상태를 Dirty로 변경
    ChangeState(pBuf, BUF_STATE_DIRTY);
    // pData의 값을 pBuf->pMem에 저장
    memcpy(pBuf->pMem, pData, BLOCK_SIZE);

    // lru List 저장
    if(!FindlruList(pBuf)){
        InsertlruList(pBuf);
    }
    else{
        // lru에 존재하면 위치 변경
        DeletelruList(pBuf);
        InsertlruList(pBuf);
    }
}

void BufSync(void)
{
    // Dirty List가 비어있는 경우
    while(1){
    if(CIRCLEQ_EMPTY(&stateList[BUF_STATE_DIRTY])){
        return;
    }
    
    Buf* pBuf = CIRCLEQ_FIRST(&stateList[BUF_STATE_DIRTY]);
    DevWriteBlock(pBuf->blkno, pBuf->pMem);
    ChangeState(pBuf, BUF_STATE_CLEAN);
    BufSync();
    }
}

void BufSyncBlock(int blkno)
{
    Buf* pBuf = BufFind(blkno);
    if(pBuf == NULL){
        exit(-1);
    }

    DevWriteBlock(blkno, pBuf->pMem);
    ChangeState(pBuf, BUF_STATE_CLEAN);
}


int GetBufInfoInStateList(BufStateList listnum, Buf* ppBufInfo[], int numBuf)
{
    if(CIRCLEQ_EMPTY(&stateList[listnum])){
        return 0;
    }
    else {
        int index = 0;
        Buf *cursor = CIRCLEQ_FIRST(&stateList[listnum]);

        while(1){
            if(cursor == CIRCLEQ_LAST(&stateList[listnum])){
                ppBufInfo[index] = cursor;
                index++;

                return index;
            }

            // State List 탐색
            ppBufInfo[index] = cursor;
            cursor = CIRCLEQ_NEXT(cursor, slist);
            index++;
        }
    }
}

int GetBufInfoInLruList(Buf* ppBufInfo[], int numBuf)
{
    if(CIRCLEQ_EMPTY(&lruListHead)){
        return 0;
    }
    else {
        int index = 0;
        Buf *cursor = CIRCLEQ_FIRST(&lruListHead);

        while(1){
            if(cursor == CIRCLEQ_LAST(&lruListHead)){
                ppBufInfo[index] = cursor;
                index++;

                return index;
            }

            // Lru List 탐색
            ppBufInfo[index] = cursor;
            cursor = CIRCLEQ_NEXT(cursor, llist);
            index++;
        }
    }
}


int GetBufInfoInBufferList(int index, Buf* ppBufInfo[], int numBuf)
{
    if(CIRCLEQ_EMPTY(&bufList[index])){
        return 0;
    }
    else {
        int a = 0;
        Buf *cursor = CIRCLEQ_FIRST(&bufList[index]);

        while(1){
            if(cursor == CIRCLEQ_LAST(&bufList[index])){
                ppBufInfo[a] = cursor;
                a++;

                return a;
            }

            // List 탐색
            ppBufInfo[a] = cursor;
            cursor = CIRCLEQ_NEXT(cursor, blist);
            a++;
        }
    }
}