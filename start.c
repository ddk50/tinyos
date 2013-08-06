
#include "page.h"
#include "system.h"
#include "gdebug.h"

extern void start_kernel(void);
extern void _sys_set_pg0_paging();
extern void _sys_set_pg1_paging();

void startup_32(void)
{
    _sys_printf(" hello world\n");
    
    /* VM�̃y�[�W���O */
    _sys_set_pg0_paging();
    
    /* Guset��Ԃ̃y�[�W���O */
    _sys_set_pg1_paging();
    
    /* �J�[�l�������� */
    start_kernel();    
}
