/*
 * SparkFun MP3Shield Audio Library: SFAudioShield.h
 * Portions Copyright 2012 Michael Schwager
 *
 * Distributed under the following license:
 * This program is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or(at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.  <http://www.gnu.org/licenses/>
 */
/* 
 * Much code taken from: http://mbed.org/users/christi_s/code/VS1053b/
 * mbed VLSI VS1053b library
 *
 * The following notice accompanies this code, due to the large contribution of
 * Christian Schmiljun (who in turn borrowed from Shigeki KOMATSU, see
 * http://mbed.org/users/xshige/code/mbeduino_MP3_Shield_MP3Player/ )
 * "If I have seen further it is by standing on ye sholders of Giants." -Newton
 *
 * Copyright (c) 2010 Christian Schmiljun
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* This code based on:
 *  mbeduino_MP3_Shield_MP3Player
 *  http://mbed.org/users/xshige/programs/mbeduino_MP3_Shield_MP3Player/lgcx63
 *  2010-10-16
 */

/*
 * VS1053 clock: 12.288 MHz on the shield frequency
 * 1 cycle: 81.3 nanos == .0813 micros
 * VS1053 SPI bus: 2 CLKI cycles high, 2 CLKI cycles low minimum == .3252 micros total
 * (4 * .08138)
 * == 3.072 MHz max.
 */

#ifndef SFAUDIO_H
#define SFAUDIO_H

#define MYLED A2 // MIKE: temporary use only!

// ----------------------------------------------------------------------------
// Extended settings
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
//   Enable debug output
//   --------------------------------------------------------------------------
#undef DEEPDEBUG
#undef DEBUG
//   --------------------------------------------------------------------------

#ifdef DEEPDEBUG
#undef DEBUG
#endif
#ifdef DEBUG
#define DEBUGOUT(x)  Serial.print(F(x)); Serial.flush()
#define DEBUGOUT2(x,y)  Serial.print(x,y);
#define DEBUGNL  Serial.println("");
#else
#define DEBUGOUT(x)  ((void)0)
#define DEBUGOUT2(x,y)  ((void)0)
#define DEBUGNL  ((void)0)
#endif

// include the SPI library:
#include "SPI.h"

// Not supported, but just in case. 
#if ARDUINO > 22
#include "Arduino.h"
#else
#error "Older Arduino environment is not supported"
#include "WProgram.h"
#endif

#include "digitalWriteFast.h"
//Add the SdFat Libraries
#include <SdFat.h>
#include <SdFatUtil.h> 

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif 

#ifndef cli
# define cli()  __asm__ __volatile__ ("cli" ::)
#endif
#ifndef sei
# define sei()  __asm__ __volatile__ ("sei" ::)
#endif
#ifndef nop
#define nop asm volatile ("nop\n\t")
#endif

// SD CARD Stuff **********************************************************
//Create the variables to be used by SdFat Library
static Sd2Card card;
static SdVolume driveVolume;
static SdFile root;
static SdFile track;

// VS1053 Stuff *******************************************************
#define POINT5 1
#define DEFAULT_VOLUME                             0x14 // == 20 == -10 dB
//#define DEFAULT_VOLUME                             0x0A // == 10 == -5 dB
#define DEFAULT_BASS_AMPLITUDE                        0 //   0 -    15 dB
#define DEFAULT_BASS_FREQUENCY                      100 //  20 -   150 Hz
#define DEFAULT_TREBLE_AMPLITUDE                      0 //  -8 -     7 dB
#define DEFAULT_TREBLE_FREQUENCY                     15 //1 - 15 * 1000 Hz

// SCI register address assignment
#define SCI_MODE                                    0x00
#define SCI_STATUS                                  0x01
#define SCI_BASS                                    0x02
#define SCI_CLOCKF                                  0x03
#define SCI_DECODE_TIME                             0x04
#define SCI_AUDATA                                  0x05
#define SCI_WRAM                                    0x06
#define SCI_WRAMADDR                                0x07
#define SCI_HDAT0                                   0x08
#define SCI_HDAT1                                   0x09
#define SCI_AIADDR                                  0x0A
#define SCI_VOL                                     0x0B
#define SCI_AICTRL0                                 0x0C
#define SCI_AICTRL1                                 0x0D
#define SCI_AICTRL2                                 0x0E
#define SCI_AICTRL3                                 0x0F

