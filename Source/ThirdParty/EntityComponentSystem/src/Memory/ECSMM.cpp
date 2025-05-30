
/*
	Author : Tobias Stein
	Date   : 6th September, 2017
	File   : ECSMM.cpp

	Internal used memory manager.

	All Rights Reserved. (c) Copyright 2016 - 2017.
*/

#include "Memory/ECSMM.h"

namespace ECS { namespace Memory { namespace Internal {

	
	MemoryManager::MemoryManager()
	{	
		DEFINE_LOGGER("MemoryManager");
		LOG_INFO("Initialize MemoryManager!","");

		// allocate global memory
		this->m_GlobalMemory = malloc(MemoryManager::MEMORY_CAPACITY);
		if (this->m_GlobalMemory != nullptr)
		{
			LOG_INFO("{} bytes of memory allocated.", MemoryManager::MEMORY_CAPACITY);
		}
		else
		{
			LOG_FATAL("Failed to allocate {} bytes of memory!", MemoryManager::MEMORY_CAPACITY);
			assert(this->m_GlobalMemory != nullptr && "Failed to allocate global memory.");
		}

		// create allocator
		this->m_MemoryAllocator = new StackAllocator(MemoryManager::MEMORY_CAPACITY, this->m_GlobalMemory);
		assert(this->m_MemoryAllocator != nullptr && "Failed to create memory allocator!");

		this->m_PendingMemory.clear();
		this->m_FreedMemory.clear();
	}

	MemoryManager::~MemoryManager()
	{
		LOG_INFO("Releasing MemoryManager!","");

		this->m_MemoryAllocator->clear();

		delete this->m_MemoryAllocator;
		this->m_MemoryAllocator = nullptr;

		free(this->m_GlobalMemory);
		this->m_GlobalMemory = nullptr;
	}

	void MemoryManager::CheckMemoryLeaks()
	{
		assert(!(this->m_FreedMemory.size() > 0 && this->m_PendingMemory.size() == 0) && "Implementation failure!");

		if (this->m_PendingMemory.size() > 0)
		{
			LOG_FATAL("!!!  M E M O R Y   L E A K   D E T E C T E D  !!!","");

			for (auto i : this->m_PendingMemory)
			{
				bool isFreed = false;

				for (auto j : this->m_FreedMemory)
				{
					if (i.second == j)
					{
						isFreed = true;
						break;
					}
				}

				if (isFreed == false)
				{
					LOG_FATAL("\'{}\' memory user didn't release allocated memory {}!", i.first, i.second);
				}
			}
		}
		else
		{
			LOG_INFO("No memory leaks detected.","");
		}
	}

}}} // namespace ECS::Memory::Internal
