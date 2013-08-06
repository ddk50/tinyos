
#include		"io.h"
#include		"idt.h"
#include		"system.h"
#include		"memory.h"
#include		"FDCdrv.h"
#include		"gdebug.h"


#define		PRIMARY_DOR_ADDR			0x3f2
#define		SECONDARY_DOR_ADDR			0x372
#define		PRIMARY_MSR_ADDR			0x3f4
#define		SECONDARY_MSR_ADDR			0x374
#define		PRIMARY_DR_ADDR				0x3f5
#define		SECONDARY_DR_ADDR			0x375
#define		PRIMARY_DIR_ADDR			0x3f7
#define		SECONDARY_DIR_ADDR			0x377

#define		FDC_ENABLE_DMA				0x8
#define		FDC_ENABLE_RESET			0x4
#define		FDC_MOTER_START				(0x00 | FDC_ENABLE_RESET | FDC_ENABLE_DMA)
#define		FDC_MOTER_STOP				(0x00 | FDC_ENABLE_RESET | FDC_ENABLE_DMA)

#define		CMD_FIX_DRIVE_DATA			0x03
#define		CMD_SEEK					0x0F
#define		CMD_READ					0x06
#define		CMD_WRITE					0x05
#define		CMD_CHECK_INTRRUPT			0x08
#define		CMD_CALIBRATE				0x07
#define		CMD_VERSION					0x10

#define		MASTER_DMA_CMD_REG			0xD0
#define		MASTER_DMA_MODE_REG			0xD6
#define		MASTER_DMA_MASK_REG			0xD4
#define		SLAVE_DMA_CMD_REG			0x08
#define		SLAVE_DMA_MODE_REG			0x0B
#define		SLAVE_DMA_MASK_REG			0x0A
#define		CHANNEL2_PAGE_REG			0x81
#define		CHANNEL2_ADDR_REG			0x04
#define		CHANNEL2_COUNT_REG			0x05
#define		MASTER_DMA_PTR_REG			0x0C
#define		SLAVE_DMA_PTR_REG			0xD8

#define		FDC_NEC765					0x80      // Standard uPD765A controller
#define 	FDC_82077					0x90      // Extended uPD765B controller

#define		IRQ_WAIT_TIMEOUT			50000


volatile int	_intrrupt	= 0x0;
/* ���U���g�t���[�Y�i�[�e�� */
static BYTE		_result[7] 	= {0};
/* FDC�̃o�[�W���� */
static BYTE		_version;

static int		_now_seek_pos = 0;


static WORD		dir_reg_addr[] 	= {PRIMARY_DIR_ADDR, SECONDARY_DIR_ADDR};
static WORD		dor_reg_addr[] 	= {PRIMARY_DOR_ADDR, SECONDARY_DOR_ADDR};
static WORD		data_reg_addr[] = {PRIMARY_DR_ADDR, SECONDARY_DR_ADDR};
static WORD		msr_reg_addr[]	= {PRIMARY_MSR_ADDR, SECONDARY_MSR_ADDR};

static void reset_fdd(int entry, int fdd_num);

static void moter_on(int entry, int fdd_num);
static void moter_off(int entry, int fdd_num);

static void send_command(int entry, const BYTE *command, int length);
static void waitStatus(int entry, BYTE mask, BYTE expected);
static BYTE get_result(int entry);

static int cmd_seek(int entry, int fdd_num, BYTE track);
static int cmd_checkIntrrupt(int entry);
static int cmd_calibrate(int entry, int fdd_num);
static int cmd_read(int entry, int fdd_num, BYTE track, BYTE head, BYTE sector);
static BYTE get_fdc_version(int entry);

static void setup_DMA_read(WORD size);
static void setup_DMA_write(WORD size);

static void stopDMA();
static void startDMA();

static void lba2chs(int LBA, BYTE *track, BYTE *head, BYTE *sector);

void intrrupt_handler(void);
static int wait_IRQ();

static int fdc_read_sector(int entry, int fdd_num, BYTE track, BYTE head, BYTE sector, char *buffer);
static int fdc_write_sector(int entry, int fdd_num, BYTE track, BYTE head, BYTE sector, const char *buffer);

