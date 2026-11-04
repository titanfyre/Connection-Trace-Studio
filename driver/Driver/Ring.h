#ifndef CT_PRIVATE_RING_H
#define CT_PRIVATE_RING_H
/* Nonallocating indexing shared by the private driver and its offline test. */
typedef struct CT_RING_INDEX { CT_U32 Head,Count; CT_U64 Produced,Dropped; } CT_RING_INDEX;
static __inline void CtRingReset(CT_RING_INDEX* r){r->Head=0;r->Count=0;r->Produced=0;r->Dropped=0;}
static __inline CT_U32 CtRingPush(CT_RING_INDEX* r){
 CT_U32 slot;
 ++r->Produced;
 if(r->Count==CT_RING_CAPACITY){r->Head=(r->Head+1)%CT_RING_CAPACITY;--r->Count;++r->Dropped;}
 slot=(r->Head+r->Count)%CT_RING_CAPACITY;++r->Count;return slot;
}
static __inline CT_U32 CtRingPop(CT_RING_INDEX* r){
 CT_U32 slot;
 if(!r->Count)return CT_RING_CAPACITY;
 slot=r->Head;r->Head=(r->Head+1)%CT_RING_CAPACITY;--r->Count;return slot;
}
#endif
