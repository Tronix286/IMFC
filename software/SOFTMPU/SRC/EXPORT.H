/*
 *  Copyright (C) 2002-2012  The DOSBox Team
 *  Copyright (C) 2013-2014  bjt, elianda
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * ------------------------------------------
 * SoftMPU by bjt - Software MPU-401 Emulator
 * ------------------------------------------
 *
 * C types/functions used from ASM
 *
 */

#ifndef EXPORT_H

#define NULL 0 /* SOFTMPU */
#define RTCFREQ 4000 /* SOFTMPU: RTC interrupt frequency */

typedef unsigned char Bit8u;
typedef unsigned int Bitu;

typedef enum bool {false = 0,true = 1} bool;
typedef enum EventID {MPU_EVENT,RESET_DONE,EOI_HANDLER,NUM_EVENTS} EventID;
typedef enum OutputMode {M_MPU401,M_SBMIDI,M_SERIAL,M_MFC} OutputMode;

/* SOFTMPU: QEMM interface parameters */
typedef struct {
        bool installed;
        void far* qpi_entry;
} QEMMInfo;

/* Interface functions */
void PIC_Update(bool blocking);

void MPU401_Init(void far* qpientry,Bitu sbport,Bitu irq,Bitu mpuport,Bitu serialport,OutputMode outputmode,bool delaysysex,bool fakeallnotesoff);
void MPU401_SetEnableSBIRQ(bool enable);
void MPU401_SetEnableMPUVerFix(bool enable);
Bitu MPU401_ReadStatus(void);
void MPU401_WriteCommand(Bitu val);
Bitu MPU401_ReadData(void);
void MPU401_WriteData(Bitu val);

#define EXPORT_H
#endif