/* �f�o�C�X�I�y���[�^�[ */
static void fdc_all_reset();

/*
	Seek CHS
	int ioctl(0x04, DWORD track, DWORD head, DWORD sector);
	
	check media change
	int ioctl(0x03);

*/
static int fdc_ioctl(int request, va_list arg);
static int fdc_seek(int lba, WORD flag);
static int fdc_read(int read_size, void *buffer);
static int fdc_write(int write_size, const void *buffer);


#define		DMA_BUFFER_ADDR				0x100000

/* HMA�̈��DMA�̃o�b�t�@�Ƃ��Ďg�킹�Ă��炤 */
//����[���Ƃ���͂��������l����Ƃ����񂼁B
static char			*_dma_buffer		= (char*)DMA_BUFFER_ADDR;

/* �G�~�����[�V�����p�̃n���h�� */
emulation_handler	fdc_emulation[] = {
	{PRIMARY_DOR_ADDR, 		NULL, NULL, NULL, NULL, NULL, NULL},
	{SECONDARY_DOR_ADDR, 	NULL, NULL, NULL, NULL, NULL, NULL},
	{PRIMARY_MSR_ADDR, 		NULL, NULL, NULL, NULL, NULL, NULL},
	{SECONDARY_MSR_ADDR, 	NULL, NULL, NULL, NULL, NULL, NULL},
	{PRIMARY_DR_ADDR, 		NULL, NULL, NULL, NULL, NULL, NULL},
	{SECONDARY_DR_ADDR, 	NULL, NULL, NULL, NULL, NULL, NULL},
	{PRIMARY_DIR_ADDR, 		NULL, NULL, NULL, NULL, NULL, NULL},
	{SECONDARY_DIR_ADDR,	 NULL, NULL, NULL, NULL, NULL, NULL}
};

/* �f�o�C�X�o�^�p */
device_operator		fdc_operator = {
	6,
	"floppy disk drive",
	1,						/* �u���b�N�f�o�C�X */
	&fdc_all_reset,
	NULL,
	&fdc_ioctl,
	&fdc_seek,
	&fdc_read,
	&fdc_write
};

/* �f�o�C�X�m�[�h */
device_node			fdc_device = {
	fdc_emulation,
	NULL,
	NULL
};


static void fdc_all_reset(){
	
	/* 
		DMAC���Z�b�g
		DMA ch4-7 �}�X�^�N���A 
	*/
	outb(0xDA, 0x00);
	delay(1);
	
	/*
		DMAC���Z�b�g
		DMA ch0-3 �}�X�^�N���A
	*/
	outb(0x0D, 0x00);
	delay(1);
	
	
	outb(MASTER_DMA_CMD_REG, 0x00);
	delay(1);
	
	outb(SLAVE_DMA_CMD_REG, 0x00);
	delay(1);
	
	outb(MASTER_DMA_MODE_REG, 0xC0);
	delay(1);
	
	outb(SLAVE_DMA_MODE_REG, 0x46);
	delay(1);
	
	/* Master DMAC�����n�� */
	outb(MASTER_DMA_MASK_REG, 0x00);
	delay(1);
	
	/* ���荞�݃t���O������ */
	_intrrupt = 0;
	
	/* IRQ�X���b�g6�ɓo�^ */
	_sys_set_irq_handle(6, &intrrupt_handler);
	
	/* fdc��version��˂��~�߂� */
	_version = get_fdc_version(0);
	
	switch(_version){
		case FDC_NEC765:
			_sys_printf(" fdc: NEC uPD765 compatible controller\n");
			break;
		case FDC_82077:
			_sys_printf(" fdc: Intel 82072/7 compatible controller\n");
			break;
		case 0x81:
			_sys_printf(" fdc: Type 0x81\n");
			break;
		default:
			_sys_printf(" fdc: unknown fdc type %x\n", _version);
	}
	
	/* primary, drive A������*/
	reset_fdd(0, 0);
	
	delay(4);
	
	moter_off(0, 0);
	
	delay(4);
	
}

