# Bugs in the retail game

Bugs in *No One Lives Forever* (GOTY Edition, version 1.003) as it shipped,
found while reconstructing it. Each is a mistake in the code that a source
change would fix.

The release build reproduces every one of them, on purpose: compiling to the
retail machine code means compiling its mistakes too. They are fixed in the
**modern build** (`make modern`, the `NOLFSRC_MODERN` define; see
`BUILDING.md`, "Variant builds"): each fix goes behind `#ifdef
NOLFSRC_MODERN`, with the retail code kept in the `#else`, so the release
build is unchanged and still matches. This is step 3 of the README's roadmap.

An entry that is fixed says so under its heading, with the file the fix is
in.

**Fixed so far** (in the modern build):

* [`CSoundMgr::GetFilterIndex`: the name comparison is inverted](#csoundmgrgetfilterindex-the-name-comparison-is-inverted)

Every entry names the retail address it was read from, the binary, and the
file that reproduces it. An entry is listed only once its function is
byte-identical, or a known near match, so it describes what the retail code
does, not a guess. Addresses are as in the retail images
(`build/reference.sh` extracts them into `out/reference/`).

---

## Wrong results

### The property-type guard normalises to a value that is also out of range
`0x100016be` (server.dll) · `src/engine/server/counter.cpp`

Walking a freshly loaded object DLL's class table, every `PropDef::m_PropType`
outside `PT_STRING`..`PT_ROTATION` is rewritten to 8, which is one past
`PT_ROTATION` and so not a valid type either. A level with a corrupt property
type is silently given a type nothing can interpret, rather than being
refused.

### `ar_FindFile` closes a find handle it never opened
`0x1000706a` (server.dll) · `src/engine/server/filemgr.cpp`

`_findclose` is called with the handle even when `_findfirst` returned -1:
the close sits after the `if`, not inside it.

### `CSoundMgr::GetFilterIndex`: the name comparison is inverted
`0x00482fd0` (lithtech.exe) · `src/engine/lithtech/clientsound.cpp`

> **✅ FIXED in the modern build** (`NOLFSRC_MODERN`):
> `src/engine/lithtech/clientsound.cpp` tests `stricmp(...) == 0`. The
> release build keeps the retail test.

The function answers "which index is the filter called this?" by walking
Miles' filter enumeration, and the comparison is the wrong way round:

```c
if (stricmp(pFilter, pName))
    return LT_OK;      /* names DIFFER -> report found */

(*pIndex)++;           /* names MATCH  -> keep walking */
```

So the first filter whose name is *not* the one asked for is reported as
found, and a name that matches makes the walk carry on past it.
`CSoundMgr::GetFilterParamName` (`0x004830d0`) does the same search and gets
it right; this one is missing `== 0`.

### `CSoundMgr::GetReverbProperties`: damping is guarded by the wrong flag
`0x00482230` (lithtech.exe) · `src/engine/lithtech/clientsound.cpp`

`ReverbProperties::m_dwParams` says which fields the caller wants. The getter
tests bit 1 for the volume, 2 for the room type, 4 for the reflection time,
8 for the decay time, and 8 again for the damping, which is bit 0x10's.
`SetReverbProperties` (`0x00482010`) uses 0x10, so a caller can set the
damping but cannot read it back unless it also asks for the decay time.

### `CString::Concat(LPCTSTR)`: appending leaves the length short
`MFCStub.lib` (also in `ButeMgrNoMFC.lib`) · `src/lithshared/MFCStub/mfcs_string.cpp`

It expands the buffer, copies the new characters in and returns, without
calling `SetLength`. The stored length stays what `ExpandBuffer` carried over,
and `GetLength()` under-reports the string from then on.

### `CBaseStream::WriteStream`: the source offset is ignored
`0x10011402` (server.dll) · `src/engine/server/basestream.cpp`

The function copies a range out of another stream, starting at `dwMin`.
`dwMin` is used to work out how many bytes to copy and for nothing else: the
body never seeks the source stream (no call through its `SeekTo` slot), so
the bytes are read from wherever the source happened to be. Every caller in
the game passes `WRITESTREAM_DEFAULT`, which is why it never shows.

### The compressed-vector codec does not round-trip
`0x10012d4e` writer, `0x10014830` reader (server.dll) ·
`src/engine/server/servermsg.cpp`, `src/engine/server/lmessage.cpp`

The writer scales each minor-axis fraction by `262143.0` (2^18 - 1); the
reader multiplies by `3.814697265625e-06`, which is 2^-18 (1/262144). They
are not reciprocals, so every decoded vector comes back short by one part in
262144 on both minor axes.

### `wm_ResetGeometry`: wrong block centres, unguarded divisions, and a missing null check
`0x1000a411` (server.dll) · `src/engine/server/worldmodels.cpp`

Three faults in one function:

* A block's centre is the sum of its vertices divided by a count that is
  summed over the block's polygons, not over its distinct vertices. A vertex
  shared by two polygons is added once and counted twice, so the centre is
  pulled toward the most-shared corners.
* That division (`0x1000a4ae`) is never checked. The loop above it skips null
  and vertex-less polygons, so a block with none that count divides 0 by 0;
  the radius loop then compares against a NaN, and the block ends up with a
  garbage centre and a radius of 5.0. The per-polygon division by
  `m_nVertices` (`0x1000a5bd`) is unguarded too.
* The block half checks each `m_ppPolies` entry for null (`0x1000a4df`); the
  polygon half dereferences the same array (`0x1000a562`) without checking.

### `SharedCommonLT::GetAttachmentTransform` asks the socket accessor for a node
`0x10012378` (server.dll) · `src/engine/server/sharedcommon.cpp`

For an attachment to a model node, the function means to call
`ILTModel::GetNodeTransform` (vtable slot `+0x18`) with
`Attachment::m_iNode`. It calls through `+0x1c` (`0x100123ff`), which is
`GetSocketTransform`: the retail `ILTModel` table at `0x1004e5f4` has the two
at slots 6 and 7, and each names itself in its own parameter check
(`0x1001d478`, `0x1001d77f`). So the node index is looked up as a socket
index. (The source reproduces it by calling through `ILTModelI` in
`commonmodel.h`, which declares an extra leading slot.) The neighbouring `GetAttachedTransform` (`0x100124ce`)
picks the two slots correctly.

### The box-intersect test reports the wrong plane on three of six faces
`0x1000f1f7` (server.dll) · `src/engine/server/objbank.cpp`

`w_IsectObjectShape` builds all six faces of the object's bounding box. The
three negative faces are right: the normal points along -axis and the distance
is the box's minimum, negated. The three positive faces set the normal to +1
along the axis and then take the distance from the minimum as well:

    1000f358:  fld1                     ; +1.0f, into the normal
    1000f366:  mov    0xf0(%ecx),%ecx   ; m_BBoxMin.x, not m_BBoxMax.x
    1000f640:  mov    %ecx,0xc(%eax)    ; m_Dist = min.x

(`0xf4` on the +Y face and `0xf8` on +Z are the same slip.) A hit on the far
side of any object comes back with a plane through the near face, displaced
by the width of the box, although the intersection point itself is right.
Anything that reflects or slides along the reported plane gets a surface in
the wrong place.

### `LTMatrix::TransformPlane` never transforms the point it dots
`0x1000f74e` (server.dll) · `src/engine/server/objbank.cpp`

The plane transform inlined into `w_IsectObjectGeometry` is
`LTMatrix::TransformPlane` from a version of `ltmatrix.h` that is one line
shorter than the SDK's (`src/LT2/sdk/inc/ltmatrix.h`):

    startPt = thePlane.m_Normal * thePlane.m_Dist;
    MatVMul_3x3(&ret.m_Normal, this, &thePlane.m_Normal);
    MatVMul_InPlace_H(this, &startPt);        // <-- missing in the retail code
    ret.m_Dist = ret.m_Normal.Dot(startPt);

So the new distance is the transformed normal dotted with a point still in
the object's own space: the reported plane is right only when the transform
has no translation. (The source writes the shorter version out in place,
since calling the header would emit the missing line.)

### `Model::LoadChildModels` reports success when it cannot size one table
`0x1001c604` (server.dll) · `src/engine/server/commonmodel.cpp`

Every failure in the function goes to the exit that returns the `LT_ERROR`
recorded on entry, except one: when sizing the table at `+0x8c` fails, it
jumps to the success return instead:

    1001c6c3:  call   0x10019f29        ; SetSize2(...)
    1001c6c8:  test   %eax,%eax
    1001c6ca:  je     0x1001cb25        ; 0x1001cb25 is `xor %eax,%eax`

A model whose table cannot be allocated loads as `LT_OK` with none of its
pieces, nodes, child models or animations read.

### `ServerMessage::WriteData` clips the count, then grows the buffer anyway
`0x1002616a` (server.dll) · `src/engine/server/servermsg.cpp`

The byte count is clipped against `m_MaxDataLen` first, and the loop after it
grows the allocation until the write fits. A write longer than `m_MaxDataLen`
silently loses its tail, while the buffer is enlarged to a size that would
have held it.

### `CBaseStream::ReadString` never reports a short read
`0x10011314` (server.dll) · `src/engine/server/basestream.cpp`

The string is read in two parts, a two-byte length and then the body, and
only the body read's result is returned. A stream that runs out between the
two returns `LT_OK` with an empty string. (A caller that checks
`ErrorStatus` afterwards, as the SDK advises, still finds out.)

### `CLTDirectMusicMgr::Stop` discards the queue it was asked to wait for
`0x00440480` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp`

`Stop(LTDMEnactNextSegment)` means "stop when the current segment ends". The
enact type is honoured only for the primary segment, which is left to run out;
the pending queue is emptied at once, and the secondary segments and motifs
are stopped immediately, whatever was asked.

### `StopSecondary` and `StopMotif` test the wrong object when given a name
`0x00440ae0`, `0x00440dc0` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp`

Stopping everything asks each node `pNode->m_pSegState` (is it playing?);
stopping one by name asks `pNode->m_pSegment->m_pDMSegment` (is it loaded?)
instead. The by-name loop re-runs `FindByName` until nothing is found and
`StopNode` does nothing to a node that is not playing, so a node that is
loaded but not playing would be found forever: a hang. Nothing reaches that
state today (a node is only added once DirectMusic has returned a segment
state), so the wrong test is the only thing between the loop and the hang.

---

## Crashes and corruption: checks that are missing

### `CObjBase::InitFields` leaves `m_StandingLink` uninitialised
`0x10024802` (server.dll) · `src/engine/server/objtypes.cpp`

Of the five links the function sets up, `m_StandingLink` (`+0x114`) is the
only one given its `m_pData` and not tied off, so an object that has never
stood on anything carries a link whose `m_pPrev`/`m_pNext` are whatever the
object bank's memory held. (It also ties off the link at `+0x060` twice,
which is harmless.)

