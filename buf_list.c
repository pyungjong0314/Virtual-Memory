#include "buf_list.h"
#include <stdio.h>
#include <string.h>

int HashFun(int blocknum, int length) { return blocknum % length; }

void InsertBufToHead(Buf* pBuf, int blocknum, BufStateList pState){
    // pBuf state와 blkno 설정
    pBuf->blkno = blocknum;
    pBuf->state = pState;
    int hash_blk = HashFun(blocknum, MAX_BUFLIST_NUM);

    // BufList HEAD 삽입
    CIRCLEQ_INSERT_HEAD(&bufList[hash_blk], pBuf, blist);
    // stateList Tail 삽입
    CIRCLEQ_INSERT_TAIL(&stateList[pState], pBuf, slist);
}

// pBuf의 상태를 반환하는 함수 (상태가 없으면 -1 반환)
int FindBufState(Buf* pBuf){
    return pBuf->state;
}

// CLEAN, DIRTY LIST에 존재하는 pBuf 삭제
BOOL DeleteBuf(Buf* pBuf){
    int state = FindBufState(pBuf);
    // stateList에 존재하지 않는 경우
    if(state == -1){
        return FALSE;
    }

    // bufList와 stateList에 존재하는 경우
    // bufList에서 삭제
    int blk_hash = HashFun(pBuf->blkno, MAX_BUFLIST_NUM);
    CIRCLEQ_REMOVE(&bufList[blk_hash], pBuf, blist);
    // stateList에서 삭제
    CIRCLEQ_REMOVE(&stateList[pBuf->state], pBuf, slist);

    // pBuf state 설정
    pBuf->blkno = BLKNO_INVALID;
    pBuf->state = BLKNO_INVALID;
    return TRUE;
}

// BUF의 BufState를 state로 변경하는 함수
BOOL ChangeState(Buf* pBuf, BufStateList new_state){
    int old_state = FindBufState(pBuf);
    // pBuf가 stateList에 존재하지 않는 경우
    if(old_state == -1){
        return FALSE;
    }
    // 변경하려는 상태가 같은 경우
    else if(old_state == new_state){
        return TRUE;
    }
    
    // 상태가 변경되는 경우
    pBuf->state = new_state;
    // 기존 stateList에서 제거
    CIRCLEQ_REMOVE(&stateList[old_state], pBuf, slist);
    // 새로운 stateList TAIL에 삽입
    CIRCLEQ_INSERT_TAIL(&stateList[new_state], pBuf, slist);
    return TRUE;
}

// lruList Tail에 삽입
void InsertlruList(Buf* pBuf){
    CIRCLEQ_INSERT_TAIL(&lruListHead, pBuf, llist);
}

// lruList에서 검색
BOOL FindlruList(Buf* pBuf){
    // lruList가 비어있는 경우
    if(CIRCLEQ_EMPTY(&lruListHead)){
        return FALSE;
    }

    // lruList 탐색할 cursor 선언
    Buf* cursor = CIRCLEQ_FIRST(&lruListHead);
    while(1){
        // pBuf와 동일한 첫번째 Buf
        if(cursor == pBuf){
            return TRUE;
        }
        else if(cursor == CIRCLEQ_LAST(&lruListHead)){
            return FALSE;
        }
        // 다음 Buf 탐색
        cursor = CIRCLEQ_NEXT(cursor, llist);
    }
}

// lruList에서 삭제
BOOL DeletelruList(Buf* pBuf){
    if(FindlruList(pBuf)){
        CIRCLEQ_REMOVE(&lruListHead, pBuf, llist);
        return TRUE;
    }
    else{
        return FALSE;
    }
}

void InsertFreeList(Buf *pBuf){
    // pBuf state 설정
    pBuf->blkno = BLKNO_INVALID;
    pBuf->state = BLKNO_INVALID;

    CIRCLEQ_INSERT_TAIL(&freeListHead, pBuf, flist);
}

// 상태 변경 필요
BOOL DeleteFreeList(Buf *pBuf){
    if(CIRCLEQ_EMPTY(&freeListHead)){
        return FALSE;
    }
    else{
        CIRCLEQ_REMOVE(&freeListHead, pBuf, flist);
        return TRUE;
    }
}

void ChangelruList(){
    // lruList Head가 DIRTY라면 동기화
    Buf* pBuf = CIRCLEQ_FIRST(&lruListHead);
    if(pBuf->state == BUF_STATE_DIRTY){
        BufSyncBlock(pBuf->blkno);
    }
    // lruList Head 제거
    CIRCLEQ_REMOVE(&lruListHead, pBuf, llist);
    // Buf 제거
    DeleteBuf(pBuf);

    // FreeList 삽입
    CIRCLEQ_INSERT_TAIL(&freeListHead, pBuf, flist);
}