//SCI_MODE register bits as of p.38 of the datasheet
#define SM_DIFF                                     0x0001
#define SM_LAYER12                                  0x0002
#define SM_RESET                                    0x0004
#define SM_CANCEL                                   0x0008
#define SM_EARSPEAKER_LO                            0x0010
#define SM_TESTS                                    0x0020
#define SM_STREAM                                   0x0040
#define SM_EARSPEAKER_HI                            0x0080
#define SM_DACT                                     0x0100
#define SM_SDIORD                                   0x0200
#define SM_SDISHARE                                 0x0400
#define SM_SDINEW                                   0x0800
#define SM_ADPCM                                    0x1000
#define SM_B13                                      0x2000
#define SM_LINE1                                    0x4000
#define SM_CLK_RANGE                                0x8000

//SCI_CLOCKF register bits as of p.42 of the datasheet
#define SC_ADD_NOMOD                                0x0000
#define SC_ADD_10x                                  0x0800
#define SC_ADD_15x                                  0x1000
#define SC_ADD_20x                                  0x1800
#define SC_MULT_XTALI                               0x0000
#define SC_MULT_XTALIx20                            0x2000
#define SC_MULT_XTALIx25                            0x4000
#define SC_MULT_XTALIx30                            0x6000
#define SC_MULT_XTALIx35                            0x8000
#define SC_MULT_XTALIx40                            0xA000
#define SC_MULT_XTALIx45                            0xC000
#define SC_MULT_XTALIx50                            0xE000

// Extra Parameter in X memory (refer to p.58 of the datasheet)
#define para_chipID_0                               0x1E00
#define para_chipID_1                               0x1E01
#define para_version                                0x1E02
#define para_config1                                0x1E03
#define para_playSpeed                              0x1E04
#define para_byteRate                               0x1E05
// endFillByte is updated when decoding of a format starts or ends, i.e. when
// HDAT1 changes. Normally the value is 0, for some wav formats and FLAC the
// value will be different.
//
// The trick is to know when the format has changed. It is usually safe to
// assume one file contains only one audio format, so you only need to read
// endFillByte once when you see HDAT1 being non-zero. -From an email @support
#define para_endFillByte                            0x1E06
#define para_positionMsec_0                         0x1E27
#define para_positionMsec_1                         0x1E28
#define para_resync                                 0x1E29
   
//attach refill interrupt off DREQ line, pin 2
//If DREQ is high, VS1053b can take at least 32 bytes of SDI data or one SCI command.
//DREQ is turned low when the stream buffer is too full and for the duration of a SCI command.
#define INTERRUPT_HANDLER_ENABLE                   attachInterrupt(0, loadVS1053FIFO, RISING);
#define INTERRUPT_HANDLER_DISABLE                   detachInterrupt(0);

// This should remain 32; this number is tied to DREQ interrupts. See datasheet.
#define VS_BUFFER_SIZE 32

#define ENDING_START INTERRUPT_HANDLER_DISABLE sei();
// set SCI MODE bit SM CANCEL    
#define ENDING_SET_SM_CANCEL   sciModeWord = sci_read(SCI_MODE); \
    sciModeWord |= SM_CANCEL; \
    sci_write(SCI_MODE, sciModeWord); \
	delayMicroseconds(7); // wait 80 CLKI cycles, or 80/12288000 seconds (see the datasheet)
#define SEND_2048_ENDFILLBYTE readEndFillByte(); \
	for (uint8_t j = 0; j < VS_BUFFER_SIZE; j++) audioDataBuffer[j]=vs1053EndFillByte; \
    for (uint8_t i = 0; i < 64; i++) { \
		while (!_DREQ); \
		sdi_write_data(VS_BUFFER_SIZE, false); \
	}

