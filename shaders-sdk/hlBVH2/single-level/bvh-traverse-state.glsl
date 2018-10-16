
#ifndef _CACHE_BINDING
#define _CACHE_BINDING 9
#endif

#ifndef _RAY_TYPE
#define _RAY_TYPE VtRay
#endif


#ifndef fpInner
  const float fpInner = 0.0000152587890625f, fpOne = 1.f;
#endif

#ifdef ENABLE_VEGA_INSTRUCTION_SET
  const  lowp  int localStackSize = 8, pageCount = 4; // 256-bit global memory stack pages
#else
  const  lowp  int localStackSize = 4, pageCount = 8; // 128-bit capable (minor GPU, GDDR6 two-channels)
#endif
  //const highp uint maxIterations  = 8192u;
  const highp uint maxIterations  = 16384u;


const uint GridSize = 256;
layout ( binding = _CACHE_BINDING, set = 0, std430 ) coherent buffer VT_PAGE_SYSTEM {
    int stack[WORK_SIZE*GridSize][localStackSize];
    int pages[][localStackSize];
} traverseCache;


// stack system of current BVH traverser
shared int localStack[WORK_SIZE][localStackSize];
uint currentState = BVH_STATE_TOP;
#define lstack localStack[Local_Idx]
#define sidx  traverseState.stackPtr

// BVH traversing state
#define _cacheID gl_GlobalInvocationID.x
struct BvhTraverseState {
    int defElementID, maxElements, entryIDBase, diffOffset; bool saved;
    int idx;    lowp int stackPtr   , pageID;
#ifdef ENABLE_STACK_SWITCH
    int idxTop; lowp int stackPtrTop, pageIDTop;
#else
    int idxDefer;
#endif
    fvec4_ directInv, minusOrig;
} traverseState; //traverseStates[2];
//#define traverseState traverseStates[currentState] // yes, require two states 

// 13.10.2018 added one mandatory stack page, can't be reached by regular operations 
#define CACHE_BLOCK_SIZE (gl_WorkGroupSize.x*gl_NumWorkGroups.x*uint(pageCount)) // require one reserved block 
#define CACHE_BLOCK (_cacheID*pageCount)

#ifdef ENABLE_STACK_SWITCH
#define STATE_PAGE_OFFSET 0u
#else
#define STATE_PAGE_OFFSET (CACHE_BLOCK_SIZE*currentState)
#endif


//#define VTX_PTR (currentState == BVH_STATE_TOP ? bvhBlockTop.primitiveOffset : bvhBlockIn.primitiveOffset)
#define VTX_PTR 0
int cmpt(in int ts){ return clamp(ts,0,localStackSize-1); };

void loadStack(inout int rsl) {
    [[flatten]] if (traverseState.stackPtr <= 0 && traverseState.pageID > 0 && traverseState.pageID <= pageCount) { // make store/load deferred 
        lstack = traverseCache.pages[STATE_PAGE_OFFSET + CACHE_BLOCK + (--traverseState.pageID)]; traverseState.stackPtr = localStackSize;
    };
    [[flatten]] if (sidx > 0) { rsl = lstack[cmpt(--sidx)]; };
};

void storeStack(in int rsl) {
    [[flatten]] if (sidx < localStackSize) { lstack[cmpt(sidx++)] = rsl; };
    [[flatten]] if (traverseState.stackPtr >= localStackSize && traverseState.pageID < pageCount && traverseState.pageID >= 0) { // make store/load deferred 
        traverseCache.pages[STATE_PAGE_OFFSET + CACHE_BLOCK + (traverseState.pageID++)] = lstack; traverseState.stackPtr = 0;
    };
};


// corrections of box intersection
const bvec4 bsgn = false.xxxx;
const 
float dirlen = 1.f, invlen = 1.f, bsize = 1.f;
