# toolchain/ -- the compilers (not in the repository)

The build runs the original 2000-era Windows compilers under Wine, inside
the build container. They are not redistributable, so this directory is
gitignored except for this file and `fetch.sh`, which assembles it from
public copies of the original media:

```sh
make toolchain                          # all four trees, in the build container
make shell  # then: bash toolchain/fetch.sh icl-4.0      (just one tree)
            #       bash toolchain/fetch.sh --force ...  (rebuild a tree)
```

Each download is checked against a pinned SHA-256, and each finished tree
against a digest of every file in it, so the result is byte-for-byte the
toolchain this source was matched with. A tree that is already present and
verified is left alone. The downloads (about 2 GB) are kept in
`toolchain/downloads/`, so a rerun fetches nothing; delete that directory
once the trees are built if you want the space back. `make` checks that the
four compiler executables exist before it starts.

| Directory | What | Used for | Size |
|---|---|---|---|
| `msvc600/` | Microsoft Visual C++ 6.0 SP6 command-line tools, portable layout: `VC98/` (`Bin/CL.EXE`, back end `C2.DLL` 12.00.9782, `LINK.EXE`, `LIB.EXE`, headers, CRT and MFC libraries) and `Common/MSDev98/Bin/` (`RC.EXE`) | the default compiler (game DLLs, `NOLF.exe`, `ltmsg.dll`, `sres.dll`), and the linker, librarian and resource compiler for everything | 176 MB |
| `msvc-6.00-8447/` | Visual C++ 6.0 SP3 compiler (`BIN/cl.exe`, back end `c2.dll` 12.00.8447) with its own `INCLUDE/`, `LIB/`, `MFC/` | `server.dll`, `lithtech.exe`, the lithshared libraries (`CL_REV=8447`) | 131 MB |
| `msvc-6.00-8168/` | Visual C++ 6.0 RTM compiler (`BIN/cl.exe`, back end 12.00.8168) with its own `INCLUDE/`, `LIB/`, `MFC/` | headers and libraries for the Intel compiler (`d3d.ren`) | 131 MB |
| `icl-4.0/` | Intel C/C++ Compiler 4.0 for Windows (build 99100): `BIN/icl.exe` and its companions, `INCLUDE/`, `LIB/` | `d3d.ren` | 5.4 MB |

About 450 MB in all.

`build/env.sh` is the one place these paths are spelled; it fails with a
clear message if `msvc600` is missing, and `run_icl` does the same for
`icl-4.0`. The revisions are not interchangeable: each binary is compiled
with the revision its original was built with, and a different one produces
different (though working) code.

## Where each tree comes from

| Tree | Source |
|---|---|
| `msvc-6.00-8168` | `VC98/{BIN,INCLUDE,LIB,MFC/INCLUDE,MFC/LIB}` and `COMMON/MSDEV98/BIN/MSPDB60.DLL` from Visual Studio 6.0 Enterprise, Disc 1 ([archive.org](https://archive.org/details/visualstudiov60enterpriseedition_199807)); names lower-cased. |
| `msvc-6.00-8447` | The 8168 tree with Service Pack 3's English `vc98/` files laid over it ([archive.org](https://archive.org/details/msvs6sp3)). The pressed SP3 disc lacks the new back end: it comes from the Internet/Core edition of SP3 on the August 1999 TechNet disc ([archive.org](https://archive.org/details/ms-technet-tnsb9908)), whose `msvcep.dll` is what SP3 setup installs as `c2.dll`. |
| `msvc600` | The portable VC6 SP6 tree at [github.com/itsmattkc/MSVC600](https://github.com/itsmattkc/MSVC600), pinned to commit `001c4baf`. |
| `icl-4.0` | Intel's evaluation release ([archive.org](https://archive.org/details/iclv.-4.0)): the RAR holds an InstallShield setup whose `data1.cab` has the compiler's file groups (`Compiler_Bin_Files`, `Compiler_Include_files`, `Compiler_Lib_files_*`, and `Msvcrt.dll`). The evaluation's licence check is disabled by one byte, as the archive's own `$ICL.COM` describes: `icl.exe` offset `0xA8D0`, `0x55` -> `0xC3`. `icl.exe.orig` is the unmodified file. It runs against `msvc-6.00-8168`'s headers and libraries. |

## Thanks

The way the two versioned VC6 trees are rebuilt from the original
Microsoft media -- which discs, which files, and where SP3's back end hides
-- comes from
[second-impressions/msvc-compilers](https://github.com/second-impressions/msvc-compilers).