### The fragment-record allocator passes a null to the list splice
`0x10023495` (server.dll) · `src/engine/server/netmgr.cpp`

When the six-record cap is not reached the function does `new CNetFragRec`,
and on failure (`xor %edi,%edi` at `0x100234f2`) both paths still call
`LMessageList::Append` with the result. `Append` writes through its argument
either way (`0x10024311`, `0x100242f5`), so running out of memory here is a
write through null, not a dropped fragment.

### `ws_AddClippedContact` divides by a dot product that can be zero
`0x10004512` (server.dll) · `src/engine/server/worldsim.cpp`

The divisor reduces to the dot product of the move with the contact plane's
normal. A move exactly parallel to a contact plane divides by zero, and the
infinity or NaN is copied into the object's position.

### `w_IsPolyBlocking` has no bounds checks
`0x10001cad` (server.dll) · `src/engine/server/worldsim.cpp`

The clip pool (200 vertices) and the working pointer list (200) are
allocated across all six clip rounds and never reset, and the input list
holds 40 pointers filled straight from `m_nVertices`. A polygon with more than
40 vertices, or a clip sequence producing more than 200 new vertices, writes
past the end of the stack frame (`0x10001d36`-`0x10001e6d`).

### `at_Advance` divides by the last keyframe's time
`0x100012b6` (server.dll) · `src/engine/server/modelanim.cpp`

