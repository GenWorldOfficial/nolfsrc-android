// zTools: zErrHandler::set_ef — install/replace/remove a dedicated
// error handler in the sorted handler table (reconstructed from
// ehset.obj).
//
// NEAR (ours 269 bytes, shipped 281; same instructions per path). What is left is
// block layout only: shipped keeps both refusals' `return 0` epilogues
// inline and puts the found-handler block last; ours sinks the refusals
// into one shared tail and keeps the found block inline. About thirty
// spellings (if/else nests, inverted `== -1`, goto, for-loop, a single
// result local, `return f`, NULL/cast returns, `ndh >= MAX_DEDICATED`
// in place of ok_set_ef()) all compile to this one output: VC6
// canonicalises the CFG here, so no source lever was found.
// `lasterr = 0` before `ndh++` IS source: it gives shipped's store
// order in the insert block.
#include "ztools.h"

erf_t zErrHandler::set_ef(void *ob, erf_t f)
{
	if (f && !ok_set_ef())
		return 0;

	int i;

	if (ndh)
	{
		if ((i = srch(ob)) != -1)
		{
			erf_t t = dl[i].handler;

			if (f)
			{
				dl[i].handler = f;
				return t;
			}

			memmove(&dl[slot], &dl[slot + 1],
					(char *)&dl[ndh] - (char *)&dl[slot + 1]);
			ndh--;
			return t;
		}
	}

	if (!f)
		return 0;

	if (ndh)
		memmove(&dl[slot + 1], &dl[slot],
				(char *)&dl[ndh] - (char *)&dl[slot]);

	dl[slot].handler = f;
	dl[slot].object = ob;
	dl[slot].lasterr = 0;
	ndh++;	return 0;
}
