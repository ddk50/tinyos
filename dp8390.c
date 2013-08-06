
#include        "dp8390.h"
#include        "system.h"
#include        "io.h"
#include        "vga.h"
#include        "pci-main.h"
#include        "gdebug.h"
#include        "system.h"
#include        "memory.h"
#include        "packet.h"
#include        "ethernet.h"
#include        "idt.h"
#include        "task.h"


#define         NS_DATA_PORT            0x10
#define         NS_RESET                0x1f

#define         COMMAND_REG             0x00
#define         INTR_STATUS_REG         0x07
#define         INTR_MASK_REG           0x0f
#define         DATA_CFG_REG            0x0e
#define         TRSMT_CFG_REG           0x0d
#define         TRSMT_STATUS_REG        0x04
#define         RECV_CFG_REG            0x0c
#define         RECV_STATUS_REG         0x0c

#define         EN1_PHYS                0x01
#define         EN0_COUNTER0            0x0d
#define         EN0_IMR                 0x0f
#define         EN0_ISR                 0x07
#define         EN0_RSA0                0x08    /* Remote Start Address Lo */
#define         EN0_RSA1                0x09    /* Remote Start Address Hi */

#define         IMR_VL_PRXE             0x01    /* Packet Received Interrupt Enable */
#define         IMR_VL_PTXE             0x02    /* Packet Transmit Interrupt Enable */
#define         IMR_VL_RXEE             0x04    /* Receive Error Interrupt Enable */
#define         IMR_VL_TXEE             0x08    /* Transmit Error Interrupt Enable */
#define         IMR_VL_OVWE             0x10    /* Overwrite Error Interrupt Enable */
#define         IMR_VL_CNTE             0x20    /* Counter Overflow Interrupt Enable */
#define         IMR_VL_RDCE             0x40    /* Remote DMA Complete Interrupt Enable */

#define         ISR_VL_PRX              0x01    /* Packet Received */
#define         ISR_VL_PTX              0x02    /* Packet Transmitted */
#define         ISR_VL_RXE              0x04    /* Receive Error */
#define         ISR_VL_TXE              0x08    /* Transmission Error */
#define         ISR_VL_OVW              0x10    /* Overwrite */
#define         ISR_VL_CNT              0x20    /* Counter Overflow */
#define         ISR_VL_RDC              0x40    /* Remote Data Complete */
#define         ISR_VL_RST              0x80    /* Reset status */

#define         EN0_PSTART_REG          0x01
#define         EN0_PSTOP_REG           0x02
#define         EN0_BNRY_REG            0x03

#define         REG_PAGE0               0x00
#define         REG_PAGE1               0x40
#define         REG_PAGE2               0x80
#define         REG_PAGE3               0xC0

#define         CMD_REMOTE_READ         0x08
#define         CMD_REMOTE_WRITE        0x10
#define         CMD_SEND_PACKET         0x20
#define         CMD_TXP_BIT             0x04
#define         CMD_NO_DMA              0x20
#define         CMD_STOP                0x01
#define         CMD_START               0x02

#define         P1_PAR0                 0x01
#define         P1_PAR1                 0x02
#define         P1_PAR2                 0x03
#define         P1_PAR3                 0x04
#define         P1_PAR4                 0x05
#define         P1_PAR5                 0x06
#define         P1_CURR                 0x07

#define         P1_MAR0                 0x08
#define         P1_MAR1                 0x09
#define         P1_MAR2                 0x0A
#define         P1_MAR3                 0x0B
#define         P1_MAR4                 0x0C
#define         P1_MAR5                 0x0D
#define         P1_MAR6                 0x0E
#define         P1_MAR7                 0x0F

#define         P0_RCR                  0x0c
#define         P0_TCR                  0x0d
#define         P0_ISR                  0x07
#define         P0_RBCR0                0x0a    /* Remote Byte Count Lo */
#define         P0_RBCR1                0x0b    /* Remote Byte Count Hi */
#define         P0_TBCR0                0x05
#define         P0_TBCR1                0x06

#define         P0_RSAR0                0x08
#define         P0_RSAR1                0x09
#define         P0_TPSR                 0x04
#define         P0_RSR                  0x0c

#define         P1_CURR                 0x07

#define         RCR_VL_AB               0x04    /* accept Broadcast */

#define         ISR_RESET_BIT           0x80

#define         DCR_FT1                 0x40
#define         DCR_WTS                 0x01
#define         DCR_LB_S                0x08

#define         VENDOR_ID               0x10ec
#define         DEVICE_ID               0x8029

