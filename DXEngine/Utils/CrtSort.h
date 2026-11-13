#pragma once

#include <cstddef>
#include <utility>

// The C runtime's qsort, which the original's appQsort calls (Core.dll
// 0x10123640, MSVCRT's qsort), for the order it leaves equal elements in: a
// run of up to 8 sorted by moving its largest -- the first of equals -- to
// its end, again and again; a longer one split round the middle of its
// first, middle and last elements, those three put in order first, the two
// parts kept on a stack, the larger pushed first. Wine's makes the
// comparisons and moves Windows' (Vista and later) makes, as its tests of
// native's assert. compare takes pointers into the array, as the original's
// comparisons are given them, and answers under 0, 0 or over 0.
template<typename T, typename Compare>
void CrtQsort(T* x, size_t count, Compare compare)
{
	if (count < 2)
		return;
	size_t stackLo[64], stackHi[64];
	int pos = 0;
	stackLo[0] = 0;
	stackHi[0] = count - 1;
	while (pos >= 0)
	{
		size_t beg = stackLo[pos];
		size_t end = stackHi[pos--];
		if (end - beg < 8)
		{
			for (size_t last = end; last > beg; last--)
			{
				size_t max = beg;
				for (size_t i = beg + 1; i <= last; i++)
				{
					if (compare(&x[i], &x[max]) > 0)
						max = i;
				}
				if (max != last)
					std::swap(x[max], x[last]);
			}
			continue;
		}

		size_t lo = beg, hi = end, med = lo + (hi - lo + 1) / 2;
		if (compare(&x[lo], &x[med]) > 0)
			std::swap(x[lo], x[med]);
		if (compare(&x[lo], &x[hi]) > 0)
			std::swap(x[lo], x[hi]);
		if (compare(&x[med], &x[hi]) > 0)
			std::swap(x[med], x[hi]);
		lo++;
		hi--;
		for (;;)
		{
			while (lo <= hi && (lo == med || compare(&x[lo], &x[med]) <= 0))
				lo++;
			while (med != hi && compare(&x[hi], &x[med]) > 0)
				hi--;
			if (hi < lo)
				break;
			std::swap(x[lo], x[hi]);
			if (hi == med)
				med = lo;
			lo++;
			hi--;
		}
		while (hi > beg && (hi == med || compare(&x[hi], &x[med]) == 0))
			hi--;

		if (hi - beg >= end - lo)
		{
			stackLo[++pos] = beg;
			stackHi[pos] = hi;
			stackLo[++pos] = lo;
			stackHi[pos] = end;
		}
		else
		{
			stackLo[++pos] = lo;
			stackHi[pos] = end;
			stackLo[++pos] = beg;
			stackHi[pos] = hi;
		}
	}
}
