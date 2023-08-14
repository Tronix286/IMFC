/*
 *  Copyright (C) 2002-2013  The DOSBox Team
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
 * Based on original midi.c from DOSBox
 *
 */

/* SOFTMPU: Moved exported functions & types to header */
#include "export.h"

/* SOFTMPU: Don't need these includes */
/*#include <assert.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <algorithm>

#include "SDL.h"

#include "dosbox.h"
#include "cross.h"
#include "support.h"
#include "setup.h"
#include "mapper.h"
#include "pic.h"
#include "hardware.h"
#include "timer.h"*/

/* SOFTMPU: Additional defines, typedefs etc. for C */
typedef unsigned long Bit32u;
typedef int Bits;

#define SYSEX_SIZE 1024
#define RAWBUF  1024

/* SOFTMPU: Note tracking for RA-50 */
#define MAX_TRACKED_CHANNELS 16
#define MAX_TRACKED_NOTES 8

static char* MIDI_welcome_msg = "\xf0\x41\x10\x16\x12\x20\x00\x00    SoftMPU v1.9    \x24\xf7"; /* SOFTMPU */

static Bit8u MIDI_note_off[3] = { 0x80,0x00,0x00 }; /* SOFTMPU */

static Bit8u MIDI_evt_len[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x10
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x30
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x70

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x80
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0x90
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xc0
  2,2,2,2, 2,2,2,2, 2,2,2,2, 2,2,2,2,  // 0xd0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xe0

  0,2,3,2, 0,0,1,0, 1,0,1,1, 1,0,1,0   // 0xf0
};

/* SOFTMPU: Note tracking for RA-50 */
typedef struct {
        Bit8u used;
        Bit8u next;
        Bit8u notes[MAX_TRACKED_NOTES];
} channel;

channel tracked_channels[MAX_TRACKED_CHANNELS];

static struct {
	Bitu mpuport;
        Bitu sbport;
        Bitu serialport;
	Bitu status;
	Bitu cmd_len;
	Bitu cmd_pos;
	Bit8u cmd_buf[8];
	Bit8u rt_buf[8];
	struct {
		Bit8u buf[SYSEX_SIZE];
		Bitu used;
                Bitu usedbufs;
		Bitu delay;
		Bit32u start;
	} sysex;
        bool fakeallnotesoff;
	bool available;
	/*MidiHandler * handler;*/ /* SOFTMPU */
} midi;

/* SOFTMPU: Sysex delay is decremented from PIC_Update */
Bitu MIDI_sysex_delay;

/* SOFTMPU: Also used by MPU401_ReadStatus */
OutputMode MIDI_output_mode;

/* SOFTMPU: Initialised in mpu401.c */
extern QEMMInfo qemm;

int old_drum_voice_number = 0;

/*
[Bank : 3]
              
 1:Brass         2:Horn          3:Trumpet       4:Lo String
 5:Strings       6:Piano         7:NewElPiano    8:Elec Grand
 9:JazzGuitar   10:Elec Bass    11:Wood Bass    12:ElecOrgan1
13:ElecOrgan2   14:PipeOrgan1   15:PipeOrgan2   16:Flute
17:Piccolo      18:Oboe         19:Clarinet     20:Glockenspl
21:Vibes        22:Xylophone    23:Koto         24:Zither
25:Clavichord   26:Harpsichrd   27:Bells        28:Harp
29:Smad Synth   30:Harmonica    31:Steel Drum   32:Timpani
33:LoString 2   34:Horn Low     35:Whistle      36:ZingPlop
37:Metal        38:Heavy        39:Funk Synth   40:Voices
41:Marimba      42:ElecBass 2   43:Snare Drum   44:Ride Cymb
45:Tom Tom      46:Mars To      47:Storm        48:Wind Bell
 
[Bank : 4]
 
 1:Uprt Piano    2:SoftPiano    3:Piano 2       4:Piano 3
 5:Piano 4       6:Piano 5       7:Ph Grand      8:Grand
 9:Deep Grand   10:Lo Piano 1   11:Lo Piano 2   12:El Grand 2
13:Honkey 1     14:Honkey 2     15:Pf Bell      16:Pf Vibes
17:NewElPian2   18:NewElPian3   19:NewElPian4   20:NewElPian5
21:ElecPiano1   22:ElecPiano2   23:ElecPiano3   24:ElecPiano4
25:ElecPiano5   26:High Tin     27:Hard Tin     28:Perc Pf
29:Wood Pf      30:EP String    31:EP Brass     32:Clav 2
33:Clav 3       34:Clav 4       35:Fuzz Clav    36:Mute Clav
37:MuteClav 2   38:SynthClav1   39:SynthClav2   40:SynthClav3
41:SynthClav4   42:Harpsi 2     43:Harpsi 3     44:Harpsi 4
45:Harpsi 5     46:Circus       47:Celeste      48:Squeeze
 
[Bank : 5]
 
 1:Horn 2        2:Horn 3        3:Horns         4:Flugelhorn
 5:Trombone      6:Trumpet 2     7:Brass 2       8:Brass 3
 9:HardBrass1   10:HardBrass2   11:HardBrass3   12:HardBrass4
13:HuffBrass    14:PercBrass1   15:PercBrass2   16:String 1
17:String 2     18:String 3     19:String 4     20:SoloViolin
21:RichStrng1   22:RichStrng2   23:RichStrng3   24:RichStrng4
25:Cello 1      26:Cello 2      27:LoString 3   28:LoString 4
29:LoString 5   30:Orchestra    31:5th String   32:Pizzicato1
33:Pizzicato2   34:Flute 2      35:Flute 3      36:Flute 4
37:Pan Flute    38:SlowFlute    39:5th Flute    40:Oboe 2
41:Bassoon      42:Reed         43:Harmonca 2   44:Harmonca 3
45:Harmonca 4   46:Mono Sax     47:Sax 1        48:Sax 2
 
[Bank : 6]
 
 1:FunkSynth2    2:FunkSynth3    3:SynthOrgan    4:Synth Feed
 5:Synth Harm    6:Synth Clar    7:Synth Lead    8:Huff Talk
 9:So Heavy     10:Hollow       11:Schmooh      12:Mono Synth
13:Cheeky       14:Synth Bell   15:Synth Pluk   16:ElecBass 3
17:RubberBass   18:Solo Bass    19:Pluck Bass   20:Uprt Bass
21:Fretless     22:Flap Bass    23:Mono Bass    24:SynthBass1
25:SynthBass2   26:SynthBass3   27:SynthBass4   28:SynthBass5
29:SynthBass6   30:SynthBass7   31:Marimba 2    32:Marimba 3
33:Xylophone2   34:Vibe 2       35:Vibe 3       36:Glocken 2
37:TublrBell1   38:TublrBell2   39:Bells 2      40:TempleGong
41:Steel Drum   42:Elect Drum   43:Hand Drum    44:SynTimpani
45:Clock        46:Heifer       47:SnareDrum2   48:SnareDrum3
 
[Bank : 7]
 
 1:JazzOrgan1    2:JazzOrgan2    3:CmboOrgan1    4:CmboOrgan2
 5:El Organ 3    6:El Organ 4    7:El Organ 5    8:El Organ 6
 9:El Organ 7   10:El Organ 8   11:Small Pipe   12:Mid Pipe
13:Big Pipe     14:Soft Pipe    15:Organ        16:Guitar
17:FolkGuitar   18:PluckGuitr   19:BriteGuitr   20:FuzzGuitar
21:Zither 2     22:Lute         23:Banjo        24:Soft Harp
25:Harp 2       26:Harp 3       27:Soft Koto    28:Hard Koto
29:Sitar 1      30:Sitar 2      31:Huff Synth   32:Fantasy
33:SynthVoice   34:Male Voice   35:Touch Tone   36:Racing
37:Water        38:Wild War     39:Ghostie      40:Wave
41:Space 1      42:SpaceChime   43:Space Talk   44:Winds
45:Smash        46:Alarm        47:Helicopter   48:Sine Wave
*/