#define         NE_DATAPORT             0x10

#define         NE_PAGE_SIZE            256

#define         TIME_OUT                100000


typedef struct _dp8390_pkt_hdr {
    BYTE        status;
    BYTE        next_pointer;
    WORD        packet_length;
} __attribute__ ((packed)) dp8390_pkt_hdr;

typedef struct _cmd_subset {
    WORD        io_addr;
    BYTE        value;
} cmd_subset;

static void destroy_nic();
static int read_nic_mem(void *dest, unsigned short src, int n);
static int find_myself();
static void reset_event(int *flag);
static void read_physical_address(char *buffer);
static int prob_ne2000();
static void ne2000_setup();
static void receive_packet();
static void irq_event();
static void wait_dma_complete();
static void wait_transmit_complete();
static int trans_packet(int size, const void *buf);
static void test();

/* �f�o�C�X�o�^�p */
device_operator eth0_operator = {
    0,
    "dp8390 Ethernet Controller",
    1,          /* �u���b�N�f�o�C�X */
    &init_dp8390,
    NULL,
    NULL,
    NULL,
    NULL,
    &trans_packet
};

static pci_dev  *my_dev     = NULL;

/*
    �Ӗ��킩��Ȃ��񂾂��ǁAne2k�n�̃R���g���[���ɂ�
    MAC�A�h���X��1byte�����ꂼ�����L�^����Ă�݂���
    6 * 2 = 12
*/
static BYTE     physical_address[12] = {0};

static WORD     e8390_base  = 0x0;

volatile int        _flag_packet_trans = 0;
volatile int        _flag_dma_comp = 0;

static WORD     rx_ring_start;
static WORD     rx_ring_end;
static WORD     rx_page_start = 72;
/* static WORD      rx_page_start = 64; */
static WORD     rx_page_end = 128;

static BYTE     next_pkt;

void init_dp8390(void)
{    
    BYTE irq_line;
    
    if (!find_myself()) {
        _sys_printf(" NE2000 compatible device not found!\n");
        return;
    }
    
    _sys_printf(" NIC: find device, pci_num=%d\n", my_dev->pci_num);
    
    irq_line = my_dev->irq_num;
    eth0_operator.irq_num = irq_line;
    
    if (!pci_register_irq(irq_line, &irq_event)) {
        _sys_printf("failed regist irq line\n");
        return;
    }
    
    e8390_base = my_dev->io_addr[0];
    _sys_printf(" NIC: i/o_addr: %x, irq_num=%d\n", e8390_base, irq_line);
    
    /* eoi���蓮�ɕύX�i�f�o�h���������悤�ɂ���j */
    //_sys_change_irq_proc_type(irq_line, IRQ_PROC_TYPE_2);
    
    /* ne2000��{�i�I�ɃZ�b�g�A�b�v */
    ne2000_setup();
    
    return;
    
}

static int read_nic_mem(void *dest, unsigned short src, int n)
{    
    int  i    = 0;
    char *tmp = (char*)dest;
    
    /*
     * DMA���߂ď������A�]�����ł����Ă���~
     * page0�I��
    */
    
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START);
    delay(2);
    
    outb(e8390_base + P0_RBCR0, (n & 0xff));
    delay(2);
    
    outb(e8390_base + P0_RBCR1, ((n >> 8) & 0xff));
    delay(2);
    
    /* src�A�h���X�ݒ� */
    outb(e8390_base + EN0_RSA0, (src & 0xff));
    delay(2);
    
    outb(e8390_base + EN0_RSA1, ((src >> 8) & 0xff));
    delay(2);
    
    outb(e8390_base + COMMAND_REG, CMD_REMOTE_READ | CMD_START);
    delay(2);
    
    for ( ; i < n ; i++) {
        *(tmp + i) = inb(e8390_base + NS_DATA_PORT);
    }
    return n;
}

/* mac address��ǂݍ���ł݂� */
static void read_physical_address(char *buffer)
{    
    read_nic_mem(buffer, 0, sizeof(BYTE) * 12);    
}

/* ne2000�h���C�o�����K�Ȃ��̂��ǂ������`�F�b�N���� */
static int prob_ne2000(void)
{    
    BYTE ret;
    
    if ((inb(e8390_base)) == 0xff) {
        return 0;
    }
    
    /* page0�I�� */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_STOP);
    
    delay(100);
    
    ret = inb(e8390_base + COMMAND_REG);
    ret &= CMD_NO_DMA | CMD_TXP_BIT | CMD_START | CMD_STOP;
    
    if (ret != (CMD_NO_DMA | CMD_STOP)) {
        return 0;
    }
    
    ret = inb(e8390_base + INTR_STATUS_REG);
    ret &= ISR_RESET_BIT;
    
    if (ret != ISR_RESET_BIT) {
        return 0;
    }
    
    /* ���������� */
    return 1;
}

