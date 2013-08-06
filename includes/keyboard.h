
#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

#include "resource.h"

//�L�[�{�[�h�h���C�o�p�̃����O�o�b�t�@�T�C�Y
#define		RING_BUFFER_SIZE	15
#define		KEYBOARD_IN_PORT	0x60

//#define		USE_LINUX_KEYCODE

/* �G�~�����[�V�����p�̃n���h�� */
extern emulation_handler	keyboard_emu;
extern device_operator		keyboard_operator;
extern device_node			keyboard_device;

void _sys_keyboard_event(void);
void _sys_init_keyboard_handler(void (*handler)(int));

#endif