struct GM_to_mfc_t
{
    int bank;
    int instr;
};

struct GM_to_mfc_t GM_to_mfc[128] = {
// Piano
{4, 1},	 //1.	Acoustic Grand Piano  -> 4/1:Uprt Piano
{4, 2},  //2.	Bright Acoustic Piano -> 4/2:SoftPiano
{4, 12}, //3.	Electric Grand Piano  -> 4/12:El Grand 2
{4, 13}, //4.	Honky-tonk Piano      -> 4/13:Honkey 1
{4, 21}, //5.	Electric Piano 1      -> 4/21:ElecPiano1
{4, 22}, //6.	Electric Piano 2      -> 4/22:ElecPiano2
{3, 26}, //7.	Harpsichord           -> 3/26:Harpsichrd
{3, 25}, //8.	Clavi                 -> 3/25:Clavichord
// Chromatic Percussion
{4, 47}, //9.	Celesta		      -> 4/47:Celeste
{3, 20}, //10.	Glockenspiel          -> 3/20:Glockenspl
{6, 33}, //11.	Music Box             -> 6/33:Xylophone2 (?)
{6, 34}, //12.	Vibraphone            -> 6/34:Vibe 2
{3, 41}, //13.	Marimba               -> 3/41:Marimba
{3, 22}, //14.	Xylophone             -> 3/22:Xylophone
{6, 38}, //15.	Tubular Bells         -> 6/38:TublrBell2
{3, 24}, //16.	Dulcimer              -> 3/24:Zither (?)
// Organ
{7, 1}, //17.	Drawbar Organ	      -> 7/1:JazzOrgan1
{7, 2}, //18.	Percussive Organ      -> 7/2:JazzOrgan2
{7, 3}, //19.	Rock Organ            -> 7/3:CmboOrgan1
{7, 4}, //20.	Church Organ          -> 7/4:CmboOrgan2
{7, 5}, //21.	Reed Organ            -> 7/5:El Organ 3
{7, 6}, //22.	Accordion             -> 7/6:El Organ 4
{3, 30},//23.	Harmonica             -> 3/30:Harmonica
{7, 7}, //24.	Tango Accordion       -> 7/7:El Organ 5
// Guitar
{7, 16},//25.	Acoustic Guitar (nylon)	-> 7/16:Guitar
{7, 17},//26.	Acoustic Guitar (steel) -> 7/17:FolkGuitar
{3, 9}, //27.	Electric Guitar (jazz)  -> 3/9:JazzGuitr
{7, 19},//28.	Electric Guitar (clean) -> 7/19:BriteGuitr
{7, 20},//29.	Electric Guitar (muted) -> 7/20:FuzzGuitar
{3, 9}, //30.	Overdriven Guitar	-> 3/9:JazzGuitar
{7, 20},//31.	Distortion Guitar       -> 7/20:FuzzGuitar
{7, 18},//32.	Guitar harmonics        -> 7/18:PluckGuitr
// Bass
{6, 18},//33.	Acoustic Bass           -> 6/18:Solo Bass
{6, 16},//34.	Electric Bass (finger)  -> 6/16:ElecBass 3
{3, 10},//35.	Electric Bass (pick)    -> 3/10:Elec Bass
{6, 17},//36.	Fretless Bass		-> 6/17:RubberBass
{6, 19},//37.	Slap Bass 1		-> 6/19:Pluck Bass
{6, 20},//38.	Slap Bass 2		-> 6/20:Uprt Bass
{6, 24},//39.	Synth Bass 1		-> 6/24:SynthBass1
{6, 25},//40.	Synth Bass 2		-> 6/25:SynthBass2
// Strings
{5, 20},//41.	Violin			-> 5/20:SoloViolin
{5, 16},//42.	Viola			-> 5/16:String 1
{5, 25},//43.	Cello			-> 5/25:Cello 1
{5, 21},//44.	Contrabass              -> 5/21:RichStrng1
{5, 17},//45.	Tremolo Strings         -> 5/17:String 2
{5, 32},//46.	Pizzicato Strings	-> 5/32:Pizzicato1
{5, 30},//47.	Orchestral Harp         -> 5/30:Orchestra
{3, 32},//48.	Timpani                 -> 3/32:Timpani
// Ensemble
{5, 18},//49.	String Ensemble 1	-> 5/18:String 3
{5, 19},//50.	String Ensemble 2	-> 5/19:String 4
{5, 27},//51.	SynthStrings 1		-> 5/27:LoString 3
{5, 28},//52.	SynthStrings 2		-> 5/28:LoString 4
{5, 22},//53.	Choir Aahs		-> 5/22:RichStrng2
{5, 23},//54.	Voice Oohs		-> 5/23:RichStrng3
{5, 24},//55.	Synth Voice		-> 5/24:RichStrng4
{5, 31},//56.	Orchestra Hit		-> 5/31:5th String
// Brass
{5, 6}, //57.	Trumpet			-> 5/6:Trumpet 2
{5, 5}, //58.	Trombone		-> 5/5:Trombone
{5, 9}, //59.	Tuba			-> 5/9:HardBrass1
{3, 3}, //60.	Muted Trumpet		-> 3/3:Trumpet
{5, 1}, //61.	French Horn		-> 5/1:Horn 2
{5, 9}, //62.	Brass Section		-> 5/9:HardBrass1
{5, 10},//63.	SynthBrass 1		-> 5/10:HardBrass2
{5, 11},//64.	SynthBrass 2		-> 5/11:HardBrass3
// Reed
{5, 48},//65.	Soprano Sax		-> 5/48:Sax 2
{5, 47},//66.	Alto Sax		-> 5/47:Sax 1
{5, 47},//67.	Tenor Sax		-> 5/47:Sax 1
{5, 48},//68.	Baritone Sax            -> 5/48:Sax 2
{3, 18},//69.	Oboe			-> 3/18:Oboe
{3, 2}, //70.	English Horn		-> 3/2:Horn
{5, 41},//71.	Bassoon			-> 5/41:Bassoon
{3, 19},//72.	Clarinet		-> 3/19:Clarinet
// Pipe
{3, 17},//73.	Piccolo			-> 3/17:Piccolo
{3, 16},//74.	Flute			-> 3/16:Flute
{5, 37},//75.	Recorder		-> 5/37:Pan Flute
{5, 34},//76.	Pan Flute		-> 5/34:Flute 2
{5, 35},//77.	Blown Bottle		-> 5/35:Flute 3
{5, 36},//78.	Shakuhachi		-> 5/36:Flute 4
{3, 35},//79.	Whistle			-> 3/35:Whistle
{5, 38},//80.	Ocarina			-> 5/38:SlowFlute
// Synth Lead
// !!! must be bank 6 inst 0 - 15 !!!
{4, 17},//81.	Lead 1 (square)		-> 4/17:NewElPian2
{4, 18},//82.	Lead 2 (sawtooth)	-> 4/18:NewElPian3
{4, 19},//83.	Lead 3 (calliope)	-> 4/19:NewElPian4
{4, 20},//84.	Lead 4 (chiff)		-> 4/20:NewElPian5
{4, 21},//85.	Lead 5 (charang)	-> 4/21:ElecPiano1
{4, 22},//86.	Lead 6 (voice)		-> 4/21:ElecPiano2
{4, 23},//87.	Lead 7 (fifths)		-> 4/21:ElecPiano3
{4, 24},//88.	Lead 8 (bass + lead)	-> 4/21:ElecPiano4
// Synth Pad
{4, 25},//89.	Pad 1 (new age)		-> 4/25:ElecPiano5
{4, 26},//90.	Pad 2 (warm)		-> 4/26:High Tin
{4, 27},//91.	Pad 3 (polysynth)	-> 4/27:Hard Tin
{4, 28},//92.	Pad 4 (choir)		-> 4/28:Perc Pf
{4, 29},//93.	Pad 5 (bowed)		-> 4/29:Wood Pf
{4, 30},//94.	Pad 6 (metallic)	-> 4/30:EP String
{4, 31},//95.	Pad 7 (halo)		-> 4/31:EP Brass
{3, 7}, //96.	Pad 8 (sweep)		-> 3/7:NewElPiano
// Synth Effects
{0, 0}, //97.	FX 1 (rain)
{0, 0}, //98.	FX 2 (soundtrack)
{0, 0}, //99.	FX 3 (crystal)
{0, 0}, //100.	FX 4 (atmosphere)
{0, 0}, //101.	FX 5 (brightness)
{0, 0}, //102.	FX 6 (goblins)
{0, 0}, //103.	FX 7 (echoes)
{7, 30},//104.	FX 8 (sci-fi)		-> 30:Sitar 2
// Ethnic
{7, 29},//105.	Sitar		-> 7/29:Sitar 1
{7, 23},//106.	Banjo		-> 7/23:Banjo
{7, 12},//107.	Shamisen	-> 7/12:Mid Pipe
{3, 23},//108.	Koto		-> 3/23:Koto
{7, 13},//109.	Kalimba		-> 7/13:Big Pipe
{7, 11},//110.	Bag pipe	-> 7/11:Small Pipe
{7, 14},//111.	Fiddle		-> 7/14:Soft Pipe
{7, 15},//112.	Shanai		-> 7/15:Organ
// Percussive
{6, 37},//113.	Tinkle Bell	-> 6/37:TublrBell1
{6, 34},//114.	Agogo		-> 6/34:Vibe 2
{6, 41},//115.	Steel Drums	-> 6/41:Steel Drum
{6, 35},//116.	Woodblock	-> 6/35:Vibe 3
{6, 47},//117.	Taiko Drum	-> 6/47:SnareDrum2
{6, 43},//118.	Melodic Tom	-> 6/43:Hand Drum
{6, 42},//119.	Synth Drum	-> 6/42:Elect Drum
{6, 46},//120.	Reverse Cymbal	-> 6/46:Heifer
// Sound Effects
{7, 35},//121.	Guitar Fret Noise 	-> 7/35:Touch Tone
{7, 32},//122.	Breath Noise		-> 7/32:Fantasy
{7, 40},//123.	Seashore		-> 7/40:Wave
{7, 44},//124.	Bird Tweet		-> 7/44:Winds
{7, 46},//125.	Telephone Ring		-> 7/46:Alarm
{7, 47},//126.	Helicopter		-> 7/47:Helicopter
{7, 48},//127.	Applause		-> 7/48:Sine Wave
{7, 45}//128.	Gunshot			-> 7/45:Smash
};
 