static void irq_event(void)
{    
    BYTE irq_value;
    
    /* page0��I�� */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START);
    
    /* prx���������ׂ� */
    while ((irq_value = inb(e8390_base + P0_ISR)) != 0) {
      
        /* ���荞�݂͎󂯎�����ƒʒm���� */
        outb(e8390_base + P0_ISR, irq_value);
        
        if (irq_value & ISR_VL_PRX) {
            /* �V�����p�P�b�g���͂�����I */
            receive_packet();
        }
        
        if (irq_value & ISR_VL_PTX) {
            /* �p�P�b�g�]�������I */
            _flag_packet_trans = 1;
        }
        
        if (irq_value & ISR_VL_RDC) {
            /* remote DMA complete */
            _flag_dma_comp = 1;
        }
        
        /* ���߂�page0��I�� */
        outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START);
        
    }
    
    return;
}

static void receive_packet(void)
{    
    dp8390_pkt_hdr header;
    WORD           packet_ptr;
    WORD           len, tmp_len;
    BYTE           *dest;
    BYTE           boundary_reg;
    
    tmp_len = 0;
    
    /* Page1��I�� */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START | REG_PAGE1);
    
    /* ���ǃp�P�b�g�܂ł��ׂēǂ� */
    while (next_pkt != inb(e8390_base + P1_CURR)) {
        
        /* ���A�h���X�ɕϊ� */
        packet_ptr = next_pkt * NE_PAGE_SIZE;
        
        /* �ŏ��Ƀw�b�_�[��ǂ݂܂��傤 */
        read_nic_mem(&header, packet_ptr, sizeof(dp8390_pkt_hdr));
        
        len = header.packet_length - sizeof(dp8390_pkt_hdr);
        
        dest = (BYTE*)_sys_kmalloc(len);
        
        if (dest == NULL) {
            _sys_printf("NIC: alloc error!\n");
            return;
        }
        
        /* �w�b�_�[��� */
        _sys_printf("packet receive %d [bytes]\n", len);
        
        /* �����O�o�b�t�@�Ȃ̂�... */
        if ((packet_ptr + len) > rx_ring_end) {
            tmp_len = rx_ring_end - packet_ptr;
            
            read_nic_mem(dest, packet_ptr, tmp_len);
            
            /* dest += tmp_len; */
            packet_ptr = rx_ring_start;
            len -= tmp_len;
            
        }
        
        /* �c���ǂ� */
        read_nic_mem(dest + tmp_len, packet_ptr, len);
        
        in_recv_packet_que(dest + sizeof(dp8390_pkt_hdr), 
                           header.packet_length - sizeof(dp8390_pkt_hdr));
        
        next_pkt = header.next_pointer;
        
        //_sys_printf("next_pkt: %d\n", header.next_pointer);
        
        /* Page0��I�� */
        outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START | REG_PAGE0);
        
        boundary_reg = next_pkt - 1;
        
        /* �����O�o�b�t�@������A�I�[�ɒB�������Ƃ��l���ĂȂ���... */
        if (boundary_reg < rx_page_start) {
            boundary_reg = rx_page_end - 1;
        }
        
        /* boundary register updata */
        outb(e8390_base + EN0_BNRY_REG, boundary_reg);
        
        /* Page1���đI�� */
        outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START | REG_PAGE1);
        
    }
    in_task_que(&start_packet_store);
}

static void get_packet(void *dest, WORD src, int n)
{    
    WORD  size;
    
    /* �����O�o�b�t�@�Ȃ̂�... */
    if((src + n) > rx_ring_end){
        
        size = rx_ring_end - src;
        
        read_nic_mem(dest, src, size);
        
        dest += size;
        src = rx_ring_start;
        n -= size;
    }
    
    /* �c���ǂ� */
    read_nic_mem(dest, src, n);
}

