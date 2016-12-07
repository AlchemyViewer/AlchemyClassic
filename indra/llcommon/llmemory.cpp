// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llmemory.cpp
 * @brief Very special memory allocation/deallocation stuff here
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"


#if defined(LL_WINDOWS)
#include "llwin32headerslean.h"
# include <psapi.h>
#elif defined(LL_DARWIN)
# include <sys/types.h>
# include <mach/task.h>
# include <mach/mach_init.h>
#elif LL_LINUX
# include <unistd.h>
#endif

#include "llmemory.h"

#include "llthread.h"

#include "llsys.h"
#include "llframetimer.h"
#include "lltrace.h"
//----------------------------------------------------------------------------

//static
char* LLMemory::reserveMem = 0;
U32Kilobytes LLMemory::sAvailPhysicalMemInKB(U32_MAX);
U32Kilobytes LLMemory::sMaxPhysicalMemInKB(0);
static LLTrace::SampleStatHandle<F64Megabytes> sAllocatedMem("allocated_mem", "active memory in use by application");
static LLTrace::SampleStatHandle<F64Megabytes> sVirtualMem("virtual_mem", "virtual memory assigned to application");
U32Kilobytes LLMemory::sAllocatedMemInKB(0);
U32Kilobytes LLMemory::sAllocatedPageSizeInKB(0);
U32Kilobytes LLMemory::sMaxHeapSizeInKB(U32_MAX);
BOOL LLMemory::sEnableMemoryFailurePrevention = FALSE;

void ll_assert_aligned_func(uintptr_t ptr,U32 alignment)
{
#if defined(LL_WINDOWS) && defined(LL_DEBUG_BUFFER_OVERRUN)
	//do not check
	return;
#else
	#ifdef SHOW_ASSERT
		// Redundant, place to set breakpoints.
		if (ptr%alignment!=0)
		{
			LL_WARNS() << "alignment check failed" << LL_ENDL;
		}
		llassert(ptr%alignment==0);
	#endif
#endif
}

//static
void LLMemory::initClass()
{
	if (!reserveMem)
	{
		reserveMem = new char[16*1024]; // reserve 16K for out of memory error handling
	}
}

//static
void LLMemory::cleanupClass()
{
	delete [] reserveMem;
	reserveMem = NULL;
}

//static
void LLMemory::freeReserve()
{
	delete [] reserveMem;
	reserveMem = NULL;
}

//static 
void LLMemory::initMaxHeapSizeGB(F32Gigabytes max_heap_size, BOOL prevent_heap_failure)
{
	sMaxHeapSizeInKB = max_heap_size;
	sEnableMemoryFailurePrevention = prevent_heap_failure ;
}

//static 
void LLMemory::updateMemoryInfo(bool for_cache) 
{
#if LL_WINDOWS	
	HANDLE self = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS counters;
	
	if (!GetProcessMemoryInfo(self, &counters, sizeof(counters)))
	{
		LL_WARNS() << "GetProcessMemoryInfo failed" << LL_ENDL;
		return ;
	}

	sAllocatedMemInKB = (U32Bytes)(counters.WorkingSetSize) ;
	sample(sAllocatedMem, sAllocatedMemInKB);
	sAllocatedPageSizeInKB = (U32Bytes)(counters.PagefileUsage) ;
	sample(sVirtualMem, sAllocatedPageSizeInKB);

	if (!for_cache)
	{
		U32Kilobytes avail_phys, avail_virtual;
		LLMemoryInfo::getAvailableMemoryKB(avail_phys, avail_virtual);
		sMaxPhysicalMemInKB = llmin(avail_phys + sAllocatedMemInKB, sMaxHeapSizeInKB);

		if (sMaxPhysicalMemInKB > sAllocatedMemInKB)
		{
			sAvailPhysicalMemInKB = sMaxPhysicalMemInKB - sAllocatedMemInKB;
		}
		else
		{
			sAvailPhysicalMemInKB = U32Kilobytes(0);
		}
	}
#else
	//not valid for other systems for now.
	sAllocatedMemInKB = (U32Bytes)LLMemory::getCurrentRSS();
	sMaxPhysicalMemInKB = (U32Bytes)U32_MAX ;
	sAvailPhysicalMemInKB = (U32Bytes)U32_MAX ;
#endif
}

