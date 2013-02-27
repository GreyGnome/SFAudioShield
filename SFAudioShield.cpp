// in vi/vim:   :set ts=4 sw=4
/*
 * SparkFun MP3Shield Audio Library: SFAudioShield
 * Copyright 2012 Michael Schwager
 *
 * This library is distributed under the following license:
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or(at your option) any later
 * version. This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.  <http://www.gnu.org/licenses/>
 */
/* 
 * Much code taken from: http://mbed.org/users/christi_s/code/VS1053b/
 * mbed VLSI VS1053b library
 *
 * The following notice accompanies this code, as this library is based on code from
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

#include "SFAudioShield.h"

volatile bool SFAudioShield::isPlaying=false;
volatile uint32_t SFAudioShield::byteCount=0;
volatile int8_t SFAudioShield::SDFileReadResults=0;
volatile uint8_t SFAudioShield::vs1053EndFillByte=0;
volatile bool SFAudioShield::fileError=false;
volatile uint32_t SFAudioShield::callCount=0;
volatile bool SFAudioShield::terminate=false;
// Buffer for music from file. This is also used for storing data from file metadata.
static uint8_t audioDataBuffer[VS_BUFFER_SIZE];
volatile uint16_t SFAudioShield::timer0OFCounter=0;
// This needs to be static so that static methods such as the interrupt handler
// can know the value of the variable as set by the programmer when the SFAudioShield object
// is created (since the interrupt handler is a general-purpose byte-transferrer
// to the VS1053). We can't use static variables in initializers of the Class.
static bool _via_interruptS=DEFAULT_INTERRUPT_STATE;
bool SFAudioShield::continuous=true;

const char PROGMEM SFAudioShield::_sampleRateTable[4][4] = {
    11, 12, 8, 0,
    11, 12, 8, 0,
    22, 24, 16, 0,
    44, 48, 32, 0
};
 

/*===================================================================
 * Functions
 *==================================================================*/
 
static inline __attribute__((always_inline)) void cs_low(void) { _CS(LOW); }
static inline __attribute__((always_inline)) void cs_high(void) { _CS(HIGH); }
static inline __attribute__((always_inline)) void dcs_low(void) { _DCS(LOW); }
static inline __attribute__((always_inline)) void dcs_high(void) { _DCS(HIGH); }
static inline __attribute__((always_inline)) void sci_en(void) { _CS(HIGH); _DCS(HIGH); _CS(LOW); }  //SCI enable
static inline __attribute__((always_inline)) void sci_dis(void) { _CS(HIGH); }						 //SCI disable
static inline __attribute__((always_inline)) void sdi_en(void) { _DCS(HIGH); _CS(HIGH); _DCS(LOW); } //SDI enable
static inline __attribute__((always_inline)) void sdi_dis(void) { _DCS(HIGH); }						 //SDI disable

// hardware reset
void SFAudioShield::resetVS1053(void) {
    if (_via_interrupt) { INTERRUPT_HANDLER_DISABLE; }
	delay(10); _RST(LOW); delay(5); _RST(HIGH); delay(10);
}

// IF DREQ is high, vs1053 can take at least 32 bytes of data.
void sci_write(uint8_t address, uint16_t data) {
    while (!_DREQ);                       //wait until data request is high
    // disable all interrupts
    uint8_t oldSREG = SREG; cli();
	// but maybe we returned from the previous while, and *just* before the cli() we got interrupted!
	// Then we would fill the VS1053's FIFO. So, to be careful,
    while (!_DREQ);                       //wait until data request is high

    sci_en();                             //enables SCI/disables SDI
    SPI.transfer(0x02);                   //SCI write
    SPI.transfer(address);                //register address
    SPI.transfer((data >> 8) & 0xFF);     //write out first half of data word
    SPI.transfer(data & 0xFF);            //write out second half of data word

    sci_dis();                                //enables SDI/disables SCI
	// We waited for DREQ to be high.  Since DREQ was high before this
	// routine, it will be set high afterwards.
	// The interrupt routine won't be called unless the signal is
	// RISING. So it is safe to enable all interrupts now.
	SREG=oldSREG;
	delayMicroseconds(5); // delay here for if we call another AudioShield routine.
						  // Some may not check DREQ or what have you,
						  // and may try to write too soon.
}

uint16_t sci_read(uint16_t address) {

    while (!_DREQ);                           //wait until data request is high

    // disable all interrupts
    uint8_t oldSREG = SREG; cli();
	// but maybe we returned from the previous while, and *just* before the cli() we got interrupted!
	// Then we would fill the VS1053's FIFO. So, to be careful,
    while (!_DREQ);                           //wait until data request is high
     
	sci_en();                                //enables SCI/disables SDI
    SPI.transfer(0x03);                        //SCI write
    SPI.transfer(address);                     //register address
    uint16_t received = SPI.transfer(0x00);    //write out dummy byte; causes SPI to activate and SPDR to be filled upon read.
    received <<= 8;
    received |= SPI.transfer(0x00);            //write out dummy byte; causes SPI to activate and SPDR to be filled upon read.
 
    sci_dis();                                //enables SDI/disables SCI
 
    // enable all interrupts
	SREG=oldSREG;
    return received;                        //return received word
}

// Write data to the VS1053. fromFile is true if we're counting the bytes sent from a file.
// There are instances where we want to send data to the chip but it's not from a file.
// Also, a file read error means we will not count the bytes because it's not data that's
// been properly set.
static void sdi_write_data(uint8_t count, bool fromFile) {
	uint8_t i;

    // disable all interrupts
    uint8_t oldSREG = SREG; cli();
    sdi_en();
	// if asked to send the entire buffer_size, increase the byteCount sent to the shield...
	// but only if the file read was successful and only send number of bytes sent is less than the
	// file read results (we're counting the size of the music file)
	if (fromFile) {
		if (SFAudioShield::SDFileReadResults < 0) fromFile=false;
	}
	for (i=0; i < count; i++) {
		if (fromFile) {
			if (i <= SFAudioShield::SDFileReadResults) SFAudioShield::byteCount++;
		}
    	SPI.transfer(audioDataBuffer[i]);
	}
    sdi_dis();    
	SREG=oldSREG;
}

void SFAudioShield::powerCycle(void) {              //hardware and software reset
    // disable all interrupts
    uint8_t oldSREG = SREG; cli();
    sci_en();
    resetVS1053();
//    sci_write(0x00, SM_PDOWN);
    sci_write(0x00, 0x04);			// software reset, bit 4
    while (!_DREQ);                 // wait until data request is high
}