//MP3 Player Shield pin mapping. See the schematic
// was MP3_XCS
#define MP3_CS 6 //Control Chip Select Pin (for accessing SPI Control/Status registers)
#define MP3_DCS 7 //Data Chip Select / BSYNC Pin. This is for selecting the chip for SDI transfers
#define MP3_DREQ 2 //Data Request Pin: Player asks for more data
#define MP3_RST 8 //Reset is active low
#define SD_CS 9 //select pin for SD card

#define _CS(value) digitalWriteFast2(MP3_CS, value)
#define _DCS(value) digitalWriteFast2(MP3_DCS, value)
#define _RST(value) digitalWriteFast2(MP3_RST, value)
#define _DREQ digitalReadFast2(MP3_DREQ)



/** Types of audio streams
 *
 */
enum AudioType 
{
    WAV,                        /*!< WAVE audio stream */
    AAC,                        /*!< AAC audio stream (ADTS (.aac), MPEG2 ADIF (.aac) and MPEG4 AUDIO (.mp4 / .m4a / .3gp / .3g2)) */
    WMA,                        /*!< Windows Media Audio (WMA) stream */
    MIDI,                       /*!< Midi audio stream */    
    OGG_VORBIS,                 /*!< Ogg Vorbis audio stream */
    MP3,                        /*!< MPEG Audio Layer */
    UNKNOWN                     /*!< Unknown */
};

typedef enum AudioType AudioType;

/** Types of MPEG Audio Layer stream IDs.
 *
 */
enum MP3_ID
{
    MPG2_5a  = 0,               /*!< MPG 2.5, nonstandard, proprietary */
    MPG2_5b  = 1,               /*!< MPG 2.5, nonstandard, proprietary */
    MPG2_0   = 2,               /*!< ISO 13818-3 MPG 2.0 */
    MPG1_0   = 3                /*!< ISO 11172-3 MPG 1.0 */
};

typedef enum MP3_ID MP3_ID;

/** Types of MPEG Audio Layer channel modes.
 *
 */
enum MP3_MODE
{
    STEREO       = 0,           /*!< Stereo */
    JOINT_STEREO = 1,           /*!< Joint Stereo */
    DUAL_CHANNEL = 2,           /*!< Dual Channel */
    MONO         = 3            /*!< Mono */
};

typedef enum MP3_MODE MP3_MODE;

/** Struct for information about audio streams. This is gained by query'ing the
 * VS1053. That method has been completely untested and is not known to work. -Mike 11/3/2012
 */
typedef struct AudioInfo
{
   AudioType            type            : 4;        /*!< Type of the audio stream - important for the interpretation of the lower union */
   uint16_t kBitRate;                   			/*!< Average bitrate of the audio stream - in kBit/s */
   uint16_t decodeTime;                 			/*!< Decode time */
   union {
        struct {
            MP3_ID      id              : 2;        /*!< ID */
            char        layer           : 2;        /*!< Layer */
            char        protectBit      : 1;        /*!< Protect bit, see p.44 of the datasheet */
            char        padBit          : 1;        /*!< Pad bit, see p.44 of the datasheet */
            MP3_MODE    mode            : 2;        /*!< Channel mode */
            char        extension       : 2;        /*!< Extension, see p.44 of the datasheet */
            char        copyright       : 1;        /*!< Copyright, see p.44 of the datasheet */
            char        original        : 1;        /*!< Original, see p.44 of the datasheet */
            char        emphasis        : 2;        /*!< Emphasis, see p.44 of the datasheet */
            char        kSampleRate     : 6;        /*!< Samplerate - in kHz (rounded) */
        } mp3;                                      /*!< MPEG Audio Layer */
        struct {
        	// empty
        } wma;                                      /*!< Windows Media Audio (WMA) stream */
        struct {
        	// empty
        } aac;                                      /*!< AAC audio stream */
        struct {
        	// empty
        } other;                                    /*!< Other */
   } ext;
   
} AudioInfo;

typedef struct fieldNames {
uint8_t entries;
uint8_t length;
char *name[];
};

