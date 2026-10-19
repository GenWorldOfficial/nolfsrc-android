/* nolfios.h -- the names the pre-standard iostreams had outside namespace
   std, and their ios::nocreate (open only an existing file, which is what
   input streams do anyway). */
#ifndef NOLF_NOLFIOS_H
#define NOLF_NOLFIOS_H
#include <ios>
#include <iostream>
#include <istream>
#include <ostream>
#include <fstream>
#include <strstream>
using std::istream;
using std::ostream;
using std::iostream;
using std::ifstream;
using std::ofstream;
using std::fstream;
using std::istrstream;
using std::ostrstream;
using std::strstream;
using std::strstreambuf;
using std::streampos;
using std::streamoff;
using std::filebuf;
using std::endl;
using std::ends;
using std::flush;
using std::ws;
using std::cin;
using std::cout;
using std::cerr;
// ios:: as a scope: everything std::ios has, plus the old flags
struct ios : public std::ios {
    static const std::ios_base::openmode nocreate = std::ios_base::openmode(0);
    static const std::ios_base::openmode noreplace = std::ios_base::openmode(0);
private:
    ios();
};
#endif
