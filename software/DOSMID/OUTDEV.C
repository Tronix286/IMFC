/*
 * Wrapper for outputing MIDI commands to different devices.
 *
 * Copyright (C) 2014-2020, Mateusz Viste
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <conio.h>  /* outp(), inp() */
#include <dos.h>    /* _disable(), _enable() */
#include <malloc.h> /* _fmalloc(), _ffree() */

#ifdef OPL
#include "opl.h"
#endif

#include "fio.h"
#include "gus.h"
#include "mpu401.h"
#include "rs232.h"
#include "sbdsp.h"

extern unsigned char mfc_bank;

#ifdef SBAWE
#include "awe32/ctaweapi.h"
static char far *presetbuf = NULL; /* used to allocate presets for custom sound banks */
#endif

#include "mfc.h"
#include "outdev.h" /* include self for control */

/* force the compiler to load valid DS segment value before calling
 * the AWE32 API functions (in far data models, where DS is floating) */
#pragma aux __pascal "^" parm loadds reverse routine [] \
                         value struct float struct caller [] \
                         modify [ax bx cx dx es];


static enum outdev_types outdev = DEV_NONE;
static unsigned short outport = 0;

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
{5, 47},//65.	Soprano Sax		-> 5/47:Sax 1
{5, 48},//66.	Alto Sax		-> 5/48:Sax 2
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
{41, 60}, 	// 35 Acoustic Bass Drum
{42, 67},       // 36 Bass Drum 1
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
{47, 72},	// 51 Ride Cymbal 1
{46, 79},	// 52 Chinese Cymbal
{36, 103},	// 53 Ride Bell
{46, 103},	// 54 Tambourine
{46, 72},	// 55 Splash Cymbal
{39, 103},	// 56 Cowbell
{43, 72},	// 57 Crash Cymbal 2
{39, 72},	// 58 Vibraslap
{45, 108},	// 59 Ride Cymbal 2
{41, 67},	// 60 Hi Bongo
{41, 72},	// 61 Low Bongo
{41, 84},	// 62 Mute Hi Conga
{41, 79},	// 63 Open Hi Conga
{41, 72},	// 64 Low Conga
{45, 79},	// 65 High Timbale
{45, 84},	// 66 Low Timbale
{35, 91},	// 67 High Agogo
{35, 84},	// 68 Low Agogo
{46, 108},	// 69 Cabasa
{46, 103},	// 70 Maracas
{32, 108},	// 71 Short Whistle
{33, 103},	// 72 Long Whistle
{47, 91},	// 73 Short Guiro
{47, 84},	// 74 Long Guiro
{47, 108},	// 75 Claves
{32, 91},	// 76 Hi Wood Block
{32, 84},	// 77 Low Wood Block
{46, 108},	// 78 Mute Cuica
{46, 103},	// 79 Open Cuica
{34, 96},	// 80 Mute Triangle
{35, 96}	// 81 Open Triangle
};

/* loads a SBK sound font to AWE hardware */
#ifdef SBAWE
static int awe_loadfont(char *filename) {
  struct fiofile_t f;
  SOUND_PACKET sp;
  long banks[1];
  int i;
  char buffer[PACKETSIZE];
  awe32TotalPatchRam(&sp); /* get available patch DRAM */
  if (sp.total_patch_ram < 512*1024) return(-1);
  /* Setup bank sizes with all available RAM */
  banks[0] = sp.total_patch_ram;
  sp.banksizes = banks;
  sp.total_banks = 1; /* total number of banks */
  if (awe32DefineBankSizes(&sp) != 0) return(-1);
  /* Open SoundFont Bank */
  if (fio_open(filename, FIO_OPEN_RD, &f) != 0) return(-1);
  fio_read(&f, buffer, PACKETSIZE); /* read sf header */
  /* prepare stuff */
  sp.bank_no = 0;
  sp.data = buffer;
  if (awe32SFontLoadRequest(&sp) != 0) {
    fio_close(&f);
    return(-1); /* invalid soundfont file */
  }
  /* stream sound samples into the hardware */
  if (sp.no_sample_packets > 0) {
    fio_seek(&f, FIO_SEEK_START, sp.sample_seek); /* move pointer to where instruments begin */
    for (i = 0; i < sp.no_sample_packets; i++) {
      if ((fio_read(&f, sp.data, PACKETSIZE) != PACKETSIZE) || (awe32StreamSample(&sp))) {
        fio_close(&f);
        return(-1);
      }
    }
  }
  /* load presets to memory */
  presetbuf = _fmalloc(sp.preset_read_size);
  if (presetbuf == NULL) { /* out of mem! */
    fio_close(&f);
    return(-1);
  }
  sp.presets = presetbuf;
  fio_seek(&f, FIO_SEEK_START, sp.preset_seek);
  fio_read(&f, sp.presets, sp.preset_read_size);
  /* close the sf file */
  fio_close(&f);
  /* apply presets to hardware */
  if (awe32SetPresets(&sp) != 0) {
    _ffree(presetbuf);
    presetbuf = NULL;
    return(-1);
  }
  return(0);
}
#endif