void SFAudioShield::powerDown(void) {              //hardware and software reset
	_RST(LOW);
}

uint8_t SFAudioShield::powerUp(void) { //hardware and software reset
	_RST(HIGH);
    while (!_DREQ);                 // wait until data request is high
	return(initVS1053());
}

// No need to do this as the SD card library does it for us.
/*
void SFAudioShield::spi_initialise(void) {
    _RST(HIGH);                                //no reset
    SPI.format(8,0);                        //spi 8bit interface, steady state low
//   SPI.frequency(1000000);                //rising edge data record, freq. 1Mhz
    SPI.frequency(2000000);                //rising edge data record, freq. 2Mhz
 
 
    cs_low();
    for (int i=0; i<4; i++) {
        SPI.write(0xFF);                        //clock the chip a bit
    }
    cs_high();
    dcs_high();
    wait_us(5);
}
*/

/*
 * No need to do this because sdfat library handles it for us.
 */
/*
void SFAudioShield::sdi_initialise(void) {
    SPI.frequency(8000000);                //set to 8 MHz to make fast transfer
    cs_high();
    dcs_high();
}
*/

 
static uint16_t wram_read(uint16_t address) {
    uint16_t tmp1,tmp2;
    sci_write(SCI_WRAMADDR,address);
    tmp1=sci_read(SCI_WRAM);
    sci_write(SCI_WRAMADDR,address);
    tmp2=sci_read(SCI_WRAM);
    if (tmp1==tmp2) return tmp1;
    sci_write(SCI_WRAMADDR,address);
    tmp1=sci_read(SCI_WRAM);
    if (tmp1==tmp2) return tmp1;
    sci_write(SCI_WRAMADDR,address);
    tmp1=sci_read(SCI_WRAM);
    if (tmp1==tmp2) return tmp1;
    return tmp1;
}
 
void wram_write(uint16_t address, uint16_t data) {
    sci_write(SCI_WRAMADDR,address);
    sci_write(SCI_WRAM,data);
    return;
}
 
uint8_t SFAudioShield::initVS1053(void) {
	isPlaying=false;
	pinModeFast2(MP3_DREQ, INPUT);
	pinModeFast2(MP3_CS, OUTPUT);
	pinModeFast2(MP3_DCS, OUTPUT);
	pinModeFast2(MP3_RST, OUTPUT);

    _CS(HIGH);	//Deselect Control       
	_DCS(HIGH);	//Deselect Data
	_RST(LOW); //Put VS1053 into hardware reset

	//We have no need to setup SPI for VS1053 because this has already been done by the SDfatlib
    //spi_initialise();                    //initialise MBED
         
    //sci_write(SCI_MODE, (SM_SDINEW+SM_RESET)); //  set mode reg.    
    //wait_ms(10);
	//From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz. 
	//Internal clock multiplier is 1.0x after power up. 
	//Therefore, max SPI speed is 1.75MHz. We will use 1MHz to be safe.
	SPI.setClockDivider(SPI_CLOCK_DIV16); //Set SPI bus speed to 1MHz (16MHz / 16 = 1MHz)
	for (int i=0; i<4; i++) {
		SPI.transfer(0xFF); //Throw a dummy byte at the bus
    }
	//Initialize VS1053 chip 
	delay(10);
 	_RST(HIGH); //Bring up VS1053

#ifdef DEBUG    
    uint16_t info = wram_read(para_chipID_0);
    //DEBUGOUT("SFAudioShield: ChipID_0:%04X\r\n", info);
    info = wram_read(para_chipID_1);
    //DEBUGOUT("SFAudioShield: ChipID_1:%04X\r\n", info);
    info = wram_read(para_version);
    //DEBUGOUT("SFAudioShield: Structure version:%04X\r\n", info);
#endif
 
    //get chip version, set clock multiplier and load patch
	//Now that we have the VS1053 up and running, increase the internal clock multiplier and up our SPI rate
    int i = (sci_read(SCI_STATUS) & 0xF0) >> 4;
    if (i == 4) {
     
        //DEBUGOUT("SFAudioShield: Installed Chip is: VS1053\r\n");
        sci_write(SCI_CLOCKF, (SC_MULT_XTALIx50));
        delay(10);
    } 
    else
    {
        //DEBUGOUT("SFAudioShieldb: Not Supported Chip\r\n");
        return 0;
    }
     
    // change spi to higher speed 
    //sdi_initialise();                
	SPI.setClockDivider(SPI_CLOCK_DIV4); //Set SPI bus speed to 4MHz (16MHz / 4 = 4MHz)

    _CS(HIGH);
    _DCS(HIGH);
	setVolume(DEFAULT_VOLUME);
    updateBassTreb();    
    return 1;
}

uint8_t SFAudioShield::init(void) {
	fileError=false;
	if (!card.init(SPI_FULL_SPEED, SD_CS)) { fileError=true; return 2; }

	for (uint8_t i=0; i < 4; i++) {
		delay(300);
		digitalWriteFast(MYLED, HIGH);
		delay(100);
		digitalWriteFast(MYLED, LOW);
	}

	//Initialize a volume on the SD card.
	if (!driveVolume.init(&card)) { fileError=true; return 3; }
	//Open the root directory in the volume.
	if (!root.openRoot(&driveVolume)) {
		fileError=true; return 4; //Serial.println("Error: Opening root"); //Open the root directory in the driveVolume. 
	}

	_via_interruptS=_via_interrupt;
	return(initVS1053());
}
 
void SFAudioShield::setPlaySpeed(uint16_t speed)
{
    wram_write(para_playSpeed, speed);
    //DEBUGOUT("SFAudioShield: Change speed. New speed: %d\r\n", speed);
}

/*
 * volume is from 0 (highest) to 0xFE (254), or 127 dB down (aka, "Total silence").
 * Volume is entered as the number of db. If you want to do 1/2 db, give it the "POINT5"
 * argument.
 */
void SFAudioShield::setVolume(uint8_t vol) 
{    
	setVolume(vol, 0);
}
 
void SFAudioShield::setVolume(uint8_t vol, uint8_t point5) 
{
	if (vol > 127) vol=127;
	_volume=vol * 2 + point5;

    changeVolume();
}