struct GMdrum_to_mfc_t
{
    int instr;
    int note;
};


// MUSIC.COM keyboard to notes translation
// a = 48; s = 55; d = 60; f = 67; g = 72
// h = 79; j = 84; k = 91; l = 96; ; = 103
// ' = 108
struct GMdrum_to_mfc_t GMdrum_to_mfc[] = {
{42, 48}, 	// 35 Acoustic Bass Drum
{42, 55},       // 36 Bass Drum 1
{45, 108},	// 37 Side Stick
{47, 55},	// 38 Acoustic Snare
{46, 96},	// 39 Hand Clap
{46, 96},	// 40 Electric Snare
{40, 60},	// 41 Low Floor Tom
{45, 103},	// 42 Closed Hi Hat
{40, 67},	// 43 High Floor Tom
{45, 103},	// 44 Pedal Hi-Hat
{40, 72},	// 45 Low Tom
{43, 67},	// 46 Open Hi-Hat
{42, 60},	// 47 Low-Mid Tom
{42, 72},	// 48 Hi-Mid Tom
{43, 72}, 	// 49 Crash Cymbal 1
{42, 79},	// 50 High Tom

{47, 120},	// 51 Ride Cymbal 1
{46, 103},	// 52 Chinese Cymbal
{36, 103},	// 53 Ride Bell
{46, 103},	// 54 Tambourine
{46, 72},	// 55 Splash Cymbal
{39, 103},	// 56 Cowbell
{43, 72},	// 57 Crash Cymbal 2
{39, 72},	// 58 Vibraslap
{45, 108},	// 59 Ride Cymbal 2
{41, 67},	// 60 Hi Bongo
{41, 72},	// 61 Low Bongo
{41, 96},	// 62 Mute Hi Conga
{41, 103},	// 63 Open Hi Conga
{41, 108},	// 64 Low Conga
{45, 80},	// 65 High Timbale
{45, 100},	// 66 Low Timbale
{45, 20},	// 67 High Agogo
{45, 40},	// 68 Low Agogo
{46, 120},	// 69 Cabasa
{46, 120},	// 70 Maracas
{33, 120},	// 71 Short Whistle
{33, 100},	// 72 Long Whistle
{47, 100},	// 73 Short Guiro
{47, 100},	// 74 Long Guiro
{47, 120},	// 75 Claves
{44, 100},	// 76 Hi Wood Block
{32, 100},	// 77 Low Wood Block
{46, 120},	// 78 Mute Cuica
{46, 120},	// 79 Open Cuica
{34, 100},	// 80 Mute Triangle
{35, 100}	// 81 Open Triangle
};