A looping animation past its end wraps with `m_Cur.m_Time %= nLength`, where
`nLength` is the last keyframe's time. The only guard is `nKeyFrames > 1`, so
an animation with several keyframes all at time 0 divides by zero. (The
blend-percentage division just before it, `0x10001320`, does check.)

### `sm_WriteObjectFiles` dereferences the first file identifier unchecked
`0x10029087` (server.dll) · `src/engine/server/servercommon.cpp`

Of the three file identifiers it sends, the first is read through
`m_pSData->m_pFiles[0]->m_FileID` (`0x10029092`) with no null test; the other
two are tested and replaced by `0xffff` when missing. An object with no model
faults here.

### `ci_SetLightRadius` does not check its object handle
`0x0040bf30` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

Every other accessor in the client interface checks the handle for null; this
one reads the object type straight off it, so a null handle is a read of
address `0xbc`.

### `ci_GetSurfaceDims` writes through unchecked pointers
`0x0040cfd0` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

Given a surface, it checks both output pointers and fills in whichever were
asked for. Given a null surface, it zeroes both without checking either.

### `ci_GetModelNodeName` refuses node 0 and does not terminate
`0x004363f0` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

The node handle is tested against null before it is tested against the node
count, so node 0, a valid node, is refused as an invalid parameter. The copy
is `strncpy(pName, ..., maxLen - 1)` with nothing writing the terminator, so
a name of `maxLen - 1` characters or more comes back unterminated.