/* inits the out device, also selects the out device, from one of these:
 *  DEV_MPU401
 *  DEV_AWE
 *  DEV_OPL
 *  DEV_RS232
 *  DEV_SBMIDI
 *  DEV_GUS
 *  DEV_NONE
 *
 * This should be called only ONCE, when program starts.
 * Returns NULL on success, or a pointer to an error string otherwise. */
char *dev_init(enum outdev_types dev, unsigned short port, char *sbank) {
  outdev = dev;
  outport = port;
  switch (outdev) {
    case DEV_MPU401:
      /* reset the MPU401 */
      if (mpu401_rst(outport) != 0) return("MPU doesn't answer");
      /* put it into UART mode */
      mpu401_uart(outport);
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32NumG = 30; /* global var used by the AWE lib, must be set to 30 for
                         DRAM sound fonts to work properly */
      if (awe32Detect(outport) != 0) return("No EMU8000 chip detected");
      if (awe32InitHardware() != 0) return("EMU8000 initialization failed");
      /* preload GM samples from AWE's ROM */
      if (sbank == NULL) {
        awe32SoundPad.SPad1 = awe32SPad1Obj;
        awe32SoundPad.SPad2 = awe32SPad2Obj;
        awe32SoundPad.SPad3 = awe32SPad3Obj;
        awe32SoundPad.SPad4 = awe32SPad4Obj;
        awe32SoundPad.SPad5 = awe32SPad5Obj;
        awe32SoundPad.SPad6 = awe32SPad6Obj;
        awe32SoundPad.SPad7 = awe32SPad7Obj;
      } else if (awe_loadfont(sbank) != 0) {
        dev_close();
        return("Sound bank could not be loaded");
      }
      if (awe32InitMIDI() != 0) {
        dev_close();
        return("EMU8000 MIDI processor initialization failed");
      }
#endif
      break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
    {
      int res;
      res = opl_init(outport);
      if (res < 0) return("No OPL2/OPL3 device detected");
      /* change the outdev device depending on OPL autodetection */
      if (res == 0) {
        outdev = DEV_OPL2;
      } else {
        outdev = DEV_OPL3;
      }
      /* load a custom sound bank, if any provided */
      if (sbank != NULL) {
        if (opl_loadbank(sbank) != 0) {
          dev_close();
          return("OPL sound bank could not be loaded");
        }
      }
    }
#endif
      break;
    case DEV_RS232:
      if (rs232_check(outport) != 0) return("RS232 failure");
      break;
    case DEV_SBMIDI:
      /* The DSP has to be reset before it is first programmed. The reset
       * causes it to perform an initialization and returns it to its default
       * state. The DSP reset is done through the Reset port. */
      if (dsp_reset(outport) != 0) return("SB DSP initialization failure");
      dsp_write(outport, 0x30); /* switch the MIDI I/O into polling mode */
      break;
    case DEV_GUS:
      gus_open(port);
      break;
    case DEV_NONE:
      break;
    case DEV_MFC:
	mfc_init();
	mfc_setbank(mfc_bank);
      break;

  }
  dev_clear();
  return(NULL);
}


/* pre-load a patch (so far needed only for GUS) */
void dev_preloadpatch(enum outdev_types dev, int p) {
  switch (dev) {
    case DEV_MPU401:
    case DEV_AWE:
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
    case DEV_RS232:
    case DEV_SBMIDI:
    case DEV_MFC:
      break;
    case DEV_GUS:
      gus_loadpatch(p);
      break;
    case DEV_NONE:
      break;
  }
}


/* returns the device that has been inited/selected */
enum outdev_types dev_getcurdev(void) {
  return(outdev);
}


/* close/deinitializes the out device */
void dev_close(void) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_rst(outport); /* resets it to intelligent mode */
      break;
    case DEV_AWE:
#ifdef SBAWE
      /* Creative recommends to disable interrupts during AWE shutdown */
      _disable();
      awe32Terminate();
      _enable();
      /* free memory used by custom sound banks */
      if (presetbuf != NULL) {
        _ffree(presetbuf);
        presetbuf = NULL;
      }