int            base, lastdata, piu0, piu1, piu2, pcr, tcr;
unsigned char  txrdy, rxrdy, exr8, ext8;

/* getbyte -- Return a data byte from the MFC, or -1 if none */

int mfc_getbyte(void)
{
    unsigned int timer;
    unsigned char stat;

    timer = 0;
    //stat = inp(piu2);              /* Check status */
    _asm
	{
			mov dx,piu2
        	        cmp     qemm.installed,1
                        jne     Agetbyte_UntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        Agetbyte_UntrappedIN:
                        in      al,dx
			mov stat,al
	}
    if ((stat & rxrdy) == 0) {         /* No data - wait */
      while (((stat & rxrdy) == 0) & (timer < 500l)) {
	 //stat = inp(piu2);
    	_asm
		{
			mov dx,piu2
        	        cmp     qemm.installed,1
                        jne     AAgetbyte_UntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        AAgetbyte_UntrappedIN:
                        in      al,dx
			mov stat,al
		}
	 timer++;
      }
    }
    lastdata = -1;
    if (timer < 500l) {
      //lastdata = inp(piu0);        /* Read data */
    	_asm
		{
			mov dx,piu0
        	        cmp     qemm.installed,1
                        jne     AAAgetbyte_UntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        AAAgetbyte_UntrappedIN:
                        in      al,dx
			mov byte ptr lastdata,al
		}
      if ((stat & exr8) != 0)
        lastdata = lastdata + 256;     /* Add 256 if command bit set */
    }
    return(lastdata);
}

