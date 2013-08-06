
#ifndef memoryH
#define	memoryH

#include		"kernel.h"

/*
	�r�b�O�G���f�B�A���Ȃ烊�g���G���f�B�A��
	���g���G���f�B�A���Ȃ�r�b�O�G���f�B�A��
	�ɂ��ꂼ��ϊ�����
	WORD�P�ʂȂ̂Œ��ӁI
*/
WORD *change_endian(WORD *data, int count);

void _sys_memset16(void *dest, const char src, WORD count);

void _sys_memset32(void *dest, const char src, unsigned int count);

void *_sys_memcpy(void *dest, const void *src, int n);

void *_sys_physic_mem_write(void *dest, const void *src, int n);

void *_sys_physic_mem_read(void *dest, const void *src, int n);

#endif