#define DEFAULT_INTERRUPT_STATE true
/** Class for VS1053 - Ogg Vorbis / MP3 / AAC / WMA / FLAC / MIDI Audio Codec Chip.
 *  Datasheet, see http://www.vlsi.fi/fileadmin/datasheets/vlsi/vs1053.pdf
 *
 * This code based on:
 *  mbeduino_MP3_Shield_MP3Player
 *  http://mbed.org/users/xshige/programs/mbeduino_MP3_Shield_MP3Player/lgcx63
 *  2010-10-16
 *
 * For a complete sample, see http://mbed.org/users/christi_s/programs/Lib_VS1053b
 *
 */
class SFAudioShield  {

public:
    /** Create a SFAudioShield object.
     *
     * @param mosi 
     *   SPI Master Out, Slave In pin to vs1053b.
     * @param miso 
     *   SPI Master In, Slave Out pin to vs1053b.
     * @param sck  
     *   SPI Clock pin to vs1053b.
     * @param cs   
     *   Pin to vs1053b control chip select.
     * @param rst  
     *   Pin to vs1053b reset.
     * @param dreq 
     *   Pin to vs1053b data request.
     * @param dcs  
     *   Pin to vs1053b data chip select.
     * @param buffer  
     *   Array to cache audio data.
     * @param buffer_size  
     *   Length of the array.
     */
/* ==================================================================
 * Constructor
 * =================================================================*/
	SFAudioShield(bool via_interrupt) : _via_interrupt(via_interrupt),
        _continuous(true),
		_volume(DEFAULT_VOLUME),
		_balance(0),
        _sb_amplitude(DEFAULT_BASS_AMPLITUDE),
        _sb_freqlimit(DEFAULT_BASS_FREQUENCY),
        _st_amplitude(DEFAULT_TREBLE_AMPLITUDE),
        _st_freqlimit(DEFAULT_TREBLE_FREQUENCY) {
	INTERRUPT_HANDLER_DISABLE
    };
	SFAudioShield() : _via_interrupt(DEFAULT_INTERRUPT_STATE),
        _continuous(true),
		_volume(DEFAULT_VOLUME),
		_balance(0),
        _sb_amplitude(DEFAULT_BASS_AMPLITUDE),
        _sb_freqlimit(DEFAULT_BASS_FREQUENCY),
        _st_amplitude(DEFAULT_TREBLE_AMPLITUDE),
        _st_freqlimit(DEFAULT_TREBLE_FREQUENCY) {
	INTERRUPT_HANDLER_DISABLE
    };
	SFAudioShield(bool via_interrupt, bool load_continuously) : _via_interrupt(via_interrupt),
        _continuous(load_continuously),
		_volume(DEFAULT_VOLUME),
		_balance(0),
        _sb_amplitude(DEFAULT_BASS_AMPLITUDE),
        _sb_freqlimit(DEFAULT_BASS_FREQUENCY),
        _st_amplitude(DEFAULT_TREBLE_AMPLITUDE),
        _st_freqlimit(DEFAULT_TREBLE_FREQUENCY) {
	INTERRUPT_HANDLER_DISABLE
    };

    /** Initialize the SFAudioShield object, and the hardware.
     * This initializes the SD card, opens the SD card volume, and 
     * initializes the VS1053.
     * @return
     *  0 on failure to read SCI_STATUS from vs1053
     *  1 on successful init
     *  2 on failure to initalize SD card
	 *  3 on failure to open volume
	 *  4 on failure to open the root directory
     */
	uint8_t init(void);

    /** Initialize the vs1053b device.
     *
     * @return 
     *    0 on failure to read SCI_STATUS from vs1053
	 *	  1 on successful init
     */
    uint8_t  initVS1053(void);

    /** Software reset vs1053b
     * Performs a software reset of the device as per the datasheet.
     * @return none
     */
    void sResetVS1053(void);
    
