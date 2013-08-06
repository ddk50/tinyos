
#ifndef pageH
#define pageH

#include		"kernel.h"

#define		PAGE_MASK						0xfffff000
#define		PAGE_SHIFT(x)					((x) & PAGE_MASK)

/*	�K���Ȉʒu��PDE��PTE��u���B
	���ۂɂ͉���12�r�b�g��0�Ƀ}�X�N���ꂽ�A�h���X���g���� */
/*
	PDE��PTE�ł��ꂼ��0x1000�g�p����̂�0x2000�g�p����
*/
#define		PAGE_ENTRY_POINT				0x20000
#define		PAGE_4K_ENTRY_COUNT				1024

//#define		pg0_HEAD_LINER_ADDR			0xC0000000
//#define		pg0_1st_HEADLINER_ADDR		0xC0010000
//#define		pg0_2nd_HEADLINER_ADDR		0xC00A0000
//#define		pg0_3rd_HEADLINER_ADDR		0xC0100000

#define		pg0_1st_HEADLINER_ADDR			*P_VIRTUAL_MEMSIZE
#define		pg0_2nd_HEADLINER_ADDR			((*P_VIRTUAL_MEMSIZE) + 0xA0000)
#define		pg0_3rd_HEADLINER_ADDR			((*P_VIRTUAL_MEMSIZE) + 100000)


#define		pg0_PAGEBLOCK_SIZE				0x1000		/* pg0��4kbyte�y�[�W */
#define		pg1_PAGEBLOCK_SIZE				0x400000	/* pg1��4Mbyte�y�[�W */

#define		PAGE_PDE_ENTRY_NUM(x)			((x) >> 22)
#define		PAGE_PTE_ENTRY_NUM(x)			(((x) >> 12) & 0x3ff)


#define		PG_F_P							0x01		/* 	Present bit	*/
#define		PG_F_RW							0x02		/* 	R/W bit		*/
#define		PG_F_US							0x04
#define		PG_F_PWT						0x08
#define		PG_F_PCD						0x10


typedef struct {
	DWORD		liner_addr;
	DWORD		physic_addr;
	DWORD		PTE_physic_addr;
	DWORD		*PTEentry_data;
	DWORD		*PDEentry_data;
	BYTE		flag;
} PagingInfo;


extern DWORD	*pg0_PDE_entry;
extern DWORD	*pg0_PTE_entry;

//gfw�ŏ��̃y�[�W���O��ݒ�E�L���ɂ���
//void _sys_set_first_paging(DWORD PDE_entry_addr);

//cr0���W�X�^�̃r�b�g�𑀍삵�āA�y�[�W���O��ON�ɂ���
void _sys_enable_paging(void);

void _sys_link_physic2liner(PagingInfo*);

//PDE�G���g���̕����A�h���X��cr3���W�X�^�ɃZ�b�g����
void _sys_set_cr3(DWORD);

void _sys_set_pg1_paging(void);

//�G�~�����[�V�����̂��߂̃y�[�W���O�pPDE�A�h���X��Ԃ�
DWORD _sys_pg1_cr3_value(void);

void _sys_use_both_page(void);

#endif