//
//this function is to test if there is enough space with the size in the virtual address space.
//it does not do any real allocation
//if success, it returns the address where the memory chunk can fit in;
//otherwise it returns NULL.
//
//static 
void* LLMemory::tryToAlloc(void* address, U32 size)
{
#if LL_WINDOWS
	address = VirtualAlloc(address, size, MEM_RESERVE | MEM_TOP_DOWN, PAGE_NOACCESS) ;
	if(address)
	{
		if(!VirtualFree(address, 0, MEM_RELEASE))
		{
			LL_ERRS() << "error happens when free some memory reservation." << LL_ENDL ;
		}
	}
	return address ;
#else
	return (void*)0x01 ; //skip checking
#endif	
}

//static 
void LLMemory::logMemoryInfo(BOOL update)
{
	if(update)
	{
		updateMemoryInfo() ;
	}

	LL_INFOS() << "Current allocated physical memory(KB): " << sAllocatedMemInKB << LL_ENDL ;
	LL_INFOS() << "Current allocated page size (KB): " << sAllocatedPageSizeInKB << LL_ENDL ;
	LL_INFOS() << "Current availabe physical memory(KB): " << sAvailPhysicalMemInKB << LL_ENDL ;
	LL_INFOS() << "Current max usable memory(KB): " << sMaxPhysicalMemInKB << LL_ENDL ;
}

//return 0: everything is normal;
//return 1: the memory pool is low, but not in danger;
//return -1: the memory pool is in danger, is about to crash.
//static 
bool LLMemory::isMemoryPoolLow()
{
	static const U32Megabytes LOW_MEMORY_POOL_THRESHOLD(64);
	const static U32Megabytes MAX_SIZE_CHECKED_MEMORY_BLOCK(64);
	static void* last_reserved_address = NULL ;

	if(!sEnableMemoryFailurePrevention)
	{
		return false ; //no memory failure prevention.
	}

	if(sAvailPhysicalMemInKB < (LOW_MEMORY_POOL_THRESHOLD / 4)) //out of physical memory
	{
		return true ;
	}

	if(sAllocatedPageSizeInKB + (LOW_MEMORY_POOL_THRESHOLD / 4) > sMaxHeapSizeInKB) //out of virtual address space.
	{
		return true ;
	}

	bool is_low = (S32)(sAvailPhysicalMemInKB < LOW_MEMORY_POOL_THRESHOLD 
						|| sAllocatedPageSizeInKB + LOW_MEMORY_POOL_THRESHOLD > sMaxHeapSizeInKB) ;

	//check the virtual address space fragmentation
	if(!is_low)
	{
		if(!last_reserved_address)
		{
			last_reserved_address = LLMemory::tryToAlloc(last_reserved_address, MAX_SIZE_CHECKED_MEMORY_BLOCK.value()) ;
		}
		else
		{
			last_reserved_address = LLMemory::tryToAlloc(last_reserved_address, MAX_SIZE_CHECKED_MEMORY_BLOCK.value()) ;
			if(!last_reserved_address) //failed, try once more
			{
				last_reserved_address = LLMemory::tryToAlloc(last_reserved_address, MAX_SIZE_CHECKED_MEMORY_BLOCK.value()) ;
			}
		}

		is_low = !last_reserved_address ; //allocation failed
	}

	return is_low ;
}

//static 
U32Kilobytes LLMemory::getAvailableMemKB() 
{
	return sAvailPhysicalMemInKB ;
}

//static 
U32Kilobytes LLMemory::getMaxMemKB() 
{
	return sMaxPhysicalMemInKB ;
}

//static 
U32Kilobytes LLMemory::getAllocatedMemKB() 
{
	return sAllocatedMemInKB ;
}

//----------------------------------------------------------------------------

#if defined(LL_WINDOWS)

U64 LLMemory::getCurrentRSS()
{
	HANDLE self = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS counters;
	
	if (!GetProcessMemoryInfo(self, &counters, sizeof(counters)))
	{
		LL_WARNS() << "GetProcessMemoryInfo failed" << LL_ENDL;
		return 0;
	}

	return counters.WorkingSetSize;
}

//static 
U32 LLMemory::getWorkingSetSize()
{
    PROCESS_MEMORY_COUNTERS pmc ;
	U32 ret = 0 ;

    if (GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof(pmc)) )
	{
		ret = pmc.WorkingSetSize ;
	}

	return ret ;
}