uint8_t SFAudioShield::getVolume(void) 
{
    return _volume;
}
 
uint8_t SFAudioShield::getVolumeDB(void) 
{
    return _volume >> 1;
}

void SFAudioShield::setBalance(int16_t balance) 
{    
    setBalance(balance, 0);
}

// + Means left is softer.
void SFAudioShield::setBalance(int16_t balance, uint8_t point5) 
{
	if (point5 > 0) point5=1;
	if (balance > 127) { balance=127; point5=0; }
	if (balance < -127) { balance=-127; point5=0; }

    _balance = balance * 2;
	if (_balance < 0) _balance-=point5;
	else _balance+=point5;

    changeVolume();
}
 
int16_t SFAudioShield::getBalance(void)
{
    return _balance;    
}

int16_t SFAudioShield::getBalanceDB(void)
{
    return _balance >> 1;    
}
 
// From datasheet:
// 		In VS1053b bass and treble initialization and volume change is delayed until
// 		the next batch of samples are sent to the audio FIFO.
void SFAudioShield::changeVolume(void) 
{
	int16_t left, right;
	left=_volume; right=_volume;
	if (_balance > 0) 
		left+=_balance;
	else
		right-=_balance;
	if (right > 254) right=254;
	if (right < 0) right=0;
	if (left > 254) left=254;
	if (left < 0) left=0;
	// volume calculation and send
    sci_write(SCI_VOL, (uint16_t)(left << 8) + (right & 0xFF));
	delayMicroseconds(6);
     
    //DEBUGOUT("SFAudioShieldb: Change volume to %#x (%f, Balance = %f)\r\n", volCalced, _volume, _balance);        
}
 
uint8_t SFAudioShield::getTrebleFrequency(void)
{
    return _st_freqlimit;
}
 
 
void SFAudioShield::setTrebleFrequency(uint8_t frequency)
{
    if(frequency < 1) frequency = 1;
    else if(frequency > 15) frequency = 15;
    _st_freqlimit = frequency;
    updateBassTreb();
}
     
uint8_t SFAudioShield::getTrebleAmplitude(void)
{
    return _st_amplitude;
}
 
void SFAudioShield::setTrebleAmplitude(int8_t amplitude)
{
    if(amplitude < -8) amplitude = -8;
    else if(amplitude > 7) amplitude = 7;
    _st_amplitude = amplitude;
    updateBassTreb();
}   
     
uint8_t SFAudioShield::getBassFrequency(void)
{
    return _sb_freqlimit * 10;
}
 
void SFAudioShield::setBassFrequency(uint8_t frequency)
{
    if(frequency < 2) frequency = 2;
    else if(frequency > 15) frequency = 15;
    _sb_freqlimit = frequency;
    updateBassTreb();
}  
     
uint8_t SFAudioShield::getBassAmplitude(void)
{
    return _sb_amplitude;
}
 
void SFAudioShield::setBassAmplitude(uint8_t amplitude)
{
    if(amplitude > 15) amplitude = 15;
    _sb_amplitude = amplitude;
    updateBassTreb();
}
 
void SFAudioShield::updateBassTreb(void)
{
    uint16_t bassCalced = ((_st_amplitude  & 0x0f) << 12) 
                              | ((_st_freqlimit  & 0x0f) <<  8) 
                              | ((_sb_amplitude  & 0x0f) <<  4) 
                              | ((_sb_freqlimit  & 0x0f) <<  0);
                             
    sci_write(SCI_BASS, bassCalced);    
 
    //DEBUGOUT("SFAudioShieldb: Change bass settings to:\r\n")
    //DEBUGOUT("SFAudioShieldb: --Treble: Amplitude=%i, Frequency=%i\r\n", getTrebleAmplitude(), getTrebleFrequency());
    //DEBUGOUT("SFAudioShieldb: --Bass:   Amplitude=%i, Frequency=%i\r\n", getBassAmplitude(), getBassFrequency());
}

inline __attribute__((always_inline)) void SFAudioShield::readEndFillByte(void) {
	vs1053EndFillByte=(uint8_t)(wram_read(para_endFillByte) & 0x00FF);
}
 
