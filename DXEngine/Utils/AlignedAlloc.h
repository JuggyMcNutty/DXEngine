#pragma once

#include <cstdlib>
#ifdef _WIN32
#include <malloc.h>
#endif

inline void* AlignedAlloc(std::size_t alignment, std::size_t size)
{
#ifdef _WIN32
	return _aligned_malloc(size, alignment);
#else
	// std::aligned_alloc requires an alignment accepted by the platform and a
	// size that is an exact multiple of that alignment. Also, macOS refuse
	// alignments smaller than sizeof(void*)
	alignment = alignment < alignof(void*) ? alignof(void*) : alignment;
	const std::size_t remainder = size % alignment;
	if (remainder != 0)
		size += alignment - remainder;
	return std::aligned_alloc(alignment, size);
#endif
}

inline void AlignedFree(void* data)
{
#ifdef _WIN32
	_aligned_free(data);
#else
	std::free(data);
#endif
}