    /** Set the volume.
     * 
     * @param volume 
     * volume is from 0 (highest) to 0xFE (254), or 127 dB down (aka, "Total silence").
 	 * Volume is entered as the number of db. If you want to do 1/2 db, give it the "POINT5"
	 * argument.
     *   Volume: -0.0dB, -1.0dB, -2.0dB .. -127.0dB. 
     *   == 0, 2, 4, ... 254
     * @param point5
     *   == 0 or 1
     *   If 1, then add -0.5dB to the volume value. Why such a weird argument? you may ask.
	 *   This avoids floating-point operations, which are time consuming for such a little
	 *   champ as the ATmega328.
     */    
    void  setVolume(uint8_t volume = DEFAULT_VOLUME);
    void  setVolume(uint8_t volume, uint8_t point5);    
    
    /** Get the volume.  Note that this is not the same value that you use to set the volume;
	 * this value is double the setVolume() value, +1 if "point5" is given.
     *
     * @return 
     *   Return the volume value.
     */
    uint8_t getVolume();

    /** Get the volume in dB.  Note that the "point5" value is not returned.
     *
     * @return 
     *   Return the volume value.
     */
    uint8_t getVolumeDB();
    
    /** Set the balance - If negative, the right side is made quieter by the number of decibels.
	 * 					  If positive, the left side is made quieter by the number of decibels.
	 * 					  (Think of turning a balance control on a stereo.)
     *   Maximum is 127 db. Parameters:
     * @param balance 
     *   The balance difference + or - up to 127 db
     *   If balance is positive, left will be softer (you're turning the knob clockwise).
     *   If balance is negative, right will be softer (you're turning the knob counterclockwise).
     * @param point5
     *   If given, then + or - 0.5 db to the balance.  If balance is positive,
     *   0.5 db will be added to your balance value. If balance is negative,
     *   0.5 db will be subtracted from your value.
     */    
    void  setBalance(int16_t balance = 0);
    void  setBalance(int16_t balance, uint8_t point5);
    
        
    /** Get the balance - volume difference between left-right.
     * 
     * @return
     *   Difference in balance, which is double the balance dB value given in setBalance.
     */
    int16_t getBalance();

    /** Get the balance - volume difference between left-right.
     * 
     * @return
     *   Difference in balance in dB. Note that the "point 5" value is not returned.
     */
    int16_t getBalanceDB();

    /** Get the treble frequency limit.
     * 
     * @return
     *   Frequenzy 1000, 2000 .. 15000Hz.
     */
    uint8_t   getTrebleFrequency(void);

    /** Set the treble frequency limit.
     * 
     * @param frequency
     *   Frequency 1, 2, .. 15 * 1000Hz.
     */
    void  setTrebleFrequency(uint8_t frequency = DEFAULT_TREBLE_FREQUENCY);
    
    /** Get the treble amplitude.
     * 
     * @return
     *   Amplitude -8 .. 7dB (in 1.5dB steps); 0 = off.
     */
    uint8_t   getTrebleAmplitude(void);

    /** Set the treble amplitude.
     * 
     * @param amplitude
     *   Amplitude -8 .. 7dB (in 1.5dB steps); 0 = off.
     */    
    void  setTrebleAmplitude(int8_t amplitude = DEFAULT_TREBLE_AMPLITUDE);   
    
     /** Get the bass frequency limit.
     * 
     * @return
     *   Frequency 2, 3, .. 15 * 10 Hz.
     */
    uint8_t   getBassFrequency(void);

    /** Set the bass frequency limit.
     * 
     * @param frequency
     *   Frequency 2, 3, .. 15 * 10 Hz.
     */
    void  setBassFrequency(uint8_t frequency= DEFAULT_BASS_FREQUENCY);  
    
    /** Get the bass amplitude.
     * 
     * @return
     *   Amplitude 0 .. 15dB (in 1dB steps); 0 = off.
     */
    uint8_t   getBassAmplitude(void);

    /** Set the bass amplitude.
     * 
     * @param amplitude
     *   Amplitude 0 .. 15dB (in 1dB steps); 0 = off.
     */    
    void  setBassAmplitude(uint8_t amplitude = DEFAULT_BASS_AMPLITUDE);  
    
