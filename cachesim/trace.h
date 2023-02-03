#ifndef __TRACE_H__
#define __TRACE_H__

#include "log.h"

#define MTRACE_COMMON_FMT "ADDR=[%08lx], WDATA=[%04x], WMASK=[%04x]; " \
"WAY=[%02d], OLD LINE:[STAT:%04x | BASE:%08lx | OFF:%04x]=[%04x] -> " \
"NEW LINE:[STAT:%04x | BASE:%08lx | OFF:%04x]=[%04x]"

#define MTRACE_WDIRTY_FMT "Write dirty block back to Memory Address: %08lx"

#define MTRACE_WDIRTY(blknum) LOG("\t"MTRACE_WDIRTY_FMT"\n", blknum);

#define MTRACE_R(addr, way, ostat, obase, off, odata, nstat, nbase, ndata) \
LOG("RW=[R], "MTRACE_COMMON_FMT"\n", addr, 0, 0, way, ostat, obase, off, odata, nstat, nbase, off, ndata)

#define MTRACE_W(addr, wd, wmsk, way, ostat, obase, off, odata, nstat, nbase, ndata) \
LOG("RW=[W], "MTRACE_COMMON_FMT"\n", addr, wd, wmsk, way, ostat, obase, off, odata, nstat, nbase, off, ndata)


#endif
