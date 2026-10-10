// *********************************************************************** //
//
//  w12_7fae0.cpp -- d3d.ren's default allocator object, g_DefAlloc
//  (0x10091df0), and its three virtuals.
//
//  0x1007faa0..0x1007fb1d is ONE translation unit of Monolith's StdLith
//  library, l_allocator.cpp (src/LT2/lithshared/stdlith/l_allocator.h):
//
//      class LAlloc {
//          virtual       ~LAlloc() {}
//          virtual void* Alloc(DWORD size) { return DefStdlithAlloc(size); }
//          virtual void  Free(void *ptr)   { DefStdlithFree(ptr); }
//      };
//      LAlloc g_DefAlloc;
//
//    0x1007faa0  the static-init entry: call 0x1007fab0; jmp 0x1007fac0
//    0x1007fab0  g_DefAlloc's constructor, inlined: store the vtable
//                0x100881b8 into 0x10091df0
//    0x1007fac0  atexit(0x1007fad0)
//    0x1007fad0  its destructor, inlined: store the vtable again
//    0x1007fae0  LAlloc::Alloc    `push size; call 0x10021ce0; ret $4`
//    0x1007faf0  LAlloc::Free     `push ptr;  call 0x10021cb0; ret $4`
//    0x1007fb00  LAlloc's scalar deleting destructor (vtable slot 0)
//    vtable 0x100881b8 = { 0x1007fb00, 0x1007fae0, 0x1007faf0 }
//
//  src/lithshared/out/StdLith.lib's l_allocator.obj reproduces all three
//  virtuals byte-identical (0x1007fae0, 0x1007faf0, 0x1007fb00),
//  so this block was VC6-compiled library code, not icl. It is NOT linked
//  from the library, though: the recovered l_allocator.cpp also DEFINES
//  DefStdlithAlloc/DefStdlithFree (malloc/free), and in d3d.ren those two
//  are the renderer's own functions -- LAlloc::Alloc calls 0x10021ce0 and
//  LAlloc::Free calls 0x10021cb0 (d3d_Free), both of which route through
//  the engine's allocator interface at 0x1008cbcc. Pulling the library
//  member would hand every StdLith allocation to the CRT heap while the
//  renderer frees through the engine: a mismatched heap at run time. So
//  the class is compiled here, under the spelling the d3d objects already
//  use (CD3DAlloc, d3dr5b.cpp), calling the renderer's two functions.
//  icl's bytes are not the shipped VC6 bytes; the behaviour is.
//
//  (StdLith.lib IS linked for struct_bank.obj and dynarray.obj -- see the
//  d3d-link target -- and their DefStdlithAlloc/DefStdlithFree references
//  are aliased onto 0x10021ce0/0x10021cb0 in link/aliases.rsp.)
//
// *********************************************************************** //

typedef unsigned long uint32;

void* sub_10021ce0(uint32 nSize);                        // 0x10021ce0 DefStdlithAlloc
void  sub_10021cb0(void* p);                             // 0x10021cb0 DefStdlithFree / d3d_Free

class CD3DAlloc
{
public:
	virtual       ~CD3DAlloc() {}                        // vtbl +0x00 (0x1007fb00)
	virtual void* Alloc(uint32 nSize);                   // vtbl +0x04 (0x1007fae0)
	virtual void  Free(void* pData);                     // vtbl +0x08 (0x1007faf0)
};

// 0x1007fae0
void* CD3DAlloc::Alloc(uint32 nSize)
{
	return sub_10021ce0(nSize);
}

// 0x1007faf0
void CD3DAlloc::Free(void* pData)
{
	sub_10021cb0(pData);
}

// 0x10091df0 -- g_DefAlloc. Its static constructor/destructor pair is
// 0x1007faa0..0x1007fada.
CD3DAlloc g_10091df0;