static void ne2000_setup(void)
{    
    int                 i;
    BYTE                bound_ptr_reg;
    ethernet_addr       mac_addr;
    BYTE                c;
    
    if(!prob_ne2000()){
        _sys_printf(" NIC: failed initialize\n");
        return;
    }
    
    _flag_packet_trans = _flag_dma_comp = 0;
    
    bound_ptr_reg = rx_page_start;
    next_pkt = rx_page_start + 1;
    
    rx_ring_start = rx_page_start * NE_PAGE_SIZE;
    rx_ring_end = rx_page_end * NE_PAGE_SIZE;
    
    /* nic�̃��Z�b�g */
    c = inb(e8390_base + NS_RESET);
    outb(e8390_base + NS_RESET, c);
    
    delay(600);
    
    /* CMD�S�X�g�b�v�ADMA������ */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_STOP);
    
    /*
        1.FIFO Threshold Select 1
        2.Loop Back mode select
        3.Byte order 80x86 mode
         (bytes�P�ʂł̓]���͂��܂�������悭�Ȃ������Ȃ��Aword�P�ʂł��ݒ�ł����)
    */
    outb(e8390_base + DATA_CFG_REG, DCR_FT1 | DCR_LB_S);
    
    read_physical_address(physical_address);
    
    for(i=0;i<6;++i){
        mac_addr.byte[i] = physical_address[i * 2];
    }
    
    /* ethernet�ɓo�^ */
    regist_eth_addr(mac_addr);
    
    _sys_printf(" NIC MAC-address: %x:%x:%x:%x:%x:%x\n", 
        physical_address[0],physical_address[2],physical_address[4],
        physical_address[6],physical_address[8],physical_address[10]);
    
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_STOP);
    
    /* Remote Count Register (Low, High)�������� */
    outb(e8390_base + P0_RBCR0, 0x0);
    outb(e8390_base + P0_RBCR1, 0x0);
    
    /* moniter mode */
    outb(e8390_base + P0_RCR, 0x20);
    
    /* loop back mode */
    outb(e8390_base + P0_TCR, 2);
    
    /* ring buffer�̏��� */
    outb(e8390_base + EN0_PSTART_REG, rx_page_start);
    outb(e8390_base + EN0_PSTOP_REG, rx_page_end);
    outb(e8390_base + EN0_BNRY_REG, bound_ptr_reg);
    
    /* ���荞�݂�L���ɂ��� */
    /* 
        Counter Overflow interrupt�͖���
    */
    outb(e8390_base + REG_PAGE0 + EN0_IMR, 
        IMR_VL_PRXE | IMR_VL_PTXE | IMR_VL_RXEE | IMR_VL_TXEE | IMR_VL_OVWE | IMR_VL_RDCE);
    
    outb(e8390_base + P0_ISR, 0xff);
    
    //outb(e8390_base + REG_PAGE0 + EN0_IMR, 0x7F);
    
    /* page1 �I�� */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_STOP | REG_PAGE1);
    
    /* �����A�h���X��PAR�ɏ������� */
    outb(e8390_base + P1_PAR0, physical_address[0]);
    outb(e8390_base + P1_PAR1, physical_address[2]);
    outb(e8390_base + P1_PAR2, physical_address[4]);
    outb(e8390_base + P1_PAR3, physical_address[6]);
    outb(e8390_base + P1_PAR4, physical_address[8]);
    outb(e8390_base + P1_PAR5, physical_address[10]);
    
    /*
        Current Page Register
        �p�P�b�g���i�[����̂Ɏg�p�����ŏ���buffer������
    */
    outb(e8390_base + P1_CURR, next_pkt);
    
    /* �}���`�L���X�g�A�h���X��MAR�ɏ������� */
    outb(e8390_base + P1_MAR0, 0x0);
    outb(e8390_base + P1_MAR1, 0x0);
    outb(e8390_base + P1_MAR2, 0x0);
    outb(e8390_base + P1_MAR3, 0x0);
    outb(e8390_base + P1_MAR4, 0x0);
    outb(e8390_base + P1_MAR5, 0x0);
    outb(e8390_base + P1_MAR6, 0x0);
    outb(e8390_base + P1_MAR7, 0x0);
    
    /* page0 �I�� */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_STOP | REG_PAGE0);
    
    /* broad cast���󂯕t����悤�ɂ��� */
    outb(e8390_base + P0_RCR, RCR_VL_AB);
    
    /* Normal Operation */
    outb(e8390_base + P0_TCR, 0x0);
    
    /* ���荞�ݏ�Ԃ�����ISR�������� */
    outb(e8390_base + P0_ISR, 0xff);
    
    /* NIC���t�g�I�t�I */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START);
    
/*
    
    delay(10);
    
    for(i=0;i<10;i++){
        test();
        delay(1000);
    }
*/
    
}