### The tree-path builder ignores the buffer length it is given
`0x100070b6` (server.dll) · `src/engine/server/filemgr.cpp`

The fourth argument, `nOutLen`, is the size of the caller's buffer; the body
is `sprintf(pOutPath, "%s\\%s", pTree->m_szDir, pFilename)` and never reads
it, so a long path overruns the buffer (`cb_LoadServerObjects` passes 256
bytes).

### The object-DLL temp name: one copy unterminated, two unbounded
`0x1000e05b` (server.dll) · `src/engine/server/counter.cpp`

Extracting an object DLL to a temporary file, the `GetTempFileName` fallback
copies with `strncpy(pOutPath, sUnique, nOutLen - 1)` and nothing writes the
terminator, which `strncpy` does not add when the source fills the count. The
other two paths `sprintf` into the caller's buffer without any bound.

### `ServerMessage::ReadData` and `WriteData` bound-check a truncated size
`0x100260d9`, `0x1002616a` (server.dll) · `src/engine/server/servermsg.cpp`

Both compare the position against `(uint16)m_Data.m_AllocSize`, a 16-bit
truncation of a 32-bit field (`0x1002619e`, `0x10026102`), while
`CPacket::WriteType` (`0x10013957`) compares the same field as a full 32-bit
value. Only the `0x1000`-byte cap on packets keeps it from mattering.

---

## Leaks

### `ci_GetRenderModes` never closes its find handle
`0x0042de00` (lithtech.exe) · `src/engine/lithtech/clientinterface.cpp`

It walks every `*.ren` beside the executable with `_findfirst`/`_findnext`
and has no `_findclose` on any path.

### `CLTDirectMusicMgr::InitLevel` leaks the path it built when the control file will not open
`0x0043fcd0` (lithtech.exe) · `src/engine/lithtech/clientmusic.cpp`