#elif defined(LL_DARWIN)

/* 
	The API used here is not capable of dealing with 64-bit memory sizes, but is available before 10.4.
	
	Once we start requiring 10.4, we can use the updated API, which looks like this:
	
	task_basic_info_64_data_t basicInfo;
	mach_msg_type_number_t  basicInfoCount = TASK_BASIC_INFO_64_COUNT;
	if (task_info(mach_task_self(), TASK_BASIC_INFO_64, (task_info_t)&basicInfo, &basicInfoCount) == KERN_SUCCESS)
	
	Of course, this doesn't gain us anything unless we start building the viewer as a 64-bit executable, since that's the only way
	for our memory allocation to exceed 2^32.
*/

// 	if (sysctl(ctl, 2, &page_size, &size, NULL, 0) == -1)
// 	{
// 		LL_WARNS() << "Couldn't get page size" << LL_ENDL;
// 		return 0;
// 	} else {
// 		return page_size;
// 	}
// }

U64 LLMemory::getCurrentRSS()
{
	U64 residentSize = 0;
	task_basic_info_data_t basicInfo;
	mach_msg_type_number_t  basicInfoCount = TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&basicInfo, &basicInfoCount) == KERN_SUCCESS)
	{
		residentSize = basicInfo.resident_size;

		// If we ever wanted it, the process virtual size is also available as:
		// virtualSize = basicInfo.virtual_size;
		
//		LL_INFOS() << "resident size is " << residentSize << LL_ENDL;
	}
	else
	{
		LL_WARNS() << "task_info failed" << LL_ENDL;
	}

	return residentSize;
}

U32 LLMemory::getWorkingSetSize()
{
	return 0 ;
}

#elif defined(LL_LINUX)

U64 LLMemory::getCurrentRSS()
{
	static const char statPath[] = "/proc/self/stat";
	LLFILE *fp = LLFile::fopen(statPath, "r");
	U64 rss = 0;

	if (fp == NULL)
	{
		LL_WARNS() << "couldn't open " << statPath << LL_ENDL;
		goto bail;
	}

	// Eee-yew!	 See Documentation/filesystems/proc.txt in your
	// nearest friendly kernel tree for details.
	
	{
		int ret = fscanf(fp, "%*d (%*[^)]) %*c %*d %*d %*d %*d %*d %*d %*d "
						 "%*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %*d %Lu",
						 &rss);
		if (ret != 1)
		{
			LL_WARNS() << "couldn't parse contents of " << statPath << LL_ENDL;
			rss = 0;
		}
	}
	
	fclose(fp);

bail:
	return rss;
}

U32 LLMemory::getWorkingSetSize()
{
	return 0 ;
}

#else

U64 LLMemory::getCurrentRSS()
{
	return 0;
}

U32 LLMemory::getWorkingSetSize()
{
	return 0;
}

#endif

#if defined(LL_WINDOWS) && defined(LL_DEBUG_BUFFER_OVERRUN)

#include <map>

struct mem_info {
	std::map<void*, void*> memory_info;
	LLMutex mutex;

	static mem_info& get() {
		static mem_info instance;
		return instance;
	}

private:
	mem_info(){}
};

void* ll_aligned_malloc_fallback( size_t size, int align )
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	
	unsigned int for_alloc = (size/sysinfo.dwPageSize + !!(size%sysinfo.dwPageSize)) * sysinfo.dwPageSize;
	
	void *p = VirtualAlloc(NULL, for_alloc+sysinfo.dwPageSize, MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
	if(NULL == p) {
		// call debugger
		__asm int 3;
	}
	DWORD old;
	BOOL Res = VirtualProtect((void*)((char*)p + for_alloc), sysinfo.dwPageSize, PAGE_NOACCESS, &old);
	if(FALSE == Res) {
		// call debugger
		__asm int 3;
	}

	void* ret = (void*)((char*)p + for_alloc-size);
	
	{
		LLMutexLock lock(&mem_info::get().mutex);
		mem_info::get().memory_info.insert(std::pair<void*, void*>(ret, p));
	}
	

	return ret;
}

void ll_aligned_free_fallback( void* ptr )
{
	LLMutexLock lock(&mem_info::get().mutex);
	VirtualFree(mem_info::get().memory_info.find(ptr)->second, 0, MEM_RELEASE);
	mem_info::get().memory_info.erase(ptr);
}

#endif
