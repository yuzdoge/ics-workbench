#ifndef __TRACE_H__
#define __TRACE_H__

#include "log.h"

#define ALEN_FMT
#define DLEN_FMT
#define MTRACE_COMMON_FMT "ADDR=[%08lx], WDATA=[%08x], WMASK=[%08x]; " \
"WAY=[%02d], OLD LINE:[STAT:%08x | BASE:%08lx | OFF:%08x]=[%08x] -> " \
"NEW LINE:[STAT:%08x | BASE:%08lx | OFF:%08x]=[%08x]"

#define MTRACE_WDIRTY_FMT "Write dirty block back to Memory Address: %08lx"

#define MTRACE_WDIRTY(blknum) LOG("\n"MTRACE_WDIRTY_FMT"----V\n", blknum);

#define MTRACE_R(addr, way, ostat, obase, off, odata, nstat, nbase, ndata) \
LOG("RW=[R], "MTRACE_COMMON_FMT"\n", addr, 0, 0, way, ostat, obase, off, odata, nstat, nbase, off, ndata)

#define MTRACE_W(addr, wd, wmsk, way, ostat, obase, off, odata, nstat, nbase, ndata) \
LOG("RW=[W], "MTRACE_COMMON_FMT"\n", addr, wd, wmsk, way, ostat, obase, off, odata, nstat, nbase, off, ndata)


#endif
