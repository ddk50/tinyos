; nasm�p�̃}�N�����B
; BIOS�ŕ�������񕪂����\���ł���B
; BIOS���荞�݂��g���Ă���̂Ńv���e�N�g���[�h�ł͎g���Ȃ����B
; 
%macro printstr 1
	pusha				;������ax���W�X�^�g������A�Ƃ肠�����ޔ�
	mov		si, %1
	
%%loop:
	lodsb
	cmp		al, 0
	je		%%end		;������������W���[���v
	mov		ah, 0Eh
	mov		bx, 7
	int		10h
	
	jmp		%%loop

%%end:
	popa				;�ޔ�����ax���W�X�^�𕜋A�����悤
	
%endmacro


%macro putchar 1
	pusha
	
	mov		al, %1
	mov		ah, 0eh
	
	mov		bx, 0
	
	int		10h
	
	popa
%endmacro

