
[ORG 0]
[BITS 16]

%include		'printstr.s'
%include		'headers.s'

%define			RST_CNT			10
%define			DRIVE_NUM		0


jmp		DEF_SEG:start


ReadFDOk		db		'Read FD Complete!!',0x0d,0x0a,0x00
loading_msg		db		'gfw Loading',0x00
errmsg			db		'disk read error',0x0d,0x0a,0x00

strack			db		2		;��������ǂݏo��
scylinder		db		0
shead			db		0
fdcnt			db		0

scnt			dw		GFW_FILE_SIZE / 512

;drive_num		dw		0

hexlist			db		'0123456789abcdef'


;AL���W�X�^�̓��e��\������
print_al:
	pusha
	
	lea			bx, [hexlist]
	mov			dl, al
	
	mov			cl, 4		;4bit�V�t�g
	shr			al, cl
	xlat
	
	putchar		al
	
	lea			bx, [hexlist]
	mov			al, dl
	and			al, 0Fh
	xlat
	
	putchar		al		;�}�N��
	
	popa
	ret

start:

;����CS���W�X�^�̒l��07C0h
;���̂Ƃ���f�[�^�̈悪CS�Ɠ���Z�O�����g���ɂ���̂ŁADS��CS��
;����̒l�ɂ��Ă����B
	cli
	mov			ax, cs
	mov			ds, ax
	
	mov			ax, 0x7000
	mov			ss, ax			;�X�^�b�N������Z�O�����g�ɂ���
	mov			sp, 0xffff		;sp�Ƃ肠�������ꂾ��
	sti
	
;�\���ȗ̈悪����Ƃ���܂ŁA���g���ړ����悤
moveself:

	mov			si, 0  ;ds�͏���������Ă��邩�� cs = ds�@si���W�X�^�ɃI�t�Z�b�g������B
	mov			ax, BOOTCODE_SEG
	mov			es, ax				;���z����Z�O�����g
	mov			di, 0				;���z����I�t�Z�b�g
	mov			cx, C_COUNTER		; 512 / 2(WORD) = 256
	
	;�ړ�
	rep
	movsw
	
	;�]�����
	jmp			BOOTCODE_SEG:go
	
go:
	;�]�����ꂽBOOTCODE_SEG�ȉ���go��
	
	;�X�^�b�N�Đݒ�
	cli
	mov			ax, cs
	mov			ds, ax
	;mov			ax, 0x7000
	mov			ss, ax				;�X�^�b�N������Z�O�����g
	mov			sp, 0xffff			;�Ƃ肠�����X�^�b�N���ꂾ��
	sti
	
diskreset:
	xor			ax, ax
	int			0x13				;FDC��������
	jc			diskreset			;�Ƃ肠������������܂ł�葱����
	
_prepare_readfd:
	
	mov			ax, CODE_TEMP_SEG	;�]����Z�O�����g�A�h���X
	mov			es, ax
	
	mov			bx, 0				;�]����I�t�Z�b�g�A�h���X
	mov			ah, 0x02			;���荞�ݗp�ԍ�

	printstr	loading_msg
	
_reset_counter:
	mov			[fdcnt], byte RST_CNT	;�g���C����񐔂̃J�E���^�[���Z�b�g
	jmp			_read_fd_loop
	
_fd_faild:
	putchar		0x0d
	putchar		0x0a
	printstr	errmsg
	
	xor			ax, ax
	xor			dl, dl
	int			0x13
	
__L0:
	jmp			__L0
	
_read_fd_loop:
	mov			bx, 0
	mov			ah, 0x02
	mov			al, 1
	mov			cl, [strack]		;���g���b�N(�Z�N�^)�ڂ���ǂݍ��ނ�
	mov			ch, [scylinder]		;���V�����_�ڂ���ǂݍ��ނ�
	mov			dh, [shead]			;�ǂ̃w�b�h����ǂݍ��ނ�
	
	dec			byte [fdcnt]
	jz			_fd_faild
	
_read_fd:
	
	int			0x13					;read floppy
	jc			_read_fd_loop
	
	mov			[fdcnt], byte RST_CNT	;���[�h������������J�E���^�[���Z�b�g
	putchar		'.'						;���[�h�����̃|�C���g
	
	;�Z�O�����g����512byte�������Ă䂭
	mov			ax, es
	add			ax, 0x20
	mov			es, ax
	
	inc			cl
	cmp			cl, 19
	je			_next_head
	
	mov			[strack], cl
	
	dec			word [scnt]
	cmp			[scnt], word 0x0
	jne			_read_fd_loop
	
	jmp			_read_complete
	
_next_head:

	inc			dh
	cmp			dh, 2
	je			_next_cylinder
	
	mov			cl, 1
	mov			[strack], cl
	mov			[shead], dh
	
	dec			word [scnt]
	cmp			[scnt], word 0x0
	jne			_read_fd_loop
	
	jmp			_read_complete
	
_next_cylinder:
	
	inc			ch
	
	mov			[scylinder], ch
	mov			[strack], byte 1
	mov			[shead], byte 0
	
	dec			word [scnt]
	cmp			[scnt], word 0x0
	jne			_read_fd_loop
	
	jmp			_read_complete
	
_read_complete:
	
stop_FDmoter:
	xor			ax, ax
	xor			dl, dl
	int			0x13
	
jumppad:
	printstr	ReadFDOk

	jmp			CODE_TEMP_SEG:0000h			;secondboot��
	
times 510-($-$$) db 0				;�Ƃ肠����510byte�ɑ���Ȃ�������0��
dw	SIG1							;�Ō��2byte���̓u�[�g�V�O�l�`��

