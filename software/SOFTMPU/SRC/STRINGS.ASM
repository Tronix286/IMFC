;
; Copyright (C) 2013-2018 bjt, elianda
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation; either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
;

; ------------------------------------------
; SoftMPU by bjt - Software MPU-401 Emulator
; ------------------------------------------
;
; String data
;

Header          DB              0DAh
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0BFh,0Dh,0Ah
                DB              0B3h,'SoftMPU 1.91 ',0FEh,' Software MPU-401 Emulator',0B3h,0Dh,0Ah
                DB              0B3h,'                                        ',0B3h,0Dh,0Ah
                DB              0B3h,'Copyright (C) 2023 Tronix (IMFC version)',0B3h,0Dh,0Ah
                DB              0B3h,'Copyright (C) 2013-2014  bjt, elianda   ',0B3h,0Dh,0Ah
                DB              0B3h,'Copyright (C) 2002-2013  The DOSBox Team',0B3h,0Dh,0Ah
                DB              0C0h
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h,0C4h
                DB              0C4h,0D9h,0Dh,0Ah,'$'
NoEMM           DB              '! Error: EMM386 4.46+ or QEMM 7.03+ not detected',0Dh,0Ah,'$'
EMMError        DB              '! Error: Port trap failed. Please submit a bug report',0Dh,0Ah,'$'
LoadedError     DB              '! Error: SoftMPU is already loaded',0Dh,0Ah,'$'


MFCError        DB              '! Error: MFC not detected',0Dh,0Ah,'$'
MFCSuccessStart DB              0FBh,' MFC detected at port ','$'
MFCSuccessEnd   DB              0Dh,0Ah,'$'
MFCPortNum      DB              '    ','$'

SBErrorStart    DB              '! Error: Sound Blaster not detected at port ','$'
SBErrorMid      DB              ' IRQ ','$'
SBErrorEnd      DB              0Dh,0Ah,'$'
SBSuccessStart  DB              0FBh,' Sound Blaster (DSP ','$'
SBSuccessMid1   DB              ' detected at port ','$'
SBSuccessMid2   DB              ' IRQ ','$'
SBSuccessEnd    DB              0Dh,0Ah,'$'
SBDSPVer        DB              '0.00)','$'
SBPortNum       DB              '   ','$'
SBIRQNum        DB              '  ','$'
MPUErrorStart   DB              '! Error: MPU-401 not detected at port ','$'
MPUErrorEnd     DB              0Dh,0Ah,'$'
MPUSuccessStart DB              0FBh,' MPU-401 detected at port ','$'
MPUSuccessEnd   DB              0Dh,0Ah,'$'
MPUPortNum      DB              '   ','$'
COMError        DB              '! Error: Serial interface not detected',0Dh,0Ah,'$'
COMSuccessStart DB              0FBh,' Serial interface detected at port ','$'
COMSuccessEnd   DB              0Dh,0Ah,'$'
COMPortNum      DB              '   ','$'
SBMIDIEnabled   DB              'i SB-MIDI mode enabled',0Dh,0Ah,'$'
DelaysEnabled   DB              'i SYSEX delays enabled',0Dh,0Ah,'$'
FakeEnabled     DB              'i Simulated "All Notes Off" enabled',0Dh,0Ah,'$'
MPUIRQDisabled  DB              'i MPU-401 interrupts disabled',0Dh,0Ah,'$'
LoadedStart     DB              0Dh,0Ah,0Eh,' SoftMPU active at port ','$'
LoadedMid       DB              ' IRQ ','$'
LoadedEnd       DB              0Dh,0Ah,'$'
HelpText        DB              'Usage: SOFTMPU /MPU:nnn [/SB:nnn /IRQ:nn] [/OUTPUT:[outputs]]',0Dh,0Ah
                DB              '               [/DELAYSYSEX] [/RA50]',0Dh,0Ah
                DB              0Dh,0Ah
                DB              '  /MPU:nnn     MPU-401 base port address (220-350)',0Dh,0Ah
                DB              '  /SB:nnn      Sound Blaster base port address (220-280)',0Dh,0Ah
                DB              '  /IRQ:nn      Sound Blaster interrupt (2-11)',0Dh,0Ah
                DB              '  /OUTPUT:     Use alternate output mode',0Dh,0Ah
                DB              '  outputs        SBMIDI  Sound Blaster MIDI',0Dh,0Ah
                DB              '                 COMn    Serial interface (1-4)',0Dh,0Ah
                DB              '                 MFC     IBM Music Feature Card',0Dh,0Ah
                DB              '  /DELAYSYSEX  Prevent Rev.0 MT-32 buffer overflow',0Dh,0Ah
                DB              '  /RA50        Simulate "All Notes Off" for Roland RA-50',0Dh,0Ah,'$'
EMMDevice       DB              'EMMXXXX0',00h
EMMDevice2      DB              'EMMQXXX0',00h
QEMMDevice      DB              'QEMM386$',00h
