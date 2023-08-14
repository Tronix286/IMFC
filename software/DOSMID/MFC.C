/* Sample Turbo C program demonstrating Music Feature card access.

   Kevin Weiner -- 2/11/90

*/

#include <dos.h>
#include <stdio.h>
#include <conio.h>
//#include <sys\timeb.h>
#include "timer.h"

int            base, lastdata, piu0, piu1, piu2, pcr, tcr;
unsigned char  txrdy, rxrdy, exr8, ext8;

//long mtime(void);
int mfc_getbyte(void);
void mfc_init(void);
void mfc_note(int chan, int notenum, int vel);
void mfc_putbyte(int x);
void mfc_setcmd(void);
void mfc_setdata(void);
void mfc_setpath(int p1, int p2, int p3, int p4, int p5);
void mfc_waitbyte(int data);


/* mtime -- Return system time in milliseconds */

/*
long mtime(void)
{
    struct timeb timer;

    ftime(&timer);
    return(timer.time*1000+timer.millitm);
}

*/
/* getbyte -- Return a data byte from the MFC, or -1 if none */

int mfc_getbyte(void)
{
    unsigned long time, starttime;
    unsigned char stat;

    time = 0;
    stat = inp(piu2);              /* Check status */
    if ((stat & rxrdy) == 0) {         /* No data - wait */
      timer_read(&starttime);
      //starttime = mtime();
      while (((stat & rxrdy) == 0) & (time < 500000l)) {
	 stat = inp(piu2);
	 timer_read(&time);
	 time = time - starttime;
	 //time = mtime() - starttime;
      }
    }
    lastdata = -1;
    if (time < 500000l) {
      lastdata = inp(piu0);        /* Read data */
      if ((stat & exr8) != 0)
        lastdata = lastdata + 256;     /* Add 256 if command bit set */
    }
    return(lastdata);
}

/* mfcinit -- Initialize MFC parameters, set paths */

void mfc_init(void)
{
    base = 0x2A20;                     /* Port address for 1st mfc */
    lastdata = -1;
    piu0 = base + 0;                   /* Parallel i/o registers */
    piu1 = base + 1;
    piu2 = base + 2;
    pcr = base + 3;                    /* PIU control register */
    tcr = base + 8;                    /* Total control register */
    txrdy = 2;                         /* Ok to send */
    rxrdy = 0x20;                      /* Data ready to read */
    exr8 = 0x80;                       /* 8th bit read */
    ext8 = 0x10;                       /* Transmit 8th bit */
    if (inp(piu2) == 0xff)
      printf("Music Feature Card not present\n");
    else {
      outp(pcr, 0xbc);
      mfc_setpath(0x1f, 0x1f, 0x1f, 0x1f, 0x1f);
    }
}

/* note -- Play note on channel (1-16) with specified velocity */

void mfc_note(int chan, int notenum, int vel)
{
    mfc_putbyte(0x90 + chan - 1);
    mfc_putbyte(notenum);
    mfc_putbyte(vel);
}

/* putbyte -- Write a data byte to the MFC */

void mfc_putbyte(int x)
{
    unsigned long time, starttime;
    unsigned char stat;

    time = 0;
    stat = inp(piu2);              /* Check status */
    if ((stat & txrdy) == 0) {         /* Not ready - wait */
      timer_read(&starttime);
      //starttime = mtime();
      while (((stat & txrdy) == 0) & (time < 500000l)) {
	 stat = inp(piu2);
	 //time = mtime() - starttime;
	 timer_read(&time);
	 time = time - starttime;
      }
    }
    outp(piu1, (x & 0xff));
}

/* setcmd -- Set MFC to command mode for subsequent output */

void mfc_setcmd(void)
{
    outp(tcr, ext8);
}

/* setdata -- Set MFC to data mode for subsequent output */

void mfc_setdata(void)
{
    outp(tcr, 0);
}

/* setpath -- Set MFC MIDI paths

   p1 - MIDI IN to System
   p2 - System to MIDI OUT
   p3 - MIDI IN to Sound Processor
   p4 - System to Sound Processor
   p5 - MIDI IN to MIDI OUT
   (Set to 0x1f to enable, 0 to disable)
*/

void mfc_setpath(int p1, int p2, int p3, int p4, int p5)
{
    mfc_setcmd();
    mfc_putbyte(0x1e2);
    mfc_putbyte(p1);  mfc_putbyte(p2);  mfc_putbyte(p3);  mfc_putbyte(p4);  mfc_putbyte(p5);
    mfc_waitbyte(0x1e2);
    mfc_setdata();
}

void mfc_setbank(int bank)
{
  int i;
  for (i = 0; i < 15; i++) {
	mfc_putbyte(0xf0);	// set max voices for each channel
	mfc_putbyte(0x43);
	mfc_putbyte(0x10 | i);
	mfc_putbyte(0x15);
	mfc_putbyte(0x00);
	mfc_putbyte(0x01);	// notes =1
	mfc_putbyte(0xf7);

	if (i == 7) {
	mfc_putbyte(0xf0);	// set midi channel for drum (10)
	mfc_putbyte(0x43);
	mfc_putbyte(0x10 | i);
	mfc_putbyte(0x15);
	mfc_putbyte(0x01);
	mfc_putbyte(0x9);	// channel 10 (9)
	mfc_putbyte(0xf7);
	}

	mfc_putbyte(0xf0);	// set bank for each channel
	mfc_putbyte(0x43);
	mfc_putbyte(0x10 | i);
	mfc_putbyte(0x15);
	mfc_putbyte(0x04);
	mfc_putbyte(bank);
	mfc_putbyte(0xf7);
  }
}

void mfc_setchanbank(int chan, int bank)
{
	mfc_putbyte(0xf0);	// set bank for each channel
	mfc_putbyte(0x43);
	mfc_putbyte(0x10 | chan);
	mfc_putbyte(0x15);
	mfc_putbyte(0x04);
	mfc_putbyte(bank);
	mfc_putbyte(0xf7);
}
/* waitbyte -- Wait for specified byte from MFC, or timeout */

void mfc_waitbyte(int data)
{
    int x;

    x = mfc_getbyte();
    while ((x != data) & (x >= 0)) x = mfc_getbyte();
}