/* putbyte -- Write a data byte to the MFC */

void mfc_putbyte(int x)
{
    unsigned int timer;
    unsigned char stat;

    timer = 0;
    //stat = inp(piu2);              /* Check status */
    _asm
	{
			mov dx,piu2
        	        cmp     qemm.installed,1
                        jne     A_UntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        A_UntrappedIN:
                        in      al,dx
			mov stat,al
	}
    if ((stat & txrdy) == 0) {         /* Not ready - wait */
      while (((stat & txrdy) == 0) & (timer < 500l)) {
	 //stat = inp(piu2);
    _asm
	{
			mov dx,piu2
        	        cmp     qemm.installed,1
                        jne     AA_UntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        AA_UntrappedIN:
                        in      al,dx
			mov stat,al
	}
	 timer++;
      }
    }
    //outp(piu1, (x & 0xff));
    _asm
	{	
			mov dx,piu1
			mov al,byte ptr x
                        cmp     qemm.installed,1
                        jne     A_UntrappedOUT
			push 	bx
                        mov     bl,al                   ; bl = value
                        mov     ax,01A01h               ; QPI_UntrappedIOWrite
                        call    qemm.qpi_entry
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
       A_UntrappedOUT:
                        out     dx,al
	}
}

/* setcmd -- Set MFC to command mode for subsequent output */

void mfc_setcmd(void)
{
    //outp(tcr, ext8);
    _asm
	{	
			mov dx,tcr
			mov al,ext8
                        cmp     qemm.installed,1
                        jne     Asetcmd_UntrappedOUT
			push 	bx
                        mov     bl,al                   ; bl = value
                        mov     ax,01A01h               ; QPI_UntrappedIOWrite
                        call    qemm.qpi_entry
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
       Asetcmd_UntrappedOUT:
                        out     dx,al
	}
}

/* setdata -- Set MFC to data mode for subsequent output */

void mfc_setdata(void)
{
    //outp(tcr, 0);
    _asm
	{	
			mov dx,tcr
			xor al,al
                        cmp     qemm.installed,1
                        jne     Asetdata_UntrappedOUT
			push 	bx
                        mov     bl,al                   ; bl = value
                        mov     ax,01A01h               ; QPI_UntrappedIOWrite
                        call    qemm.qpi_entry
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
       Asetdata_UntrappedOUT:
                        out     dx,al
	}
}

/* waitbyte -- Wait for specified byte from MFC, or timeout */