    /** Set the speed of a playback.
     * 
     * @param speed
     *   Speed 0, 1, .. (0, 1 normal speed). 
     *   Buffer must be filled quickly enough, otherwise the VS1053 will stu-uh-uh-uh-
     *   stutter. Research shows that an otherwise unoccupied ATmega328p is about
     *   able to keep up with 6x speed.
     */    
    void setPlaySpeed(uint16_t speed);       
        
    /** Start audio file. Will not start a new file if an existing track
     * is alreading playing. We need to separate start() from play() because,
     * rather than playing, we may want to grab the track's header before
     * playing.
     * @return
     *  0 if another track is playing
     *  -1 on track open error
     *  1 on success
	 */
    int8_t start(char *filename);

    /** Start playing audio from file. Will not play a new file if an existing track
	 * is already playing; you must cancel() first in order to play() something new.
     *
     * If using Interrupts:
     *
     * If not using Interrupts:
     * @param filename
	 *  from the SD card
	 * @return
     *  0 if already playing, 1 if successful start.
     */    
    int8_t play();

    /** Just stuff another file onto an existing stream. Closes the already open file.
     * Calls cancel() if the file open fails.
     * I doubt if this is supported by the chip, but what the heck.
	 * @return
     *  1 if successful start, -1 if file open failed.
     */
    int8_t concatenate(char *filename);


	/** send at least 32 bytes of data from the open file on the SD card to the vs1052. This
	 * method will check to see that the chip is ready to receive. A single block of 32 bytes
	 * is sent, or, if continuous is true, the chip will receive data in 32 byte chunks while
     * the vs1053 has buffer space. If at any time less than 32 bytes are available from the
     * file on the SD card, the stream is prepared for termination:
     * + read endFillByte from the VS1053.
     * + Fill the remainder of the send buffer with endFillBytes and send it.
     * + Begin the terminateStream() method, where the first state (send 2052 endFillByte's)
     *   is performed.
     * + The next interrupt, or the next call to loadVS1053FIFO(), will send you to the
     *   terminateStream() method.
     * + The termiateStream() method 
     *
     * DO NOT call this if you are using interrupts.
     * @param continuous
     *  load the FIFO until full (otherwise send a single block of 32 bytes)
     *  Subsequent calls of loadVS1053FIFO() will use the same value of continuous.
     *  This should always be true when loading via interrupt, or the VS1053 will not
     *  alert us that it needs new data (we are interrupted on RISING).
     *  Default is true, set in the constructor, above.
	 */
	static void loadVS1053FIFO(bool is_continuous);
	static void loadVS1053FIFO(void);


    /** Interrupt the playback. This only makes sense in the context of an interrupt-
     * driven methodology. If you want to pause using the "fill-it-yourself" method,
     * simply stop sending the chip data for a time.
     */    
    void pause(void);
    
    /** Stop the playback in the middle of a song. 
     *  After this call, you can send the next audio file to buffer.
     *      
     */        
    void cancel(void);
    
     /** Get information about played audio stream.
     * This is completely untested and not known to work. Still, it may form the basis
     * for the enterprising coder who wants to get it to work. :-)  -Mike 11/3/2012
     * @param aInfo
     *   Return value for the informations.
     *     
     */    
    void getAudioInfo(AudioInfo* aInfo);

    /**
	 * Does a hardware followed by a software reset.
	 */
    void powerCycle(void);

    /**
	 * RESET signal is driven low, which doubles as a power down situation.
	 */
    void powerDown(void);

    /**
	 * RESET signal is brought up. Then initialize() is run.
	 */
    uint8_t powerUp(void);