For a control-file name with no directory, the function allocates a full path
(`0x0043fde5`) and frees it only when the reader's `Init` succeeds
(`0x0043fecb`). The failure path returns at `0x0043fec8` without freeing it.

### `streamsim_Open` leaks the file when the stream cannot be allocated
`0x10035a8d` (server.dll) · `src/engine/server/streamsim.cpp`

`fopen` succeeds, the stream object fails to allocate, and the function
returns null without closing the file.

### `se_ModelHashCleanup` uses a global instead of its argument, and leaks
`0x1002af75` (server.dll) · `src/engine/server/serverengine.cpp`

The callback exists to free a dependency record, using the manager it is
handed. It reads the global manager pointer (`0x10058bf0`) instead, and when
that is null the whole body is skipped, so the eight-byte record is lost.
(`se_LoadChildModel` stores the manager in the record, and nothing reads it.)

---

## Wrong diagnostics

Minor: these change what the engine reports, not what it does.

### Error messages that name the wrong function
The engine's parameter checks report through a macro taking the function's
own name. These were written by copying a neighbour, and the name was never
changed:

| function | address | reports itself as |
|---|---|---|
| `ci_GetParticleTotalLifetime` | `0x0040b750` (lithtech.exe) | `CLTClient::GetParticleLifetime` |
| `ci_FreeServiceList` | `0x004081e0` (lithtech.exe) | `CLTClient::GetServiceList` |
| `ci_SetSurfaceAlpha` | `0x0040ecc0` (lithtech.exe) | `ILTClient::GetSurfaceAlpha` |
| `ci_UnoptimizeSurface` | `0x0040cdf0` (lithtech.exe) | `OptimizeSurface` |
| `ci_RenderObjects` | `0x00408f90` (lithtech.exe) | `RenderCamera`, at three of its four checks |
| `ci_GetModelNodeName` | `0x004363f0` (lithtech.exe) | `GetNextModelNode`, at both |
| `CPhysicsLT::SetObjectDims` | `0x00405000` (lithtech.exe) | `SPhysicsLT::SetObjectDims`, at the first check (which also uses the server's debug level, 2, where the next uses the client's 1) |
| `CLTServer::UnloadModel`, `UnloadTexture` | server.dll | `ThreadLoadTexture`, in both |

### Checks reported at the wrong debug level
A check's message prints only when `g_DebugLevel` is at least the level it
names, and in the client interface that is almost always 1:

* `ci_ScaleSurfaceToSurface` (`0x0040dc50`) reports at 4, where its seven
  siblings in the blit family use 1.
* `ci_RenderCamera` (`0x00408ea0`) reports all three checks at 15, which
  nothing sets, so they can never print.
* `ci_GetPolyGridInfo` (`0x0040bc80`) reports at 0, so it always prints.

### The value-returning readers quote the shutdown message
`0x0041f150`-`0x0041fbd0` (lithtech.exe), and their server.dll twins ·
`src/engine/server/csbase.cpp`

`ILTCSBase`'s error reporter takes a description as its last argument. Every
method returning `LTRESULT` passes a single space; every method returning
void or a value passes the engine's pending shutdown-message buffer (the one
`ci_Shutdown` hands to `ci_ShutdownWithMessage`). It is empty in practice; if
a shutdown message were ever pending, all twenty-nine of those errors would
quote it.

### `CMemStream::Read`'s debug line reads a missing argument
`0x10007854` (server.dll) · `src/engine/server/fsstream.cpp`

The format is `"read from stream %p size %u"` and only `this` is passed, so
`%u` prints whatever is next on the stack. It fires only at stream debug level
4 or above; `CTreeStream::Read` passes both.

### `InitLevel`'s trace misspells a parameter
`0x004ab834` (lithtech.exe, the string)

`... sDefine1=%s sDefine2=%s sDeine3=%s`: `sDeine3` for `sDefine3`, in every
trace the music system's level setup writes at debug level 3.