enum terminateState
{
    SEND2052ENDFILLBYTE,
    SETSMCANCEL,
    CHECKSMCANCEL,
    CHECKSWRESET,
    TERMINATEDONE
};
static terminateState terminateState=SEND2052ENDFILLBYTE;
static uint16_t bytesSent=0;
// endFillByte is previously loaded into audioDataBuffer[] already by fillBuffer();
// 3. Send at least 2052 bytes of endFillByte[7:0].
// 4. Set SCI MODE bit SM CANCEL.
// 5. Send at least 32 bytes of endFillByte[7:0].
// 6. Read SCI MODE. If SM CANCEL is still set, go to 5. If SM CANCEL hasn’t cleared
//    after sending 2048 bytes, do a software reset (this should be extremely rare).
// 7. The song has now been successfully sent. HDAT0 and HDAT1 should now both contain 0
//    to indicate that no format is being decoded. Return to 1.
inline __attribute__((always_inline)) void SFAudioShield::terminateStream(void) {
	uint16_t sciModeWord;
    switch(terminateState)
    {
    case SEND2052ENDFILLBYTE:
        if (bytesSent==0) {
            for (uint8_t j = 0; j < VS_BUFFER_SIZE; j++) audioDataBuffer[j]=vs1053EndFillByte; 
        }
        // Using Interrupt mode: bytes will be sent as long as _DREQ stays high, or we reach 2048 bytes.
        // Then we write 4 more bytes, set SM_CANCEL, and fall through to the next case statement.
        // Using non-Interrupt mode: bytes will be sent 32 at a time. Every call, we send 32 more bytes.
        // Once we reach 2048 bytes, we'll send the last 4 bytes. Then we set SM_CANCEL, and fall
        // through to the next case statement.
        while (_DREQ) {
            if (bytesSent < 2048) {
                transferBuffer(); bytesSent+=32; // 0.32 millis (320 uS) to load 32 bytes.
                if ((! _via_interruptS) && (! continuous)) return;
            }
            else {
                sdi_write_data(4, false); // send 4 more endFillByte
                terminateState=SETSMCANCEL; bytesSent=0;
                break;
            }
        }
        if (terminateState != SETSMCANCEL) return;
    case SETSMCANCEL:
	    sciModeWord = sci_read(SCI_MODE);
        sciModeWord |= SM_CANCEL;
        sci_write(SCI_MODE, sciModeWord);
	    delayMicroseconds(7); // wait 80 CLKI cycles, or 80/12288000 seconds (see the datasheet)
        terminateState=CHECKSMCANCEL;
        if (! _via_interruptS) return;
    case CHECKSMCANCEL:
        // 5. Send at least 32 bytes of endFillByte[7:0].
        // 6. Read SCI MODE. If SM CANCEL is still set, go to 5. If SM CANCEL hasn’t cleared
        //    after sending 2048 bytes, do a software reset (this should be extremely rare).
        while (_DREQ) {
            if (bytesSent < 2048) {
                transferBuffer(); bytesSent+=32;
                // read SCI MODE; if SM CANCEL is still set, repeat.
                sciModeWord = sci_read(SCI_MODE);    
                if ((sciModeWord & SM_CANCEL) == 0x0000) {
                    terminateState=TERMINATEDONE;
                    break;
                }
                if ((! _via_interruptS) && (! continuous)) return; // if called from a main program: send 1 block at a time.
            }
            else {
		        // In some cases the decoder software has to be reset. This is done by activating bit
                // SM RESET in register SCI MODE (Chapter 8.7.1). Then wait for at least 2 μs,
    	        sciModeWord = sci_read(SCI_MODE);
    	        sciModeWord |= SM_RESET;    
    	        sci_write(SCI_MODE, sciModeWord);
		        delayMicroseconds(2);
                bytesSent=0;
                terminateState=CHECKSWRESET;
                return;
            }
        }
    case CHECKSWRESET:
        // then look at DREQ. DREQ will stay down for about 22000 clock cycles, which means an
        // approximate 1.8 ms delay if VS1053b is run at 12.288 MHz. After DREQ is up, you
        // may continue playback as usual.
        if (_DREQ) terminateState=TERMINATEDONE;
        else return;
    }
    // Friday 2/15/13 car 2961; 6:21pm at Pulaski; cell phone
    if (terminateState==TERMINATEDONE) {
        if (_via_interruptS) { ENDING_START } // detaches out interrupt, sets interrupts on
        sci_write(SCI_DECODE_TIME, 0x0000);
	    delayMicroseconds(9); // wait 100 CLKI cycles, as per documentation
        bytesSent=0;
        track.close(); terminate=false; isPlaying=false; terminateState=SEND2052ENDFILLBYTE;
    }
}

inline void SFAudioShield::fillBuffer(void) {
	int8_t i;
	//Serial.print("&"); Serial.flush();
	sci_dis(); sdi_dis();
	SDFileReadResults=(track.read(audioDataBuffer, VS_BUFFER_SIZE)); //Go out to SD card and try reading VS_BUFFER_SIZE new bytes of the song
	//Serial.print("&"); Serial.flush();
	//Serial.print("&"); Serial.flush();

	i=SDFileReadResults;
	if (SDFileReadResults < 0) { fileError=true; i=0; }
	//Serial.print("&"); Serial.flush();
	if (i < VS_BUFFER_SIZE) {
		readEndFillByte();
		for (; i < VS_BUFFER_SIZE; i++) audioDataBuffer[i]=vs1053EndFillByte;
	}
	//Serial.print("&"); Serial.flush();
}

// IF DREQ is high, vs1053 can take at least 32 bytes of data.
inline __attribute__((always_inline)) void SFAudioShield::transferBuffer(void) {
	while (!_DREQ); // wait till it goes high
	sdi_write_data(VS_BUFFER_SIZE, true);
}
 
// IF DREQ is high, vs1053 can take at least VS_BUFFER_SIZE bytes of data.
/** Send VS_BUFFER_SIZE bytes of file to vs1053. If we don't have VS_BUFFER_SIZE
  * bytes of file, send vs1053EndFillByte.
  */
inline __attribute__((always_inline)) void SFAudioShield::fileXferBlock(void)
{
	//Serial.print("+"); Serial.flush();
	//Go out to SD card and try reading VS_BUFFER_SIZE new bytes of the song.
	//Buffer is guaranteed to be filled with song and/or vs1053EndFillByte
	fillBuffer();
    // write buffer to vs1053b
	transferBuffer();
	//Serial.print("+"); Serial.flush();
}
 
void SFAudioShield::loadVS1053FIFO(bool is_continuous)
{
	continuous=is_continuous;
	loadVS1053FIFO();
}

// IF DREQ is high, vs1053 can take at least VS_BUFFER_SIZE bytes of data.
void SFAudioShield::loadVS1053FIFO()
{
	while (_DREQ && isPlaying) {
        if (terminate) terminateStream();
        else {
	        //if (TIFR0 & _BV(TOV1)) { 	// Read Timer 0 overflow bit
	        //sbi(TIFR0, TOV1); 		// Reset Timer 0 overflow bit
	        ////TIFR0 = (regTimer0 | _BV(TOV1));
	        //timer0OFCounter++;
	        //}
	        callCount++;  // CALL COUNT UPDATED HERE ************************
	        fileXferBlock();
	        if(SDFileReadResults < VS_BUFFER_SIZE) terminate=true;
        }
		if (! continuous) break;
	}
}

int8_t SFAudioShield::start(char *fileName)
{
	if (isPlaying) return 0;
	if (_via_interrupt) { INTERRUPT_HANDLER_DISABLE; }
	sci_dis(); sdi_dis();
	if (!track.open(&root, fileName, O_READ)) { fileError=true; return -1; }

	return 1;
}