    /** getFileComments
     * Get file comments from the file. This is stuff like "Title", "Artist", etc.
	 * This must be run after start() but before play(). If you run this after you play,
     * you will jump to the very beginning of the file (effects are undefined).
	 * MP3 and Ogg files are supported. Which of the fields (Ogg nomenclature) or frames
	 * (MP3/ID3v2 nomenclature) you want to get is up to you. However, there are
	 * limitations (the ATMega328p has 2k of ram, after all). They are:
	 * - Each field is stored in by default in the audio data buffer, so 31 bytes max
     *   only. See the method's code for how to increase this size.
	 *
	 *   We conveniently hijack the audioDataBuffer in this method so we don't need to
     *   consume any extra memory space, but you are free to gobble up as much as you're
     *   comfortable with (you'll need to initialize your own array and modify the code).
     *
     *   The contents will have a null (0, or \0) appended, for easy
     *   usage with your favorite string manipulation tools (that's why we only give a 31
     *   byte string in a 32 byte array). The null will be put right after the string, or
     *   in array cell 31, whichever is shortest.
     *
	 * - For Ogg files at least, if there are multiple fields of a type in the file, the
     *   routine will not get more than the first of each type. For example, if you want
     *   to get the "Artist" and there are 3 "Artist"s in your file, only the first Artist
     *   will be returned.
	 *  Ogg Files
	 *  You can actually use partial strings for the field names; for example: "TITL", "TRAC".
	 *  The routine will match if it finds a partial match, then iterate past the "=" sign
     *  (in the file), then return your field information. It is up to you to ensure that
     *  your field name is sufficiently unique that it will return the proper data every
     *  time. Note that field names are case-insensitive.
	 *  MP3 Files
	 *  MP3 files use ID3v2, which means that all the names are 4-bytes in length. So the
     *  frame names will always look like for example: "TALB", "TCOM", "TYER".
     *  see http://id3.org/id3v2.3.0
     *
     * @param fileName
	 *  The music file from the SD card, in 8.3 formate
	 * @return
     *  -1 upon file error
     */
	#define MUSIC_FILE_BUFFER VS_BUFFER_SIZE
	int8_t getFileComments(char *fieldName);

    /** where n defines the sine test to use. n is defined as follows:
     * Send a sine wavwe out of the VS1053.
     * The frequency of the sine to be output can now be calculated from
     * F=Fs × S / 128
     * Example: Sine test is activated with value 126, which is 0b01111110.
     * Breaking n to its components, FsIdx = 0b011 = 3 and thus Fs = 22050Hz.
     * S = 0b11110 = 30, and thus the final sine frequency
     * F = 22050Hz × 30	≈ 5168Hz.
     * 
     * FsIdx Fs		FsIdx Fs
     * 0	44100 Hz	4	24000 Hz
     * 1	48000 Hz	5	16000 Hz
     * 2	32000 Hz	6	11025 Hz
     * 3	22050 Hz	7	12000 Hz
     *
     * FsIdx comes from bits 7:5 of n.  S comes from bits 4:0 of n.
     *
     */

    void sineTestActivate(uint8_t);

    /** Stop that horrid beep!
     */
    void sineTestDeactivate(void);

    /** Reset that bad boy.
     * This is a hardware reset of the VS1053, where _RST is brought low for 5 micros,
     * then brought back high.
     *
     * A software reset, btw, is done by activating bit  RESET in register SCI MODE
     * (Chapter 8.7.1 in the datasheet). Then wait for at least 2 μs.
     */
	void resetVS1053(void);

	/** Return a pointer to the audio data buffer. This buffer is used for dumping the
	 * track comments, as well as audio data.
 	* @return
 	*    pointer to the audio data buffer of VS_BUFFER_SIZE length.
 	*/    
	static uint8_t *getAudioDataBuffer(void);

    //
    // STATIC VARIABLES ********************************************************
    //
    // You can query this to find out if the library believes a file is currently playing.
	static volatile bool isPlaying;
	// Player Stuff
    // Count of the number of bytes sent to the VS1053, from the file
    // (endFillBytes don't count)
	static volatile uint32_t byteCount;
    // return value from the SD card library read functions.
	static volatile int8_t SDFileReadResults;
    // this value differs for each file type, so it must be read dynamically whenever
    // you wish to terminate a song.
	static volatile uint8_t vs1053EndFillByte;
    // fileError is for you, the user of the library.
    // You can query this to see if there has been a file open or read error in the
    // recent past. This is reset to false in the play() method. But it will be
    // set by the loadVS1053FIFO() method, start() method, init() method... wherever
    // the SD card is actually accessed.
	static volatile bool fileError;