static void test(void)
{    
    BYTE    *test_buf = (BYTE*)_sys_kmalloc(256);
    
    _sys_memset32((void*)test_buf, 'a', 256);
    
    test_buf[0] = 0x00;
    test_buf[1] = 0x07;
    test_buf[2] = 0x40;
    test_buf[3] = 0xcc;
    test_buf[4] = 0xe4;
    test_buf[5] = 0x78;
    
    test_buf[6] = physical_address[0];
    test_buf[7] = physical_address[2];
    test_buf[8] = physical_address[4];
    test_buf[9] = physical_address[6];
    test_buf[10] = physical_address[8];
    test_buf[11] = physical_address[10];
    
    test_buf[12] = 0x01;
    test_buf[13] = 0x00;
    
    trans_packet(256, test_buf);
    
    _sys_printf("transfer\n send size %d\n", 256);
}

/* pci�f�o�C�X�̒��ɂ���܂����H */
static int find_myself(void)
{    
    pci_dev *ptr;
    
    ptr = find_pci_dev_from_id(VENDOR_ID, DEVICE_ID);
    
    if (ptr != NULL) {
        my_dev = ptr;
        return 1;
    }
    
    return 0;
}

static int trans_packet(int size, const void *buf)
{    
    DWORD   i;
    char    *ptr        = (char*)buf;
    DWORD   buf_size    = (DWORD)size;
    WORD    dest_ptr    = rx_page_end;
    
    /* DMA complete��transmit����������܂ő҂��܂��傤 */
    /*
    reset_event(&_flag_dma_comp);
    reset_event(&_flag_packet_trans);
    */
    
    /* ���M�������ǂ������`�F�b�N���� */
    while ((inb(e8390_base + COMMAND_REG) & 0x04) != 0);
    
    cli();
    
    /* page0 �I�� */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_START | REG_PAGE0);
    
    /* ISR��DMA complete flag�𗧂Ă� */
    outb(e8390_base + P0_ISR, ISR_VL_RDC);
    
    /* �ǂꂾ���̃T�C�Y��]������̂��Ȃ� */
    outb(e8390_base + P0_RBCR0, buf_size);
    outb(e8390_base + P0_RBCR1, buf_size >> 8);
    
    /* �ǂ��ɓ]������̂��Ȃ� */
    outb(e8390_base + P0_RSAR0, (dest_ptr * NE_PAGE_SIZE));
    outb(e8390_base + P0_RSAR1, (dest_ptr * NE_PAGE_SIZE) >> 8);
    
    sti();
    
    /* DMA��NIC�����o�b�t�@�ɏ�������ł��炢�܂��傤 */
    outb(e8390_base + COMMAND_REG, CMD_REMOTE_WRITE | CMD_START);
    
    /* �f�[�^���������� */
    for (i = 0 ; i < buf_size ; i++) {
        outb(e8390_base + NS_DATA_PORT, ptr[i]);
        delay(10);
    }
    
    /* IRQ����ADMA complete���ʒm�����܂ő҂��܂��傤  */
    wait_dma_complete();
    
    /* �ǂ�����]�����܂����H */
    outb(e8390_base + P0_TPSR, dest_ptr);
    
    /* 64byte�����̃p�P�b�g�͓]���ł��Ȃ���ł��� */
    if (buf_size > 64) {
        outb(e8390_base + P0_TBCR0, buf_size);
        outb(e8390_base + P0_TBCR1, buf_size >> 8);
    } else {
        outb(e8390_base + P0_TBCR0, 64);
        outb(e8390_base + P0_TBCR1, 0);
    }
    
    /* �]�����J�n���܂��傤 */
    outb(e8390_base + COMMAND_REG, CMD_NO_DMA | CMD_TXP_BIT | CMD_START);
    
    /* �p�P�b�g���]�����������܂ő҂��܂��傤 */
    wait_transmit_complete();
    
    return buf_size;
    
}

static void destroy_nic(void)
{    
}

static void wait_dma_complete(void)
{    
    int i = 0;
    
    while (!_flag_dma_comp) {
        ++i;
        if (i >= TIME_OUT) {
            _sys_printf("dma time out\n");
            return;
        }
        delay(2);
    }
    _flag_dma_comp = 0;
}


static void wait_transmit_complete(void)
{    
    int i = 0;
    
    while (!_flag_packet_trans) {
        ++i;        
        if (i >= TIME_OUT) {
            _sys_printf("trasmit wait time out\n");
            return;
        }        
        delay(2);        
    }    
    _flag_packet_trans = 0;    
}

static void reset_event(int *flag)
{    
    *flag = 0;    
}