// MIKE: _via_interrupt work here!
int8_t SFAudioShield::play()
{
	digitalWriteFast(MYLED, LOW);
	if (isPlaying) return 0;
	timer0OFCounter=0;
	sci_write(SCI_MODE, 0x0802); // SM_SDINEW and SM_LAYER12 are set. SM_SDINEW is the default, as per the datasheet.
	// SM_LAYER12 allows MPEG layer 1 and 2. You are liable for any fucking software patent
	// issues that arise, because there's a fucking MPEG software patent. The greedy
	// ba$tard$.  http://www.groklaw.net  Go there. Read it. Learn. And Fuck software patents.
	// I say that with all the professionalism I can muster. :-)
	delayMicroseconds(6);
	fileError=false;

	byteCount=0; callCount=0;
	isPlaying = true;

	if (_via_interrupt) sei(); // Interrupts on
	DEBUGOUT("play() called.");
	//DEBUGOUT("TCNT1 readings. Each tick is 64 micros. TCNT1 is reset now.");
	//TCNT1=0x0000;
	DEBUGOUT("TCNT1:"); DEBUGOUT2(TCNT1, DEC); DEBUGNL;
	changeVolume();
	sci_write(SCI_DECODE_TIME, 0x0000); delayMicroseconds(9); // wait 100 CLKI cycles, as per documentation
	loadVS1053FIFO(true); // prime it, damn it!
	//Serial.print(getPSTR("TCNT1:")); Serial.println(TCNT1, DEC);
	//Serial.println(millis(), DEC);
	DEBUGOUT(" pCOUNT: "); DEBUGOUT2(callCount, DEC);
	if (_via_interrupt) { INTERRUPT_HANDLER_ENABLE; };
	DEBUGOUT(" PLAY:GO!!! ");
	DEBUGOUT2(SDFileReadResults, DEC);
	//Serial.print(getPSTR("TCNT1:")); Serial.println(TCNT1, DEC);
	return 1;
}

void SFAudioShield::pause(void)
{
	INTERRUPT_HANDLER_DISABLE;
	DEBUGOUT("SFAudioShieldb: Pause.\r\n");
}

// Was "stop", but in vs1053 parlance it's "cancel".
// 1. Send a portion of an audio file to VS1053b.
// 2. Set SCI MODE bit SM CANCEL.
// 3. Continue sending audio file, but check SM CANCEL after every 32 bytes of data. If it is still set,
//    goto 3. If SM CANCEL doesn’t clear after 2048 bytes or one second, do a software reset (this should
//    be extremely rare).
// 4. When SM CANCEL has cleared, read extra parameter value vs1053EndFillByte (Chapter 9.11).
// 5. Send 2052 bytes of vs1053EndFillByte[7:0].
// 6. HDAT0 and HDAT1 should now both contain 0 to indicate that no format is being decoded. You can now send the next audio file.
void SFAudioShield::cancel(void)
{
    uint16_t sciModeWord;


	if (! SFAudioShield::isPlaying) { return ;} // nothing to do.
    if (_via_interrupt) { ENDING_START } // detaches out interrupt, sets interrupts on
    //DEBUGOUT("SFAudioShieldb: Song stopping..\r\n");
 
	while (!_DREQ);
	ENDING_SET_SM_CANCEL
     
	// 3. Continue sending audio file, but check SM CANCEL after every 32 bytes of data. If it is still set,
	//    goto 3. If SM CANCEL doesn’t clear after 2048 bytes or one second, do a software reset (this should
	//    be extremely rare).
	uint32_t start=millis();

    for (uint8_t i = 0; i < 64;) 
    { 
		// send at least 32 bytes of audio data (or vs1053EndFillByte)
		while (!_DREQ);
		fileXferBlock();
        // read SCI MODE; if SM CANCEL is still set, repeat
        sciModeWord = sci_read(SCI_MODE);    
        if ((sciModeWord & SM_CANCEL) == 0x0000) break;
		if ((millis()-start) > 1000) break;
    }

    if ((sciModeWord & SM_CANCEL) == 0x0000)
    {    
    	// send at least 2052 bytes of endFillByte[7:0].
		SEND_2048_ENDFILLBYTE
		while (!_DREQ); sdi_write_data(4, false); // send 4 more endFillByte

        DEBUGOUT("SFAudioShield: Song successfully stopped.\r\n");
		Serial.flush();
        //DEBUGOUT("SFAudioShield: SCI MODE = %#x, SM_CANCEL = %#x\r\n", sciModeWord, sciModeWord & SM_CANCEL);
        sci_write(SCI_DECODE_TIME, 0x0000);
    }
    else
    {
        DEBUGOUT("SFAudioShield: SM CANCEL hasn't cleared after sending 2048 bytes, do software reset\r\n");
        //DEBUGOUT("SFAudioShieldb: SCI MODE = %#x, SM_CANCEL = %#x\r\n", sciModeWord, sciModeWord & SM_CANCEL);
        initVS1053();
    }
	delayMicroseconds(7); // wait 80 CLKI cycles, or 80/12288000 seconds

	/*
	for (uint8_t j = 0; j < VS_BUFFER_SIZE; j++) audioDataBuffer[j]=vs1053EndFillByte;
    for (uint8_t i = 0; i < 255; i++) { // This is quite reliable, HDAT1 will always be 0.
		while (!_DREQ);
		sdi_write_data(VS_BUFFER_SIZE, false);
	}*/

	start=millis();
	uint32_t checkpoint=0;
    uint16_t hdat0 = sci_read(SCI_HDAT0); // see p. 50 of the VS1053 v. 1.01 datasheet
    uint16_t hdat1 = sci_read(SCI_HDAT1); // see p. 50 of the VS1053 v. 1.01 datasheet
	if (hdat1 != 0) {
		bool becameZero=false;
		DEBUGOUT("ERROR: **** cancel(): HDAT1 not 0; will WAIT! ***********\r\n");
		DEBUGOUT("HDAT0: "); DEBUGOUT2(hdat0, HEX); DEBUGNL;
		DEBUGOUT("HDAT1: "); DEBUGOUT2(hdat1, HEX); DEBUGNL;
		checkpoint=start;
		while (1) {
			if ((millis() - start) > 10000) {
				Serial.print(F("10 seconds: Tired of waiting for HDAT1!!!!!!!")); break;
			}
			if ((millis() - checkpoint) > 1000) {
				Serial.print(F("Checkpoint: 1 Secord: HDAT1 still not 0!!!"));
				checkpoint=millis();
			}
    		hdat1 = sci_read(SCI_HDAT1); // see p. 50 of the VS1053 v. 1.01 datasheet
			if (hdat1 == 0) {
				checkpoint=millis()-start;
				Serial.print(F("HDAT1 Became 0 after ")); Serial.println(checkpoint, DEC);
				becameZero=true;
				break;
			}
		}
		if (! becameZero) {
    		//sciModeWord = sci_read(SCI_MODE);
    		//sciModeWord |= SM_RESET;
    		sciModeWord = 0x0800 | SM_RESET; // from SFMP3Shield; he just sends these two bytes
    		sci_write(SCI_MODE, sciModeWord);
			delayMicroseconds(2);
			while (!_DREQ);
		}
	}

	isPlaying=false;
	track.close();

	DEBUGOUT("CANCELLED :-)"); DEBUGNL;
}