    // For debugging. Not used in production.
	static volatile uint32_t callCount;
    // For timing. Not used in production.
	static volatile uint16_t timer0OFCounter;

	// static char id3Frame[4]; // See "What is this for?" in the .cpp file

protected:

    // This is an internal variable that is used to guide the code to the
    // terminateStream() method.
	static volatile bool terminate;
    /**
     * This method actually sends the data to the SCI_VOL register on the chip.
     * Volume and balance levels are both sent. You never need to use this; programs
     * will use the setVolume() method (above).
     */
    void changeVolume(void);
        
    void updateBassTreb(void);
    
	/** Fill the 32-byte buffer with data, either real data from the file or
 	*  (if not enough data, or you can't complete the file read)
 	*  with real data + endFillByte up to 32 bytes.
 	* @return
 	*    void, but sets fileReadResults:  -1 if file not open, otherwise the results 
 	*    of track.read();
 	*/    
	static void fillBuffer(void);

	/** Attempt to transfer a block of 32 bytes fram the buffer to the vs1053.
 	* Buffer is guaranteed to be filled with song and/or endFillByte.
 	* See the audioByteCount variable if you want to see if any more bytes
 	* have actually been sent. MIKE TEST!!!
 	*/
	static void fileXferBlock(void);

    /** Read endFillByte from the VS1053.
     * @return
     *  void, but sets volatile uint8_t SFAudioShield::vs1053EndFillByte, a
     *  static class variable.
     */
	static void readEndFillByte(void);

    /** terminate the stream normally.
     * This method is called by loadVS1053FIFO() (which is also our Interrupt Handler).
     * This is a stateful method; it jumps from state to state, which is stored in
     * the terminateState variable. If it exits, then the next call to loadVS1053FIFO() will 
     * simply continue the process in this method at the appropriate state.
     *
     * We depend on the usual _DREQ/interrupt interaction to regulate the data flow to the
     * chip. Or, if loading data from loop(), the terminateStream() method will work properly.
     * How do you know if your stream is done?  Check the isPlaying variable.
     *
     * Note: I am kind, very kind. How kind? Well, in
     * past iterations of this method, the whole enchilada was performed in one swoop.
     * This means that, based on my measurements, if it takes 0.32 millis to load 32 bytes,
     * times 64 (to get it to 2048 bytes) which is at least 20 millis, then this method took
     * at least that long to return. Do you really want your ATmega328p to linger in a
     * single method for 20 millis!?  No, you don't. So I made this method so that it
     * doesn't sit and wait for the whole 2048-byte payload to load; it returns as soon as
     * the VS1053 brings DREQ low (...which means it's got no more room for any data).
     *
     * This method respects continuous (see loadVS1053FIFO()).
     */
	static void terminateStream(void);

    /**
     */
	static void transferBuffer(void);

    char*                           _buffer;
    char*                           _bufferReadPointer;
    char*                           _bufferWritePointer;
    
    // variables to save, values in db
    int16_t                           _balance;    
    uint8_t                           _volume;
    //   bass enhancer settings  
    uint8_t                             _sb_amplitude;
    uint8_t                             _sb_freqlimit;
    uint8_t                             _st_amplitude;
    uint8_t                             _st_freqlimit; 

	// VS1053's FIFO is filled via interrupt, or must be called by the caller often enough
	// to ensure it doesn't drain out.
	bool							_via_interrupt;

    // If the loadVS1053FIFO() was called to load the VS1053 continuously until its
    // FIFO is filled, or only a single 32-byte block should be sent.
    // This should always be true when loading via interrupt, or the VS1053 will not
    // alert us that it needs new data (we are interrupted on RISING).
    // Default is true from the constructor, above.
	bool _continuous;

 
    static const char PROGMEM          _sampleRateTable[4][4];       // _sampleRateTable[id][srate]

};

#endif
