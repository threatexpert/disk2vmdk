#pragma once

#include <stdint.h>

class CVBoxSimple
{
	void* _pDisk;
public:
	CVBoxSimple();
	virtual ~CVBoxSimple();

	static bool InitLib();

	bool CreateImage(const char* format, const char* dst_file, uint64_t cbFile); //UTF8
	bool CreateVMDK(const char *dst_file, uint64_t cbFile);
	bool Write(uint64_t offFile, const void* pvBuf, size_t size);
	bool Close();

};