/* where n defines the sine test to use. n is defined as follows:
The frequency of the sine to be output can now be calculated from
	F=Fs × S / 128
Example: Sine test is activated with value 126, which is 0b01111110.
Breaking n to its components, FsIdx = 0b011 = 3 and thus Fs = 22050Hz.
S = 0b11110 = 30, and thus the final sine frequency
F = 22050Hz × 30	≈ 5168Hz.

FsIdx Fs		FsIdx Fs
0	44100 Hz	4	24000 Hz
1	48000 Hz	5	16000 Hz
2	32000 Hz	6	11025 Hz
3	22050 Hz	7	12000 Hz

FsIdx comes from bits 7:5 of n.  S comes from bits 4:0 of n.

*/
void SFAudioShield::sineTestActivate(uint8_t wave) {
    // disable all interrupts
    uint8_t oldSREG = SREG; cli();
	initVS1053();
	while (!_DREQ); // wait for DREQ to go high after reset
    uint16_t sciModeWord;// = sci_read(SCI_MODE);        
    sciModeWord = 0x0802 | SM_TESTS;    
    sci_write(SCI_MODE, sciModeWord);
	sdi_en();	//enables SDI/disables SCI
 
    while (!_DREQ);                           //wait unitl data request is high
    SPI.transfer(0x53);                        //SDI write
    SPI.transfer(0xEF);                        //SDI write
    SPI.transfer(0x6E);                        //SDI write
    SPI.transfer(wave);                        //SDI write
    SPI.transfer(0x00);                        //filler byte
    SPI.transfer(0x00);                        //filler byte
    SPI.transfer(0x00);                        //filler byte
    SPI.transfer(0x00);                        //filler byte
 
    // enable all interrupts
	SREG=oldSREG;
}

void SFAudioShield::sineTestDeactivate(void) {
    // disable all interrupts
    uint8_t oldSREG = SREG; cli();
    sdi_en();                                //enables SDI/disables SCI
 
    while (!_DREQ);
    SPI.transfer(0x45);                        //SDI write
    SPI.transfer(0x78);                        //SDI write
    SPI.transfer(0x69);                        //SDI write
    SPI.transfer(0x74);                        //SDI write
    SPI.transfer(0x00);                        //filler byte
    SPI.transfer(0x00);                        //filler byte
    SPI.transfer(0x00);                        //filler byte
    SPI.transfer(0x00);                        //filler byte
    // enable all interrupts
	SREG=oldSREG;
}
 
/* ogg_page is used to encapsulate the data in one Ogg bitstream page *****/

typedef struct {
  unsigned char *header;
  long header_len;
  unsigned char *body;
  long body_len;
} ogg_page;

uint8_t *SFAudioShield::getAudioDataBuffer() {
	//Serial.print(F("gADBuff:"));
	//Serial.println((char *) audioDataBuffer);
	return(audioDataBuffer);
}

void writeAudioDataBuffer(uint8_t count) {
	uint8_t i;

	//Serial.print(F("WADB: ")); Serial.println(count, DEC);
	for (i=0; i < count; i++) { Serial.write(audioDataBuffer[i]); }
	Serial.println("");
}

void printAudioDataBuffer(uint8_t count) {
	uint8_t i;

	for (i=0; i < count; i++) { Serial.print(audioDataBuffer[i], HEX); Serial.print(":"); }
	Serial.println("");
}