static int fdc_ioctl(int request, va_list arg){
	
	DWORD		sector, track, head;
	
	switch(request){
		
		case 0x03:
			/* check media change */
			break;
		case 0x04:
			/* Read CHS */
			track 	= va_arg(arg, DWORD);
			head	= va_arg(arg, DWORD);
			sector	= va_arg(arg, DWORD);
			
			/* seek! */
			_now_seek_pos = (head + track * 2) * 18 + (sector - 1);
			
			break;
		default:
			return 0;
	}
	
	return 1;
	
}

static int fdc_seek(int lba, WORD flag){
	
	flag = 0x0;
	
	_now_seek_pos = lba;
	
	return 1;
	
}

static int fdc_read(int read_size, void *buffer){
	
	BYTE	track, head, sector;
	int		cnt = read_size / 512;
	int		i;
	
	moter_on(0, 0);
	
	for(i=0;i<cnt;i++){
		lba2chs(_now_seek_pos+i, &track, &head, &sector);
		
		if(fdc_read_sector(0, 0, track, head, sector, (char*)buffer)){
			return 1;
		}
	}
	
	return 1;
	
}

static int fdc_write(int write_size, const void *buffer){
	
	BYTE	track, head, sector;
	int		cnt = write_size / 512;
	int		i;
	
	for(i=0;i<cnt;i++){
		lba2chs(_now_seek_pos+i, &track, &head, &sector);
		
		if(fdc_write_sector(0, 0, track, head, sector, (const char*)buffer)){
			return 1;
		}
	}
	
	return 1;
	
}


static void reset_fdd(int entry, int fdd_num){
	
	BYTE	command[] = {
		CMD_FIX_DRIVE_DATA,
		0xC1,	/* SRT=4ms, HUT=16ms */
		0x10	/* 16ms DMA */
	};
	
	outb(dir_reg_addr[entry], 0x0);
	
	moter_on(entry, fdd_num);
	
	send_command(entry, command, sizeof(command));
	
	//cmd_calibrate(entry, fdd_num);
	
	return;
}


static BYTE get_fdc_version(int entry){
	
	BYTE	command[] = {CMD_VERSION};
	
	send_command(entry, command, sizeof(command));
	
	return get_result(entry);
	
}

/* FD moter on */
static void moter_on(int entry, int fdd_num){
	
	BYTE	command = FDC_MOTER_START;
	
	command |= ((BYTE)fdd_num) & 0x03;
	command |= 0x10 << (BYTE)fdd_num;
	
	outb(dor_reg_addr[entry], command);
	
	delay(4);
	
}

/* FD moter off */
static void moter_off(int entry, int fdd_num){
	
	BYTE	command = FDC_MOTER_STOP;
	
	command |= ((BYTE)fdd_num) & 0x03;
	
	outb(dor_reg_addr[entry], command);
	
	delay(4);
	
}


static void send_command(int entry, const BYTE *command, int length){
	
	register int	i = 0;
	
	for(;i<length;i++){
		/* MSR:1, DIO:0�ɂȂ�܂ő҂� */
		waitStatus(entry, 0xC0, 0x80);
		
		outb(data_reg_addr[entry], command[i]);
	}
	
}

/*
�����MSR�p�^�[���������܂ŁA�҂�
*/
static void waitStatus(int entry, BYTE mask, BYTE expected){
	
	BYTE	status;
	
	do{
		
		status = inb(msr_reg_addr[entry]);
			
	}while((status & mask)!=expected);
	
}


void intrrupt_handler(void){
	
	_intrrupt = 1;
	
}


static int wait_IRQ(){
	
	while(!_intrrupt);
	
	_intrrupt = 0;
	
	return 1;
}


static BYTE get_result(int entry){
	
	waitStatus(entry, 0xd0, 0xd0);
	
	return inb(data_reg_addr[entry]);
	
}


static int cmd_seek(int entry, int fdd_num, BYTE track){
	
	BYTE	command[] = {CMD_SEEK, (BYTE)(fdd_num & 0x03), track};
	
	send_command(entry, command, sizeof(command));
	
	for(;;){
		/* IRQ����̊��荞�݂�҂� */
		wait_IRQ();
	
		/* BUSY�t���O��0�ɂȂ�܂ő҂� */
		waitStatus(entry, 0x10, 0x00);
		
		if(cmd_checkIntrrupt(entry)){
			return 1;
		}
	}
	
	return 0;
	
}


