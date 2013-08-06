
#include		"kernel.h"
#include		"i8259A.h"

typedef struct _irq_info {
	/* 
		0:Edge Trigger
		1:Level Trigger
	 */
	DWORD		interrupt_type;
} irq_info;

/* Edge�g���K�[�ɂ����Ⴄ������ */
int _sys_irq_set_edge_trigger(DWORD irq_num);

/* level�g���K�[�ɂ����Ⴄ������ */
int _sys_irq_set_level_trigger(DWORD irq_num);

/* ������g����irq_num�Ŏ������IRQ�̃g���K�[���[�h���`�F�b�N���� */
irq_info _sys_irq_info(DWORD irq_num);