int8_t SFAudioShield::getFileComments(char *fieldName) {
	uint8_t i, j, audioType=UNKNOWN;
	uint32_t number32_1; // this 32-bit integer is used for multiple purposes
	uint32_t number32_2;  // this 32-bit integer is used for multiple purposes
	uint32_t number32_3=0;
	uint8_t musicFileFrameToRead;
	uint8_t *dataBuffer=audioDataBuffer; // Want a bigger buffer for your text and comments?  Assign a
                                         // bigger buffer here and redefine MUSIC_FILE_BUFFER in the .h
                                         // file. You'll need to make the data buffer static.
	bool found=true;

	track.seekSet(0);
	SDFileReadResults=track.read(dataBuffer, 1);
	if (dataBuffer[0]=='O') {					   // cheating; files begin with OggS or ID3; assume this
                                                   // means Ogg
		audioType=OGG_VORBIS;
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		// OGG HEADER: OK +++++++++++++++++++++++++++++++++++++++++
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		SDFileReadResults=(track.read(dataBuffer, 27)); // read the rest of Ogg header to the Segment table

		//printAudioDataBuffer(27); // !!!!!!!!!!!!!!!!!!!!!!

		number32_1=dataBuffer[26]; // Contains the length of the only segment, the vorbis id header
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		// VORBIS ID HEADER: OK +++++++++++++++++++++++++++++++++++
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		SDFileReadResults=(track.read(dataBuffer, number32_1)); // read the 30 bytes of the vorbis ID header;
		//track.seekCur(number32_1); // read past the vorbis ID header

		//DEBUGOUT("Vorbis Header Size: "); DEBUGOUT2(number32_1, DEC);
		// OK +++++++++++++++++++++++++++++++++++++++++++++++++++++
		//Serial.print(F("Contents: ")); printAudioDataBuffer(number32_1); // !!!!!!!!!!!!!!!!!!!!!!
		// Sanity check for Vorbis file.
		if ((dataBuffer[0] != 1) || (dataBuffer[1] != 'v')) { track.seekSet(0); return 0; }
		DEBUGOUT("Ogg header next:\r\n");

		// 4 + 1 + 4 + 4 + 4 + 4 + 1 + 1 = 23 bytes, + 1 for packet type + 6 for "vorbis" == 30
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		// OGG HEADER +++++++++++++++++++++++++++++++++++++++++++++
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		SDFileReadResults=(track.read(dataBuffer, 27)); // read the Ogg header; assume we're only
                                                        // interested in the first segment- the vorbis
                                                        // comments section.
		Serial.print(F(""));
		number32_2=dataBuffer[26]; // This is size of the page segment table; we don't really care about it.
								   // Should be: 11 (== 17 d)
		Serial.print(F("PG Sgmnt Size: ")); Serial.println(number32_2, DEC);
		SDFileReadResults=(track.read(dataBuffer, number32_2)); // read the segment table
		//printAudioDataBuffer(number32_2); // !!!!!!!!!!!!!!!!!!!!!!
		i=0; number32_1=0; for (;;) {
			number32_1+=dataBuffer[i]; if (dataBuffer[i] != 255) break; // the length of the vorbis
                                                                        // comments header
			i++;
		}
		//DEBUGOUT("Vorbis Comments Size: "); DEBUGOUT2(number32_1, DEC);
		//i=0;
		/*while (1) {
			number32_1+=dataBuffer[i];					// ...increase the size of the vorbis comment section
			if (dataBuffer[i] != 255) break;
			i++;
		}*/

		// 
		// number32_1 is the length of the entire comments header. Must maintain it throughout this section.
		//

		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		// VORBIS COMMENTS HEADER +++++++++++++++++++++++++++++++++
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		SDFileReadResults=(track.read(dataBuffer, 7)); number32_1-=7; //read: 3, 'v', 'o', 'r', 'b', 'i', 's'
		//printAudioDataBuffer(7); // !!!!!!!!!!!!!!!!!!!!!!
		SDFileReadResults=(track.read(dataBuffer, 4)); number32_1-=4; // read the length of the vendor string
		//Serial.print(getPSTR("Vendor length: ")); printAudioDataBuffer(4); // !!!!!!!!!!!!!!!!!!!!!!
		number32_2 =									             // then calculate it (it's in LSB first)
			dataBuffer[0] | dataBuffer[1] << 8 | dataBuffer[2] << 16 | dataBuffer[3] << 24;
		//DEBUGOUT("Vendor length: "); DEBUGOUT2(number32_2, DEC);
		do {
			// read past the vendor string, at most MUSIC_FILE_BUFFER bytes at a time...
			SDFileReadResults=
                (track.read(dataBuffer, number32_2 < MUSIC_FILE_BUFFER ? number32_2 : MUSIC_FILE_BUFFER));
			if (number32_2 > MUSIC_FILE_BUFFER)
                { number32_2 -= MUSIC_FILE_BUFFER; number32_1 -= MUSIC_FILE_BUFFER; }
			else { number32_1 -= number32_2; break; }
		} while (number32_2 > MUSIC_FILE_BUFFER);
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		// VORBIS COMMENTS LIST +++++++++++++++++++++++++++++++++++
		// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++
		// ...get the number of comments
		SDFileReadResults=(track.read(dataBuffer, 4)); number32_1-=4;

		//
		// number32_2 is the number of comments.
		//

		number32_2 =								   // it's in LSB first
			dataBuffer[0] | dataBuffer[1] << 8 | dataBuffer[2] << 16 | dataBuffer[3] << 24;
		//DEBUGOUT("Number of comments: "); DEBUGOUT2(number32_2, DEC);
		while (number32_2 > 0) { // NOTE: there may be no comments, in which case number32_2 == 0!
			SDFileReadResults=(track.read(dataBuffer, 4)); number32_1-=4; // read the length of the comment
			number32_3 =									   // then calculate it (it's in LSB first)
				dataBuffer[0] | dataBuffer[1] << 8 | dataBuffer[2] << 16 | dataBuffer[3] << 24;
			//Serial.print(F("Cur Position: ")); Serial.println(track.curPosition(), DEC);
			//Serial.print(F("Length of comment: ")); Serial.println(number32_3, DEC);

			//
			// number32_3 is the length of the comment.
			//

			// To reiterate: +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			// number32_1 is the length of the entire comments header. Maintain it throughout this section.
			// number32_2 is the length of the comments list.
			// number32_3 is the length of the comment.
			i=0;
			found=true; // optimism...
//#error  MIKE: Analyze carefully the compare dataBuffer contents to the fieldName...
			for (;;) {										 	// get comment field name
				SDFileReadResults=(track.read(&dataBuffer[i], 1)); number32_1-=1; number32_3-=1; // read the next entry
				//Serial.write(dataBuffer[i]); Serial.print("-"); Serial.println(i, DEC);
				//Serial.write('>'); Serial.write(dataBuffer[i]); // !!!!!!!!!!!!!!!!!!!!!!
				if (fieldName[i]==0) break; // found it up to the length of fieldName[].
				if (dataBuffer[i] == '=') break; // end of the comment name
				if (tolower(dataBuffer[i])!=tolower(fieldName[i])) // field name is case-insensitive
                    found=false;
				i++;
			}
			while (dataBuffer[i] != '=') { // slurp in the remaining fieldName, if any.
				i++; SDFileReadResults=(track.read(&dataBuffer[i], 1));
				number32_1--; number32_3--;
			}
			//Serial.print(F("Found field: "));
			//writeAudioDataBuffer(i); // !!!!!!!!!!!!!!!!!!!!!!

			// Now get the field data...
			if (number32_3 < (MUSIC_FILE_BUFFER-1)) { musicFileFrameToRead=number32_3; }
			else musicFileFrameToRead=MUSIC_FILE_BUFFER-1;
			SDFileReadResults=
                (track.read(dataBuffer, musicFileFrameToRead)); // read the field, up to MUSIC_FILE_BUFFER-1
			//writeAudioDataBuffer(musicFileFrameToRead); // !!!!!!!!!!!!!!!!!!!!!!
			number32_1-= musicFileFrameToRead; number32_3-=musicFileFrameToRead;
			if (found) { dataBuffer[musicFileFrameToRead]=0; track.seekSet(0); return(musicFileFrameToRead); } // found? terminate the string with \0 and return
			track.seekCur(number32_3); number32_1-=number32_3; // slurp in the remainder.
			number32_2--;
		}
	}
	// ID3v2/file identifier   "ID3"			3 bytes
	// ID3v2 version           $03 00			2 bytes
	// ID3v2 flags             %abc00000		1 byte
	// ID3v2 size              4 * %0xxxxxxx	4 bytes
	
	// ID3v2.3 frame:
	// Frame ID       $xx xx xx xx (four characters)
	// Size           $xx xx xx xx
	// Flags          $xx xx
	// The frame ID is followed by a size descriptor, making a total header size of ten bytes in every frame.
	// The size is calculated as frame size excluding frame header (frame size - 10).
	else if (dataBuffer[0]=='I') {					   // ID3. Found at the beginning the file assumes it's ID3v2.
		SDFileReadResults=(track.read(dataBuffer, 9)); // the rest of the 'ID3v2' header
		audioType=MP3;
		number32_1=dataBuffer[5] << 21 | dataBuffer[6] << 14 | dataBuffer[7] << 7 | dataBuffer[8];
		if (dataBuffer[4] & B01000000) { // extended header
			SDFileReadResults=(track.read(dataBuffer, 6)); // read the extended header size
			number32_2=dataBuffer[3]; // It will be either 0x06 or 0x0a; if the latter, read the rest of the
			if (number32_2 == (uint32_t)0x0a) SDFileReadResults=(track.read(dataBuffer, 4)); // extended header
		}
		while (number32_1 > 0) {
			// To reiterate: +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			// number32_1 is the length of the entire frames header. Must maintain it throughout this section.
			// number32_1 is the length of the current frame.
			SDFileReadResults=(track.read(dataBuffer, 10)); // the next ID3 frame
			number32_1-=10;
			number32_2=dataBuffer[4] << 24 | dataBuffer[5] << 16 | dataBuffer[6] << 8 | dataBuffer[7];
			found=true; musicFileFrameToRead=0;
			for (i=0; i<4; i++) {										 	// get comment field name
				//Serial.write(dataBuffer[i]); Serial.print("-"); Serial.println(i, DEC);
				//Serial.write('>'); Serial.write(dataBuffer[i]); // !!!!!!!!!!!!!!!!!!!!!!
				if (dataBuffer[i]!=fieldName[i]) { found=false; break; } // field name is upper case
			}
			if (found) { 
				//Serial.print(F("Found field: "));
				//writeAudioDataBuffer(i); // !!!!!!!!!!!!!!!!!!!!!!
				if (number32_2 >= MUSIC_FILE_BUFFER) musicFileFrameToRead = MUSIC_FILE_BUFFER-1; else musicFileFrameToRead=(uint8_t)number32_2;
				//Serial.print(F("Bytes to Read: ")); Serial.println(musicFileFrameToRead, DEC);
				do {
					SDFileReadResults=(track.read(dataBuffer, 1)); // read past leading 0's.
					musicFileFrameToRead--;
				} while ((dataBuffer[0] == 0) && (musicFileFrameToRead != 0));
				//Serial.print(F("Bytes to Read: ")); Serial.println(musicFileFrameToRead, DEC);
				SDFileReadResults=(track.read(&dataBuffer[1], (uint16_t)musicFileFrameToRead)); // read the ID3 frame
				dataBuffer[musicFileFrameToRead+1]=0;
				Serial.print(F("Read: ")); Serial.println((char *) dataBuffer);
				number32_1-=musicFileFrameToRead; number32_2-=musicFileFrameToRead;
				track.seekSet(0); return(musicFileFrameToRead); // found? terminate the string with \0 and return
			}
			number32_1-=number32_2;
			track.seekCur(number32_2); // jump to the next ID3 frame
		}
	}
	track.seekSet(0); return(0);
}