#endif
      break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      opl_close(outport);
#endif
      break;
    case DEV_RS232:
      break;
    case DEV_SBMIDI:
      /* To terminate UART mode, send a DSP reset command. The reset command
       * behaves differently while the DSP is in MIDI UART mode. It terminates
       * MIDI UART mode and restores all the DSP parameters to the states
       * prior to entering MIDI UART mode. If your application was run in MIDI
       * UART mode, it important that you send the DSP reset command to exit
       * the MIDI UART mode when your application terminates. */
      dsp_reset(outport); /* I don't use MIDI UART mode because it requires */
                          /* DSP v2.x, but reseting the chip seems like a   */
      break;              /* good thing to do anyway                        */
    case DEV_GUS:
      gus_close();
      break;
    case DEV_MFC:
	mfc_init();
      break;
    case DEV_NONE:
      break;
  }
}


/* clears the out device (turns all sounds off...). this can be used
 * often (typically: after each song) */
void dev_clear(void) {
  int i;
  /* iterate on MIDI channels and send 'off' messages */
  for (i = 0; i < 16; i++) {
    dev_controller(i, 123, 0);   /* "all notes off" */
    dev_controller(i, 120, 0);   /* "all sounds off" */
    dev_controller(i, 121, 0);   /* "all controllers off" */
  }
  /* execute hardware-specific actions */
  switch (outdev) {
    case DEV_MPU401:
    case DEV_AWE:
    case DEV_RS232:
    case DEV_SBMIDI:
    case DEV_MFC:
      break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      opl_clear(outport);
#endif
      break;
    case DEV_GUS:
      gus_allnotesoff();
      gus_unloadpatches();
      break;
/*    case DEV_MFC:
	mfc_init();
      break;
*/
    case DEV_NONE:
      break;
  }
}


/* activate note on channel */
void dev_noteon(int channel, int note, int velocity) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, 0x90 | channel);  /* Send note ON to selected channel */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, note);            /* Send note number to turn ON */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, velocity);        /* Send velocity */
      break;
    case DEV_MFC:
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

    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      opl_midi_noteon(outport, channel, note, velocity);
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32NoteOn(channel, note, velocity);
#endif
      break;
    case DEV_RS232:
      rs232_write(outport, 0x90 | channel); /* Send note ON to selected channel */
      rs232_write(outport, note);           /* Send note number to turn ON */
      rs232_write(outport, velocity);       /* Send velocity */
      break;
    case DEV_SBMIDI:
      dsp_write(outport, 0x38);             /* MIDI output */
      dsp_write(outport, 0x90 | channel);   /* Send note ON to selected channel */
      dsp_write(outport, 0x38);             /* MIDI output */
      dsp_write(outport, note);             /* Send note number to turn ON */
      dsp_write(outport, 0x38);             /* MIDI output */
      dsp_write(outport, velocity);         /* Send velocity */
      break;
    case DEV_GUS:
      gus_write(0x90 | channel);            /* Send note ON to selected channel */
      gus_write(note);                      /* Send note number to turn ON */
      gus_write(velocity);                  /* Send velocity */
      break;
    case DEV_NONE:
      break;
  }
}


/* disable note on channel */
void dev_noteoff(int channel, int note) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, 0x80 | channel);  /* Send note OFF code on selected channel */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, note);            /* Send note number to turn OFF */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, 64);              /* Send velocity */
      break;
    case DEV_MFC:
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
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      opl_midi_noteoff(outport, channel, note);
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32NoteOff(channel, note, 64);
#endif
      break;
    case DEV_RS232:
      rs232_write(outport, 0x80 | channel); /* 'note off' + channel selector */
      rs232_write(outport, note);           /* note number */
      rs232_write(outport, 64);             /* velocity */
      break;
    case DEV_SBMIDI:
      dsp_write(outport, 0x38);           /* MIDI output */
      dsp_write(outport, 0x80 | channel); /* Send note OFF code on selected channel */
      dsp_write(outport, 0x38);           /* MIDI output */
      dsp_write(outport, note);           /* Send note number to turn OFF */
      dsp_write(outport, 0x38);           /* MIDI output */
      dsp_write(outport, 64);             /* Send velocity */
      break;
    case DEV_GUS:
      gus_write(0x80 | channel);   /* 'note off' + channel selector */
      gus_write(note);             /* note number */
      gus_write(64);               /* velocity */
      break;
    case DEV_NONE:
      break;
  }
}