static int cmd_checkIntrrupt(int entry){
	
	BYTE	command[] = {CMD_CHECK_INTRRUPT};
	
	send_command(entry, command, sizeof(command));
	
	_result[0] = get_result(entry);
	_result[1] = get_result(entry);
	
	/* 
	Result Phrase��ST0�̒l�́A7bit�ڂ�6bit�ڂ������Ă�����A
	(�ł��Q�l�}�j���A���ɂ�7bit�������ď����Ă���񂾂�˂�)
	invalid command�ł���Ƃ������ƁA�R�}���h���s���s
	*/
	if((_result[0] & 0xC0)!=0x00){
		return 0;
	}
	
	return 1;
	
}

/*
�h���C�u�w�b�_�[��0�̈ʒu�ɖ߂��ď���������
*/
static int cmd_calibrate(int entry, int fdd_num){
	
	BYTE	command[] = {CMD_CALIBRATE, (BYTE)(fdd_num & 0x03)};
	
	send_command(entry, command, sizeof(command));
	
	for(;;){
		
		wait_IRQ();
		
		/* BUSY�t���O��0�ɂȂ�܂ő҂� */
		waitStatus(entry, 0x10, 0x00);
		
		if(cmd_checkIntrrupt(entry)){
			return 1;
		}
	}
	
	return 0;
	
}


/*

DMA�o�b�t�@�[��read_size�������ǂݍ���

entry: primary or secondary
fdd_num : A:0 B:1 C:2 D:3 drive
track: target track
head: target head
sector: target sector

*/
static int cmd_read(int entry, int fdd_num, BYTE track, BYTE head, BYTE sector){
	
	int		i;
	
	BYTE	command[] = {
		(CMD_READ | 0xe0),	/* M, F, S */
		((head & 0x01) << 2) | ((BYTE)(fdd_num & 0x03)),
		track,
		head,
		sector,
		(512 >> 8),
		0x12, /* �悤�킩��� */
		0x1B, /* GAP3�̒��� */
		0xFF, /* Data Length */
	};
	
	if(!cmd_seek(entry, fdd_num, track)){
		return 0;
	}
	
	/* DMA�n�� */
	setup_DMA_read(512);
	
	send_command(entry, command, sizeof(command));
	
	wait_IRQ();
	
	for(i=0;i<7;i++){
		_result[i] = get_result(entry);
	}
	
	stopDMA();
	
	/* ST0�̃��W�X�^�`�F�b�N */
	if((_result[0] & 0xC0)!=0x00){
		return 0;
	}
	
	return 1;
	
}

static int cmd_write(int entry, int fdd_num, BYTE track, BYTE head, BYTE sector){
	
	int		i;
	
	BYTE	command[] = {
		(CMD_WRITE | 0xC0),	/* M, F */
		((head & 0x01) << 2) | ((BYTE)(fdd_num & 0x03)),
		track,
		head,
		sector,
		(512 >> 8),
		0x12,
		0x1B,
		0x00,
	};
	
	setup_DMA_write(512);
	
	if(!cmd_seek(entry, fdd_num, track)){
		return 0;
	}
	
	send_command(entry, command, sizeof(command));
	
	wait_IRQ();
	
	stopDMA();
	
	for(i=0;i<7;i++){
		_result[i] = get_result(entry);
	}
	
	/* ST0�̃��W�X�^�`�F�b�N */
	if((_result[0] & 0xC0)!=0x00){
		return 0;
	}
	
	return 1;
	
}


/*

-Floppy disk has 2 sides, and there are 2 heads;
one for each side (0..1), the drive heads move above the surface of the disk on each side.

-Each side has 80 cylinders(track) (numbered 0..79).

-Each cylinder has 18 sectors (1..18).

-Each sector has 512 bytes.

-Total size of floppy disk is: 2 x 80 x 18 x 512 = 1,474,560 bytes.

*/

int fdc_read_sector(int entry, int fdd_num, BYTE track, BYTE head, BYTE sector, char *buffer){
	
	int		i;
	
	/* 10�񂮂炢���g���C�����ق����悳���� */
	for(i=0;i<10;i++){
		if(cmd_read(entry, fdd_num, track, head, sector)){
			_sys_memcpy((char*)buffer, (char*)_dma_buffer, 512);
			return 1;
		}
	}
	
	return 0;
}