// Completely UNTESTED!!! This is a leftover from the old code. Feel free to reference it for learning.
// Feel even freer to test it and make it work :-).  -Mike 11/3/2012
void SFAudioShield::getAudioInfo(AudioInfo* aInfo)
{
    // volume calculation        
    uint16_t hdat0 = sci_read(SCI_HDAT0);
    uint16_t hdat1 = sci_read(SCI_HDAT1);
     
    //DEBUGOUT("SFAudioShieldb: Audio info\r\n");        
     
    AudioInfo* retVal = aInfo;
    retVal->type = UNKNOWN;        
     
    if (hdat1 == 0x7665)
    {
        // audio is WAV
        retVal->type = WAV;
    }  
    else if (hdat1 == 0x4154 || hdat1 == 0x4144 || hdat1 == 0x4D34 )
    {
        // audio  is AAC
        retVal->type = AAC;
    }
    else if (hdat1 == 0x574D )
    {
        // audio  is WMA
        retVal->type = WMA;
    }
    else if (hdat1 == 0x4D54 )
    {
        // audio  is MIDI
        retVal->type = MIDI;
    }
    else if (hdat1 == 0x4F76 )
    {
        // audio  is OGG VORBIS
        retVal->type = OGG_VORBIS;
    }
    else if (hdat1 >= 0xFFE0 &&  hdat1 <= 0xFFFF)
    {
        // audio  is mp3
        retVal->type = MP3;
         
        //DEBUGOUT("SFAudioShieldb:   Audio is mp3\r\n");        
        retVal->ext.mp3.id =      (MP3_ID)((hdat1 >>  3) & 0x0003);
        switch((hdat1 >>  1) & 0x0003)
        {
        case 3:
            retVal->ext.mp3.layer = 1;
            break;
        case 2:
            retVal->ext.mp3.layer = 2;
            break;
        case 1:
            retVal->ext.mp3.layer = 3;
            break;
        default:
            retVal->ext.mp3.layer = 0;
            break;
        }        
        retVal->ext.mp3.protectBit =    (hdat1 >>  0) & 0x0001;                
         
        char srate =    (hdat0 >> 10) & 0x0003;       
        retVal->ext.mp3.kSampleRate = pgm_read_byte(&_sampleRateTable[retVal->ext.mp3.id][srate]);
         
        retVal->ext.mp3.padBit =         (hdat0 >>  9) & 0x0001;
        retVal->ext.mp3.mode =(MP3_MODE)((hdat0 >>  6) & 0x0003);
        retVal->ext.mp3.extension =      (hdat0 >>  4) & 0x0003;
        retVal->ext.mp3.copyright =      (hdat0 >>  3) & 0x0001;
        retVal->ext.mp3.original =       (hdat0 >>  2) & 0x0001;
        retVal->ext.mp3.emphasis =       (hdat0 >>  0) & 0x0003;
         
        //DEBUGOUT("SFAudioShieldb:  ID: %i, Layer: %i, Samplerate: %i, Mode: %i\r\n", retVal->ext.mp3.id, retVal->ext.mp3.layer, retVal->ext.mp3.kSampleRate, retVal->ext.mp3.mode);        
    }
     
    // read byteRate
    uint16_t byteRate = wram_read(para_byteRate);
    retVal->kBitRate = (byteRate * 8) / 1000;
    //DEBUGOUT("SFAudioShieldb:  BitRate: %i kBit/s\r\n", retVal->kBitRate);
     
    // decode time
    retVal->decodeTime = sci_read(SCI_DECODE_TIME);    
    //DEBUGOUT("SFAudioShieldb:  Decodetime: %i s\r\n", retVal->decodeTime);
                   
}
