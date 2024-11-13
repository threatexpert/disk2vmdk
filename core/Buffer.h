#pragma once
#include "Thread.h"
#include <list>

class CBuffers
{
public:
	class Buffer {
	public:
		void* p;
		DWORD buffer_size;
		DWORD data_length;
		uint64_t zero_length;
		Buffer() {
			p = NULL;
			buffer_size = 0;
			data_length = 0;
			zero_length = 0;
		}
		~Buffer() {
			free(p);
		}
		bool init(DWORD size) {
			free(p);
			p = malloc(size);
			buffer_size = size;
			return p != NULL;
		}
	};
protected:
	CMyEvent m_e;
	CMyCriticalSection m_lc;
	std::list<Buffer*> m_items;
	BOOL m_bAbort;
public:
	CBuffers();
	virtual ~CBuffers();

	bool init_pool(DWORD size, DWORD count);
	void clear();

	Buffer* Pop(DWORD wait_ms);
	void Push(Buffer* p);
	void Abort(BOOL force);
	void ResetAbort();



};