int fdc_write_sector(int entry, int fdd_num, BYTE track, BYTE head, BYTE sector, const char *buffer){
	
	int		i;
	
	_sys_memcpy((char*)_dma_buffer, (char*)buffer, 512);
	
	/* ��͂�10��قǃ��g���C */
	for(i=0;i<10;i++){
		if(cmd_write(entry, fdd_num, track, head, sector)){
			return 1;
		}
	}
	
	return 0;
}



static void setup_DMA_read(WORD size){
	
	/* DMA��16bit��������ł���̂��� */
	DWORD		ptr = (DWORD)_dma_buffer;
	
	stopDMA();
	
	/*
		MODE�ݒ�
		-Channel-2 select
		-write transfer
		-Autoinitialized
		-Address increment select
		-Single mode
	*/
	outb(SLAVE_DMA_MODE_REG, 0x46);
	
	/* ������ӂŊ��荞�݂̃��b�N��... */
	
	/* �|�C���^���W�X�^�N���A */
	outb(MASTER_DMA_PTR_REG, 0x00);
	
	/* �]����A�h���X�ݒ� */
	outb(CHANNEL2_ADDR_REG, (BYTE)(ptr & 0x00ff));
	outb(CHANNEL2_ADDR_REG, (BYTE)((ptr >> 8) & 0x00ff));
	
	/* �]���T�C�Y�ݒ� */
	size--;		/* DMA�̐ݒ�T�C�Y�� actually_length - 1�Ȃ񂾂Ƃ� */
	outb(CHANNEL2_COUNT_REG, (BYTE)(size & 0x00ff));
	outb(CHANNEL2_COUNT_REG, (BYTE)((size >> 8) & 0x00ff));
	
	/* �]����A�h���X�����y�[�W�ݒ� */
	outb(CHANNEL2_PAGE_REG, (BYTE)((ptr >> 16) & 0x00ff));
	
	startDMA();
	
	return;
	
}


static void setup_DMA_write(WORD size){
	
	DWORD	ptr = (DWORD)_dma_buffer;
	
	stopDMA();
	
	/*
		MODE�ݒ�
		-Channel-2 select
		-read transfer
		-Autoinitialized
		-Address increment select
		-Single mode
	*/
	outb(SLAVE_DMA_MODE_REG, 0x4A);
	
	/* ������ӂŊ��荞�݂̃��b�N��... */
	
	/* �|�C���^���W�X�^�N���A */
	outb(MASTER_DMA_PTR_REG, 0x00);
	
	/* �]����A�h���X�ݒ� */
	outb(CHANNEL2_ADDR_REG, (BYTE)(ptr & 0x00ff));
	outb(CHANNEL2_ADDR_REG, (BYTE)((ptr >> 8) & 0x00ff));
	
	/* �]���T�C�Y�ݒ� */
	size--;
	outb(CHANNEL2_COUNT_REG, (BYTE)(size & 0x00ff));
	outb(CHANNEL2_COUNT_REG, (BYTE)((size >> 8) & 0x00ff));
	
	/* �]����A�h���X�����y�[�W�ݒ� */
	outb(CHANNEL2_PAGE_REG, (BYTE)((ptr >> 16) & 0x00ff));
	
	startDMA();
	
}


static void stopDMA(){
	
	/* 
		SlaveDMAC�iFDC�R���g���[���[���Ȃ����Ă���j
		channel-2��Set mask �ɂ���
	*/
	outb(SLAVE_DMA_MASK_REG, 0x06);
	return;
	
}


static void startDMA(){
	
	/*
		SlaveDMAC�iFDC�R���g���[���[���Ȃ����Ă���j
		channel-2��Clear mask �ɂ���
	*/
	outb(SLAVE_DMA_MASK_REG, 0x02);
	return;
	
}


static void lba2chs(int LBA, BYTE *track, BYTE *head, BYTE *sector){
	
	/*
		1.44MB��2HD�t���b�s�[�̏ꍇ
	*/
	
	*track	= LBA / (2 * 18);
	*head 	= (LBA / 18) % 2;
	*sector = LBA % 18 + 1;
	
	return;
}