void mfc_waitbyte(int data)
{
    int x;

    x = mfc_getbyte();
    while ((x != data) & (x >= 0)) x = mfc_getbyte();
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
      //outp(pcr, 0xbc);
    _asm
	{	
			mov dx,pcr
			mov al,0bch
                        cmp     qemm.installed,1
                        jne     Ainit_UntrappedOUT
			push 	bx
                        mov     bl,al                   ; bl = value
                        mov     ax,01A01h               ; QPI_UntrappedIOWrite
                        call    qemm.qpi_entry
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
       Ainit_UntrappedOUT:
                        out     dx,al
	}
      mfc_setpath(0x1f, 0x1f, 0x1f, 0x1f, 0x1f);
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

static void PlayMsg_SBMIDI(Bit8u* msg,Bitu len)
{
        /* Output a MIDI message to the hardware using SB-MIDI */
        /* Wait for WBS clear, then output a byte */
	_asm
	{
			mov     bx,msg
			mov     cx,len                  ; Assume len < 2^16
			add     cx,bx                   ; Get end ptr
                        mov     dx,midi.sbport
                        add     dx,0Ch                  ; Select DSP write port
	NextByte:       cmp     bx,cx
			je      End
        WaitWBS:        cmp     qemm.installed,1
                        jne     WaitWBSUntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        WaitWBSUntrappedIN:
                        in      al,dx
                        or      al,al
                        js      WaitWBS
                        mov     al,038h                 ; Normal mode MIDI output
                        cmp     qemm.installed,1
                        jne     WaitWBSUntrappedOUT
			push 	bx
                        mov     bl,al                   ; bl = value
                        mov     ax,01A01h               ; QPI_UntrappedIOWrite
                        call    qemm.qpi_entry
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        WaitWBSUntrappedOUT:
                        out     dx,al
        WaitWBS2:       cmp     qemm.installed,1
                        jne     WaitWBS2UntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        WaitWBS2UntrappedIN:
                        in      al,dx
                        or      al,al
                        js      WaitWBS2
			mov     al,[bx]
                        cmp     qemm.installed,1
                        jne     WaitWBS2UntrappedOUT
			push 	bx
                        mov     bl,al                   ; bl = value
                        mov     ax,01A01h               ; QPI_UntrappedIOWrite
                        call    qemm.qpi_entry
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        WaitWBS2UntrappedOUT:
			out     dx,al
                        inc     bx
			jmp     NextByte

                        ; Nothing more to send
	End:            nop
	}
};

static void PlayMsg_Serial(Bit8u* msg,Bitu len)
{
        /* Output a MIDI message to a serial port */
        /* Wait for transmit register clear, then output a byte */
	_asm
	{
			mov     bx,msg
			mov     cx,len                  ; Assume len < 2^16
			add     cx,bx                   ; Get end ptr
                        mov     dx,midi.serialport
        NextByte:       add     dx,05h                  ; Select line status register
                        cmp     bx,cx
			je      End
        WaitTransmit:   cmp     qemm.installed,1
                        jne     WaitTransmitUntrappedIN
			push	bx
                        mov     ax,01A00h               ; QPI_UntrappedIORead
                        call    qemm.qpi_entry
			mov	al,bl
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        WaitTransmitUntrappedIN:
                        in      al,dx
                        and     al,040h                 ; Shift register empty?
                        jz      WaitTransmit
                        sub     dx,05h                  ; Select transmit data register
			mov     al,[bx]
                        cmp     qemm.installed,1
                        jne     WaitTransmitUntrappedOUT
			push 	bx
                        mov     bl,al                   ; bl = value
                        mov     ax,01A01h               ; QPI_UntrappedIOWrite
                        call    qemm.qpi_entry
			pop	bx
                        _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                        ; Effectively skips next instruction
        WaitTransmitUntrappedOUT:
			out     dx,al
                        inc     bx
			jmp     NextByte

                        ; Nothing more to send
	End:            nop
	}
};

static void PlayMsg_MFC(Bit8u* msg,Bitu len)
{
  Bit8u command = msg[0];
  Bit8u commandMSB  = command & 0xF0;
  Bit8u channel = command & 0x0F;
  Bit8u note = msg[1];
  Bit8u velocity = msg[2];

  Bitu i;
	switch (commandMSB) {
		case 0x90:		// Note On
			if (channel == 9)
			{
	  		 if ((note >= 35) && (note <= 81))
	  		 {
	    		   // some opt for midi exchange overheading	
	    		   if (old_drum_voice_number != GMdrum_to_mfc[note-35].instr) {
    	                   mfc_putbyte(0xC0 | channel);
    	    		   mfc_putbyte(GMdrum_to_mfc[note-35].instr);
	    		   old_drum_voice_number = GMdrum_to_mfc[note-35].instr;
	    		   }
    	    		   mfc_putbyte(0x90 | channel);
    	    		   mfc_putbyte(GMdrum_to_mfc[note-35].note);
    	    		   mfc_putbyte(velocity);
	  		 }
			}
			else
			{
    	  		 mfc_putbyte(0x90 | channel);
    	  		 mfc_putbyte(note);
    	  		 mfc_putbyte(velocity);
			}
			break;
		case 0x80:		// Note Off
			if (channel == 9)
			{
	  		 if ((note >= 35) && (note <= 81))
	  		 {
    	    		   mfc_putbyte(0x80 | channel);
    	    	 	   mfc_putbyte(GMdrum_to_mfc[note-35].note);
    	    	           mfc_putbyte(64);
          		 }
			}
			else
			{
    	  		   mfc_putbyte(0x80 | channel);
    	  	  	   mfc_putbyte(note);
    	  		   mfc_putbyte(64);
			}
      			break;
		case 0xC0:		// Program change
			if (channel != 9) {	// dont touch ch9, its bank 5 percussion by default
	  		   mfc_setchanbank(channel,GM_to_mfc[note].bank-1);
    	  		   mfc_putbyte(0xC0 | channel);
    	  		   mfc_putbyte(GM_to_mfc[note].instr-1);
			}
      			break;
		default:
			for (i = 0; i < len; i++)
				mfc_putbyte(msg[i]);

	} // end switch
}

static void PlayMsg(Bit8u* msg,Bitu len)
{
        switch (MIDI_output_mode)
        {
        case M_MPU401:
                /* Output a MIDI message to the hardware */
                /* Wait for DRR clear, then output a byte */
                _asm
                {
                                mov     bx,msg
                                mov     cx,len                  ; Assume len < 2^16
                                add     cx,bx                   ; Get end ptr
                                mov     dx,midi.mpuport
                NextByte:       cmp     bx,cx
                                je      End
                                inc     dx                      ; Set cmd port
                WaitDRR:        cmp     qemm.installed,1
                                jne     WaitDRRUntrappedIN
                                push    bx
                                mov     ax,01A00h               ; QPI_UntrappedIORead
                                call    qemm.qpi_entry
                                mov     al,bl
                                pop     bx
                                _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                                ; Effectively skips next instruction
                WaitDRRUntrappedIN:
                                in      al,dx
                                test    al,040h
                                jnz     WaitDRR
                                dec     dx                      ; Set data port
                                mov     al,[bx]
                                cmp     qemm.installed,1
                                jne     WaitDRRUntrappedOUT
                                push    bx
                                mov     bl,al                   ; bl = value
                                mov     ax,01A01h               ; QPI_UntrappedIOWrite
                                call    qemm.qpi_entry
                                pop     bx
                                _emit   0A8h                    ; Emit test al,(next opcode byte)
                                                                ; Effectively skips next instruction
                WaitDRRUntrappedOUT:
                                out     dx,al
                                inc     bx
                                jmp     NextByte

                                ; Nothing more to send
                End:            nop
                }
                break;
        case M_SBMIDI:
                return PlayMsg_SBMIDI(msg,len);
        case M_SERIAL:
                return PlayMsg_Serial(msg,len);
	case M_MFC:
		return PlayMsg_MFC(msg,len);
        default:
                break;
        }
};

/* SOFTMPU: Fake "All Notes Off" for Roland RA-50 */
static void FakeAllNotesOff(Bitu chan)
{
        Bitu note;
        channel* pChan;

        MIDI_note_off[0] &= 0xf0;
        MIDI_note_off[0] |= (Bit8u)chan;

        pChan=&tracked_channels[chan];

        for (note=0;note<pChan->used;note++)
        {
                MIDI_note_off[1]=pChan->notes[note];
                PlayMsg(MIDI_note_off,3);
        }

        pChan->used=0;
        pChan->next=0;
}

void MIDI_RawOutByte(Bit8u data) {
        channel* pChan; /* SOFTMPU */

        if (midi.sysex.start && MIDI_sysex_delay) {
                _asm
                {
                                ; Bit 4 of port 061h toggles every 15.085us
                                ; Use this to time the remaining sysex delay
                                mov     ax,MIDI_sysex_delay
                                mov     bx,17                   ; Assume 4kHz RTC
                                mul     bx                      ; Convert to ticks, result in ax
                                mov     cx,ax
                                in      al,061h
                                and     al,010h                 ; Get initial value
                                mov     bl,al
                TestPort:       in      al,061h
                                and     al,010h
                                cmp     al,bl
                                je      TestPort                ; Loop until toggled
                                xor     bl,010h                 ; Invert
                                loop    TestPort
                                mov     MIDI_sysex_delay,0      ; Set original delay to zero
                }
                /*Bit32u passed_ticks = GetTicks() - midi.sysex.start;
                if (passed_ticks < midi.sysex.delay) SDL_Delay(midi.sysex.delay - passed_ticks);*/ /* SOFTMPU */
        }

	/* Test for a realtime MIDI message */
	if (data>=0xf8) {
		midi.rt_buf[0]=data;
		PlayMsg(midi.rt_buf,1);
		return;
	}        
	/* Test for a active sysex tranfer */
	if (midi.status==0xf0) {
		if (!(data&0x80)) {
                        /* SOFTMPU: Large sysex support */
                        /*if (midi.sysex.used<(SYSEX_SIZE-1))*/ midi.sysex.buf[midi.sysex.used++] = data;

                        if (midi.sysex.used==SYSEX_SIZE)
                        {
                                PlayMsg(midi.sysex.buf, SYSEX_SIZE);
                                midi.sysex.used = 0;
                                midi.sysex.usedbufs++;
                        }
			return;
		} else {
			midi.sysex.buf[midi.sysex.used++] = 0xf7;

                        if ((midi.sysex.start) && (midi.sysex.usedbufs == 0) && (midi.sysex.used >= 4) && (midi.sysex.used <= 9) && (midi.sysex.buf[1] == 0x41) && (midi.sysex.buf[3] == 0x16)) {
				/*LOG(LOG_ALL,LOG_ERROR)("MIDI:Skipping invalid MT-32 SysEx midi message (too short to contain a checksum)");*/ /* SOFTMPU */
			} else {
				/*LOG(LOG_ALL,LOG_NORMAL)("Play sysex; address:%02X %02X %02X, length:%4d, delay:%3d", midi.sysex.buf[5], midi.sysex.buf[6], midi.sysex.buf[7], midi.sysex.used, midi.sysex.delay);*/
				PlayMsg(midi.sysex.buf, midi.sysex.used); /* SOFTMPU */
				if (midi.sysex.start) {
                                        if (midi.sysex.usedbufs == 0 && midi.sysex.buf[5] == 0x7F) {
                                            /*midi.sysex.delay = 290;*/ /* SOFTMPU */ // All Parameters reset
                                            MIDI_sysex_delay = 290*(RTCFREQ/1000);
                                        } else if (midi.sysex.usedbufs == 0 && midi.sysex.buf[5] == 0x10 && midi.sysex.buf[6] == 0x00 && midi.sysex.buf[7] == 0x04) {
                                            /*midi.sysex.delay = 145;*/ /* SOFTMPU */ // Viking Child
                                            MIDI_sysex_delay = 145*(RTCFREQ/1000);
                                        } else if (midi.sysex.usedbufs == 0 && midi.sysex.buf[5] == 0x10 && midi.sysex.buf[6] == 0x00 && midi.sysex.buf[7] == 0x01) {
                                            /*midi.sysex.delay = 30;*/ /* SOFTMPU */ // Dark Sun 1
                                            MIDI_sysex_delay = 30*(RTCFREQ/1000);
                                        } else MIDI_sysex_delay = ((((midi.sysex.usedbufs*SYSEX_SIZE)+midi.sysex.used)/2)+2)*(RTCFREQ/1000); /*(Bitu)(((float)(midi.sysex.used) * 1.25f) * 1000.0f / 3125.0f) + 2;
                                        midi.sysex.start = GetTicks();*/ /* SOFTMPU */
				}
			}

			/*LOG(LOG_ALL,LOG_NORMAL)("Sysex message size %d",midi.sysex.used);*/ /* SOFTMPU */
			/*if (CaptureState & CAPTURE_MIDI) {
				CAPTURE_AddMidi( true, midi.sysex.used-1, &midi.sysex.buf[1]);
			}*/ /* SOFTMPU */
		}
	}
	if (data&0x80) {
		midi.status=data;
		midi.cmd_pos=0;
		midi.cmd_len=MIDI_evt_len[data];
		if (midi.status==0xf0) {
			midi.sysex.buf[0]=0xf0;
			midi.sysex.used=1;
                        midi.sysex.usedbufs=0;
		}
	}
	if (midi.cmd_len) {
		midi.cmd_buf[midi.cmd_pos++]=data;
		if (midi.cmd_pos >= midi.cmd_len) {
			/*if (CaptureState & CAPTURE_MIDI) {
				CAPTURE_AddMidi(false, midi.cmd_len, midi.cmd_buf);
                        }*/ /* SOFTMPU */

                        if (midi.fakeallnotesoff)
                        {
                                /* SOFTMPU: Test for "Note On" */
                                if ((midi.status&0xf0)==0x90)
                                {
                                        if (midi.cmd_buf[2]>0)
                                        {
                                                pChan=&tracked_channels[midi.status&0x0f];
                                                pChan->notes[pChan->next++]=midi.cmd_buf[1];
                                                if (pChan->next==MAX_TRACKED_NOTES) pChan->next=0;
                                                if (pChan->used<MAX_TRACKED_NOTES) pChan->used++;
                                        }

                                        PlayMsg(midi.cmd_buf,midi.cmd_len);
                                }
                                /* SOFTMPU: Test for "All Notes Off" */
                                else if (((midi.status&0xf0)==0xb0) &&
                                         (midi.cmd_buf[1]>=0x7b) &&
                                         (midi.cmd_buf[1]<=0x7f))
                                {
                                        FakeAllNotesOff(midi.status&0x0f);
                                }
                                else
                                {
                                        PlayMsg(midi.cmd_buf,midi.cmd_len);
                                }
                        }
                        else
                        {
                                PlayMsg(midi.cmd_buf,midi.cmd_len);
                        }
                        midi.cmd_pos=1;         //Use Running status
		}
	}
}

bool MIDI_Available(void)  {
	return midi.available;
}

/* SOFTMPU: Initialisation */
void MIDI_Init(Bitu mpuport,Bitu sbport,Bitu serialport,OutputMode outputmode,bool delaysysex,bool fakeallnotesoff){
        Bitu i; /* SOFTMPU */
	midi.sysex.delay = 0;
	midi.sysex.start = 0;
	MIDI_sysex_delay = 0; /* SOFTMPU */

        if (delaysysex==true)
	{
		midi.sysex.start = 1; /*GetTicks();*/ /* SOFTMPU */
		/*LOG_MSG("MIDI:Using delayed SysEx processing");*/ /* SOFTMPU */
	}
	midi.mpuport=mpuport;
        midi.sbport=sbport;
        midi.serialport=serialport;
	midi.status=0x00;
	midi.cmd_pos=0;
	midi.cmd_len=0;
        midi.fakeallnotesoff=fakeallnotesoff;
        midi.available=true;
        MIDI_output_mode=outputmode;

        /* SOFTMPU: Display welcome message on MT-32 */
        for (i=0;i<30;i++)
        {
                MIDI_RawOutByte(MIDI_welcome_msg[i]);
        }

        /* SOFTMPU: Init note tracking */
        for (i=0;i<MAX_TRACKED_CHANNELS;i++)
        {
                tracked_channels[i].used=0;
                tracked_channels[i].next=0;
        }

	mfc_init();
	mfc_setbank(5);
}

/* DOSBox initialisation code */
#if 0
class MIDI:public Module_base{
public:
	MIDI(Section* configuration):Module_base(configuration){
		Section_prop * section=static_cast<Section_prop *>(configuration);
		const char * dev=section->Get_string("mididevice");
		std::string fullconf=section->Get_string("midiconfig");
		/* If device = "default" go for first handler that works */
		MidiHandler * handler;
//              MAPPER_AddHandler(MIDI_SaveRawEvent,MK_f8,MMOD1|MMOD2,"caprawmidi","Cap MIDI");
		midi.sysex.delay = 0;
		midi.sysex.start = 0;
		if (fullconf.find("delaysysex") != std::string::npos) {
			midi.sysex.start = GetTicks();
			fullconf.erase(fullconf.find("delaysysex"));
			LOG_MSG("MIDI:Using delayed SysEx processing");
		}
		std::remove(fullconf.begin(), fullconf.end(), ' ');
		const char * conf = fullconf.c_str();
		midi.status=0x00;
		midi.cmd_pos=0;
		midi.cmd_len=0;
		if (!strcasecmp(dev,"default")) goto getdefault;
		handler=handler_list;
		while (handler) {
			if (!strcasecmp(dev,handler->GetName())) {
				if (!handler->Open(conf)) {
					LOG_MSG("MIDI:Can't open device:%s with config:%s.",dev,conf);
					goto getdefault;
				}
				midi.handler=handler;
				midi.available=true;    
				LOG_MSG("MIDI:Opened device:%s",handler->GetName());
				return;
			}
			handler=handler->next;
		}
		LOG_MSG("MIDI:Can't find device:%s, finding default handler.",dev);     
getdefault:     
		handler=handler_list;
		while (handler) {
			if (handler->Open(conf)) {
				midi.available=true;    
				midi.handler=handler;
				LOG_MSG("MIDI:Opened device:%s",handler->GetName());
				return;
			}
			handler=handler->next;
		}
		/* This shouldn't be possible */
	}
	~MIDI(){
		if(midi.available) midi.handler->Close();
		midi.available = false;
		midi.handler = 0;
	}
};


static MIDI* test;
void MIDI_Destroy(Section* /*sec*/){
	delete test;
}
void MIDI_Init(Section * sec) {
	test = new MIDI(sec);
	sec->AddDestroyFunction(&MIDI_Destroy,true);
}
#endif

/* DOSBox MIDI handler code */
#if 0
class MidiHandler;

MidiHandler * handler_list=0;

class MidiHandler {
public:
	MidiHandler() {
		next=handler_list;
		handler_list=this;
	};
	virtual bool Open(const char * /*conf*/) { return true; };
	virtual void Close(void) {};
	virtual void PlayMsg(Bit8u * /*msg*/) {};
	virtual void PlaySysex(Bit8u * /*sysex*/,Bitu /*len*/) {};
	virtual const char * GetName(void) { return "none"; };
	virtual ~MidiHandler() { };
	MidiHandler * next;
};

MidiHandler Midi_none;

/* Include different midi drivers, lowest ones get checked first for default */

#if defined(MACOSX)

#include "midi_coremidi.h"
#include "midi_coreaudio.h"

#elif defined (WIN32)

#include "midi_win32.h"

#else

#include "midi_oss.h"

#endif

#if defined (HAVE_ALSA)

#include "midi_alsa.h"

#endif
#endif /* if 0 */
