#ifndef mfc_h_sentinel
#define mfc_h_sentinel

//long mtime(void);
int mfc_getbyte(void);
void mfc_init(void);
void mfc_note(int chan, int notenum, int vel);
void mfc_putbyte(int x);
void mfc_setcmd(void);
void mfc_setdata(void);
void mfc_setpath(int p1, int p2, int p3, int p4, int p5);
void mfc_waitbyte(int data);
void mfc_setbank(int bank);
void mfc_setchanbank(int chan, int bank);


#endif
