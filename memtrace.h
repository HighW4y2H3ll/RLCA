
// Taint Tracing Module
// Hope to Store
// 1. Address Ref to Plain brk pointer
// 2. Address Store Encoded brk pointers
// 3. Self-Ref brk pointer
//
// 4. Address Ref to Encoded brk pointer, iff 
//    encoded by Xoring only Once
//

#ifndef _RMEMTRACE
#define _RMEMTRACE

#define MT_DEFAULT_SZ   1024

#define MT_KEY_SZ   mt_key_sz
#define MT_TAINT_SZ mt_taint_sz

#include "types.h"
#include "dr_api.h"


extern size_t mt_key_sz;
extern size_t mt_taintsz;


struct KeyEnt {
    size_t key;
    size_t ref_cnt;
};

struct KeyTab {
    size_t count;
    struct KeyEnt keys[1];  // MT_KEY_SZ
};

struct TaintEnt {
    size_t val;
    size_t ref;
};

struct TaintTab {
    size_t count;
    struct TaintEnt taints[1];  // MT_TAINT_SZ
};



bool isMemTraceInit();
void initMemTrace( int flag );
void destroyMemTrace();

//void setRegTaint(void *drcontext, size_t idx, reg_t key);
void setAddrTaint(reg_t ref, reg_t key);
void rmAddrTaint(reg_t reg);
void populateAddrTaint(reg_t src, reg_t ref);

bool isAddrTainted(size_t reg);

size_t mtIterStart();
size_t mtIterEnd();
size_t mtIterGetTaint(size_t iter);
void mtIterUpdateKey(size_t iter, size_t nkey);
void mtIterUpdateTaint(size_t iter, size_t ntaint);

instr_t *mtLeaInst(void *drcontext, size_t op1, opnd_t op2);
instr_t *mtMovInst(void *drcontext, void *op1, void *op2, size_t index);

void trimTaints( bool (*trim_func)(size_t taint, void *_args), void *args );

#endif

