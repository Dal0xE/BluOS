#include "memory.h"

int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;
	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i]) {
			return -1;
		}
		else if (b[i] > a[i]) {
			return 1;
		}
	}
	return 0;
}

void* memset(void* bufptr, int value, size_t size) {
	//how many 32b chunks we can write
	uint32_t num_dwords = size / 4;
	//leftover bytes that couldn't be set in a larger chunk
	uint32_t num_bytes = size % 4;
	//write in 32b chunks
	uint32_t* dest32 = (uint32_t*)bufptr;
	uint8_t* dest8 = ((uint8_t*)bufptr) + (num_dwords * 4);

	uint32_t val32 = value | (value << 8) | (value << 16) | (value << 24);
	uint8_t val8 = (uint8_t)value;
	
	//write 4 byte chunks
	for (uint32_t i = 0; i < num_dwords; i++) {
		dest32[i] = val32;
	}
	//write leftover bytes
	for (uint32_t i = 0; i < num_bytes; i++) {
		dest8[i] = val8;
	}
	return bufptr;
}

void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
	//how many 32b chunks we can write
	uint32_t num_dwords = size / 4;
	//leftover bytes that couldn't be written in larger chunk
	uint32_t num_bytes = size % 4;
	//write in 32b chunks
	uint32_t* dest32 = (uint32_t*)dstptr;
	uint32_t* src32 = (uint32_t*)srcptr;
	//find where we need to start writing byte-wise
	uint8_t* dest8 = ((uint8_t*)dstptr) + (num_dwords * 4);
	uint8_t* src8 = ((uint8_t*)srcptr) + (num_dwords * 4);

	//write large chunks
	for (uint32_t i = 0; i < num_dwords; i++) {
		dest32[i] = src32[i];
	}
	//write leftover bytes
	for (uint32_t i = 0; i < num_bytes; i++) {
		dest8[i] = src8[i];
	}
	return dstptr;
}

void* calloc(size_t num, size_t size) {
	void* mem = kmalloc(num * size);
	memset(mem, 0, num * size);
	return mem;
}

static size_t getsize(void* p) {
	size_t* in = p;
	if (in) { 
		--in;
		return *in;
	}
	return -1;
}

void* realloc(void* ptr, size_t size) {
	void* newptr;
	int msize = getsize(ptr);
	if (size <= msize) return ptr;

	newptr = kmalloc(size);
	memcpy(newptr, ptr, msize);

	kfree(ptr);
	return newptr;
}
