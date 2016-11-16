
#ifndef _RTHREADLOCAL
#define _RTHREADLOCAL

#include "bitmap.h"
#include "dr_api.h"

struct _ThreadINFO {
    size_t chkstk;
    size_t memwrite;

    size_t torestore;       // Depreciated Flag
    size_t tomove;          // Depreciated Flag

    reg_t reg[16];          // Reg for Record Purpose
    size_t taint_reg[16];   // Reg for Taint Purpose

    reg_t src, dst;         // Landing for Instrument Store

    opnd_t tmpdst[5];       // Place Holder for Opnd Creation ( src0 embed with instr_t )
    instr_t MOV[4];         // Place Holder for 4 Mov Instrument
    instr_t LEA;            // Place Holder for Lea Instrument
    char padd[16];
};

struct _ThreadMap {
    struct _BitMap *bitmap;
    struct _ThreadINFO info[1]; // TLI_DEFAULTSZ
};

#define TLI_DEFAULTSZ   128


void initTLS ();
void expandTLS();

void destroyTLS();

void tlsSetFlag(void *drcontext);
void tlsClrFlag(void *drcontext);
bool tlsIsFlagSet(void *drcontext);

void tlsSetMoveFlag(void *drcontext);
void tlsClrMoveFlag(void *drcontext);
bool tlsIsMoveSet(void *drcontext);

void tlsChkStk(void *drcontext);
void tlsUnChkStk(void *drcontext);
bool isTlsChkStk(void *drcontext);

void tlsSetMemWrite(void *drcontext);
void tlsClrMemWrite(void *drcontext);
bool isTlsMemWrite(void *drcontext);

void tlsReset(void *drcontext);
void tlsSetReg(void *drcontext, size_t idx, reg_t value);
void tlsGetReg(void *drcontext, size_t idx, reg_t *dst);

void tlsGetTaintReg(void *drcontext, size_t idx, size_t *key);
void tlsSetTaintReg(void *drcontext, size_t idx, size_t key);

void tlsGetDst(void *drcontext, size_t *dst);
void *tlsRegSlotAddr(void *drcontext, size_t ireg);
void *tlsDstSlotAddr(void *drcontext);
void *tlsSrcSlotAddr(void *drcontext);
void *tlsDstOpndAddr(void *drcontext, size_t index);

instr_t *tlsGetMovInst(void *drcontext, size_t index);
instr_t *tlsGetLeaInst(void *drcontext);

#endif