/* adjust the pitch wheel of a channel */
void dev_pitchwheel(int channel, int wheelvalue) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, 0xE0 | channel);  /* Send selected channel */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, wheelvalue & 127);/* Send the lowest (least significant) 7 bits of the wheel value */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, wheelvalue >> 7); /* Send the highest (most significant) 7 bits of the wheel value */
      break;
    case DEV_MFC:
    	mfc_putbyte(0xE0 | channel);
    	mfc_putbyte(wheelvalue & 127);
    	mfc_putbyte(wheelvalue >> 7);
      	break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      opl_midi_pitchwheel(outport, channel, wheelvalue);
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32PitchBend(channel, wheelvalue & 127, wheelvalue >> 7);
#endif
      break;
    case DEV_RS232:
      rs232_write(outport, 0xE0 | channel);   /* Send selected channel */
      rs232_write(outport, wheelvalue & 127); /* Send the lowest (least significant) 7 bits of the wheel value */
      rs232_write(outport, wheelvalue >> 7);  /* Send the highest (most significant) 7 bits of the wheel value */
      break;
    case DEV_SBMIDI:
      dsp_write(outport, 0x38);             /* MIDI output */
      dsp_write(outport, 0xE0 | channel);   /* Send selected channel */
      dsp_write(outport, 0x38);             /* MIDI output */
      dsp_write(outport, wheelvalue & 127); /* Send the lowest (least significant) 7 bits of the wheel value */
      dsp_write(outport, 0x38);             /* MIDI output */
      dsp_write(outport, wheelvalue >> 7);  /* Send the highest (most significant) 7 bits of the wheel value */
      break;
    case DEV_GUS:
      gus_write(0xE0 | channel);   /* Send selected channel */
      gus_write(wheelvalue & 127); /* Send the lowest (least significant) 7 bits of the wheel value */
      gus_write(wheelvalue >> 7);  /* Send the highest (most significant) 7 bits of the wheel value */
      break;
    case DEV_NONE:
      break;
  }
}


/* send a 'controller' message */
void dev_controller(int channel, int id, int val) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, 0xB0 | channel);  /* Send selected channel */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, id);              /* Send the controller's id */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, val);             /* Send controller's value */
      break;
    case DEV_MFC:
    	mfc_putbyte(0xB0 | channel);
    	mfc_putbyte(id);
    	mfc_putbyte(val);
      	break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      opl_midi_controller(outport, channel, id, val);
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32Controller(channel, id, val);
#endif
      break;
    case DEV_RS232:
      rs232_write(outport, 0xB0 | channel);  /* Send selected channel */
      rs232_write(outport, id);              /* Send the controller's id */
      rs232_write(outport, val);             /* Send controller's value */
      break;
    case DEV_SBMIDI:
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, 0xB0 | channel);  /* Send selected channel */
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, id);              /* Send the controller's id */
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, val);             /* Send controller's value */
      break;
    case DEV_GUS:
      gus_write(0xB0 | channel);           /* Send selected channel */
      gus_write(id);                       /* Send the controller's id */
      gus_write(val);                      /* Send controller's value */
      break;
    case DEV_NONE:
      break;
  }
}


void dev_chanpressure(int channel, int pressure) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, 0xD0 | channel);  /* Send selected channel */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, pressure);        /* Send the pressure value */
      break;
    case DEV_MFC:
    	mfc_putbyte(0xD0 | channel);
    	mfc_putbyte(pressure);
      	break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      /* nothing to do */
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32ChannelPressure(channel, pressure);
#endif
      break;
    case DEV_RS232:
      rs232_write(outport, 0xD0 | channel);  /* Send selected channel */
      rs232_write(outport, pressure);        /* Send the pressure value */
      break;
    case DEV_SBMIDI:
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, 0xD0 | channel);  /* Send selected channel */
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, pressure);        /* Send the pressure value */
      break;
    case DEV_GUS:
      gus_write(0xD0 | channel);           /* Send selected channel */
      gus_write(pressure);                 /* Send the pressure value */
      break;
    case DEV_NONE:
      break;
  }
}


