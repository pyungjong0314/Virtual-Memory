#ifndef __BUF_LIST_H__
#define __BUF_LIST_H__

#include "buf.h"

extern int      HashFun(int blocknum, int length);

extern void     InsertBufToHead(Buf* pObj, int blocknum, BufStateList state);
extern BOOL     DeleteBuf(Buf* pBuf);
extern int      FindBufState(Buf* pBuf);
extern BOOL     ChangeState(Buf* pBuf, BufStateList state);

extern void     InsertlruList(Buf* pBuf);
extern BOOL     FindlruList(Buf* pBuf);
extern BOOL     DeletelruList(Buf* pBuf);

extern void     InsertFreeList(Buf* pBuf);
extern BOOL     DeleteFreeList(Buf* pBuf);

extern void     ChangelruList();

#endif