void dev_keypressure(int channel, int note, int pressure) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, 0xA0 | channel);  /* Send selected channel */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, note);            /* Send the note we target */
      mpu401_waitwrite(outport);      /* Wait for port ready */
      outp(outport, pressure);        /* Send the pressure value */
      break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      /* nothing to do */
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32PolyKeyPressure(channel, note, pressure);
#endif
      break;
    case DEV_RS232:
      rs232_write(outport, 0xA0 | channel);  /* Send selected channel */
      rs232_write(outport, note);            /* Send the note we target */
      rs232_write(outport, pressure);        /* Send the pressure value */
      break;
    case DEV_SBMIDI:
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, 0xA0 | channel);  /* Send selected channel */
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, note);            /* Send the note we target */
      dsp_write(outport, 0x38);            /* MIDI output */
      dsp_write(outport, pressure);        /* Send the pressure value */
      break;
    case DEV_GUS:
      gus_write(0xA0 | channel);           /* Send selected channel */
      gus_write(note);                     /* Send the note we target */
      gus_write(pressure);                 /* Send the pressure value */
      break;
    case DEV_NONE:
      break;
  }
}


/* should be called by the application from time to time */
void dev_tick(void) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_flush(outport);
      break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
      break;
    case DEV_AWE:
      break;
    case DEV_RS232:
      /* I do nothing here - although flushing any incoming bytes would seem
       * to be the 'sane thing to do', it can lead sometimes to freezes on
       * systems where the RS232 UART always reports a 'read ready' status.
       * NOT flushing the UART, on the other hand, doesn't seem to affect
       * anything. */
      /* while (rs232_read(outport) >= 0); */
      break;
    case DEV_SBMIDI:
      break;
    case DEV_GUS:
      break;
    case DEV_NONE:
      break;
  }
}


/* sets a "program" (meaning an instrument) on a channel */
void dev_setprog(int channel, int program) {
  switch (outdev) {
    case DEV_MPU401:
      mpu401_waitwrite(outport);     /* Wait for port ready */
      outp(outport, 0xC0 | channel); /* Send channel */
      mpu401_waitwrite(outport);     /* Wait for port ready */
      outp(outport, program);        /* Send patch id */
      break;
    case DEV_MFC:
	if (channel != 9) {	// dont touch ch9, its bank 5 percussion by default
	  mfc_setchanbank(channel,GM_to_mfc[program].bank-1);
    	  mfc_putbyte(0xC0 | channel);
    	  mfc_putbyte(GM_to_mfc[program].instr-1);
	}
      	break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      opl_midi_changeprog(channel, program);
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32ProgramChange(channel, program);
#endif
      break;
    case DEV_RS232:
      rs232_write(outport, 0xC0 | channel); /* Send channel */
      rs232_write(outport, program);        /* Send patch id */
      break;
    case DEV_SBMIDI:
      dsp_write(outport, 0x38);           /* MIDI output */
      dsp_write(outport, 0xC0 | channel); /* Send channel */
      dsp_write(outport, 0x38);           /* MIDI output */
      dsp_write(outport, program);        /* Send patch id */
      break;
    case DEV_GUS:
      /* NOTE I might (?) want to call gus_loadpatch() here */
      /*if (channel == 9) program |= 128;
      gus_loadpatch(program);*/
      gus_write(0xC0 | channel);          /* Send channel */
      gus_write(program);                 /* Send patch id */
      break;
    case DEV_NONE:
      break;
  }
}


/* sends a raw sysex string to the device */
void dev_sysex(int channel, unsigned char *buff, int bufflen) {
  int x;
  switch (outdev) {
    case DEV_MPU401:
      for (x = 0; x < bufflen; x++) {
        mpu401_waitwrite(outport);     /* Wait for port ready */
        outp(outport, buff[x]);        /* Send sysex data byte */
      }
      break;
    case DEV_MFC:
      for (x = 0; x < bufflen; x++) {
    	mfc_putbyte(buff[x]);
        //outp(outport, buff[x]);        /* Send sysex data byte */
      }
      break;
    case DEV_OPL:
    case DEV_OPL2:
    case DEV_OPL3:
#ifdef OPL
      /* SYSEX is unsupported on OPL output */
#endif
      break;
    case DEV_AWE:
#ifdef SBAWE
      awe32Sysex(channel, (unsigned char far *)buff, bufflen);
#endif
      break;
    case DEV_RS232:
      for (x = 0; x < bufflen; x++) {
        rs232_write(outport, buff[x]);      /* Send sysex data byte */
      }
      break;
    case DEV_SBMIDI:
      for (x = 0; x < bufflen; x++) {
        dsp_write(outport, 0x38);           /* MIDI output */
        dsp_write(outport, buff[x]);        /* Send sysex data byte */
      }
      break;
    case DEV_GUS:
      for (x = 0; x < bufflen; x++) {
        gus_write(buff[x]);                 /* Send sysex data byte */
      }
      break;
    case DEV_NONE:
      break;
  }
}
