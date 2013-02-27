/*
 * In vi:  :set ts=2 sw=2
 * THIS IS ONLY MY TEST SHIT. THIS IS NOT ISABELLAS KITCHEN
 * TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST TEST
 */

#include <SPI.h>
//Add the SdFat Libraries
#include <SdFat.h>
#include <SdFatUtil.h>
#include <SFAudioShield.h>


// To use this example:
// I have it set up to work with any of 3 types of music files: OGG, MP3, or AAC (e.g. from iTunes).
// But it won't recognize file comments in AAC files (see below).
//
// 1. Decide if you want the music file to play by the VS1053 interrupting the Arduino, or if you want
// the sketch to be responsible for keeping the music data flowing.  Set USE_INTERRUPT appropriately:
#define USE_INTERRUPT true
SFAudioShield AudioPlayer(USE_INTERRUPT); // creates the SFAudioShield object.
//
// 2. Put music file(s) on an SD card. I use a FAT-32 formatted, 4 Gig micro sd card.
// 3. Go down to the section that begins with #define AAC_TEST. Choose which file type you want to play.
// 4. Go down to the section that says "File Definitions". Define your filename (in 7.3 notation).
// 5. Define a description of your file.
// 6. If playing Ogg or MP3, define the fields that you want to print out. I have 4 there as an example.
// Change the fields, add fields, do fewer fields- however you'd like.
// I did not do the work to recognize AAC files. If you want it, you'll have to figure it out.
// If you do, send me your patches and I'll include them in the library!
// Otherwise, I may do it myself. But probably not.
//
// Once done with all that, upload the sketch, and watch it play.

// ******** Which test? Uncomment one of these *********** //
// ******** Which test? Uncomment one of these *********** //
// ******** Which test? Uncomment one of these *********** //
#define AAC_TEST // for m4a
//#define MP3_TEST
//#define OGG_TEST
// ******** Which test? Uncomment one of those *********** //
// ******** Which test? Uncomment one of those *********** //
// ******** Which test? Uncomment one of those *********** //

// Do you want to display the fields (comments)?
#undef DO_COMMENTS
#define FIELDCOUNT 4

/*
 *
 * Analysis and calculations of some old Serial output.
HOLA Baby
01: Goo Goo Dolls mp3					// setup() for loop.
playMP3 called								// playMP3()
play() called.								// SFAudioShield::play()
36														// SFAudioShield::play(), millis before datarequesthandler
95														// SFAudioShield::play(), millis after datarequesthandler.
															// ...is 58 millis to load 179 * 32 == 5728 bytes.
															// ...is 0.32 millis (320 uS) to load 32 bytes.
															// 4.16 millis to load 32 bytes * 13 calls.
 pCOUNT: 179									// SFAudioShield::play(). Count of times datarequesthandler
 															// was called
PLAY:GO!!! 32									// SFAudioShield::play(). 32 == SDFileReadResults
START; 												// playMP3()
 ^=5728t:98										// playMP3() ^=bytes for this track since play(). millis()
 															// 					 at beginning of loop.
COUNT: 179										// playMP3() call count of dataRequestHandler. reset 
t:99:t												// playMP3() if we've just uploaded some more bytes, millis()

NOTE: FROM ABOVE, dataRequestHandler takes 58 millis to load 179 * 32 == 5728 bytes.
HOLA Baby
01: Goo Goo Dolls mp3
playMP3 calledplay() called.
TCNT1 readings. Each tick is 65 micros. TCNT1 is reset now.
TCNT1:3
TCNT1:820 (after the first datarequesthandler() call) ==> 817 * .064 millis= 52.288 millis.
 pCOUNT: 179 PLAY:GO!!! 32TCNT1:868
START; 
^ Calls:0^ Calls:14^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:0^ Calls:13^ Calls:14^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:14^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13^ Calls:13Stop because more than 1000 millis.
SFAudioShield: Song successfully stopped.
ERROR: **** cancel(): HDAT1 not 0. ***********
CANCELLED :-)

...That shows that there are generally 13 fileXferBlock() calls for every interrupt that takes place.

*/

// *****************************************
// File Definitions
// *****************************************
char *ogg_filename="loseyrsf.ogg";
#define OGG_DESCR F("Lose Yourself ogg")

// *****************************************
char *mp3_filename="track001.mp3";
#define MP3_DESCR F("Goo Goo Dolls mp3")
//char *mp3_filename="water4.mp3";
//#define MP3_DESCR F("Water4 mp3")

// *****************************************
char *aac_filename="unblvbl.m4a";
#define M4A_DESCR F("Unbelievable AAC (m4a)")


// *****************************************
#ifdef OGG_TEST
#define DESCRIPTION OGG_DESCR
const char strAlbum[] PROGMEM ="Album";
const char strTitle[] PROGMEM ="Title";
const char strArtist[] PROGMEM ="Artist";
const char strDate[] PROGMEM ="Date";
#endif
#ifdef MP3_test
#define DESCRIPTION MP3_DESCR
const char strAlbum[] PROGMEM ="TALB";
const char strTitle[] PROGMEM ="TIT2";
const char strArtist[] PROGMEM ="TPE1";
const char strDate[] PROGMEM ="TYER";
#else
#ifndef OGG_TEST // AAC HERE
#define DESCRIPTION M4A_DESCR
const char strAlbum[] PROGMEM ="DOESNT"; // AAC atom thingies. Currently this is useless. -Mike 2/1/2013
const char strTitle[] PROGMEM ="WORK";
const char strArtist[] PROGMEM ="IS";
const char strDate[] PROGMEM ="USELESS";
#endif
#endif
const char* fields[] PROGMEM = { strDate, strAlbum, strTitle, strArtist };
char aField[6];

void playMP3(char *trackname, uint8_t durationSeconds) {
	uint32_t start, now, temp, bytes, lastTransfer;
	Serial.print(F("playMP3 called "));
	Serial.println(trackname); //Serial.print(F(" play for: "));
	//Serial.println(durationSeconds, DEC); Serial.flush();
	if (durationSeconds > 65) durationSeconds=65;
	bytes=0;
	uint16_t duration=durationSeconds * 1000;
	// Set TIMER1 *************************
	// Prescalar: 1024; therefore time per tick is: 16M/1024==64uS
	TCNT1=0x0000;
	int8_t result=AudioPlayer.start(trackname); // ****** PLAY **************

	// DISPLAY COMMENTS +++++++++++++++++++++++++++++++++++++++++++++++
#ifdef DO_COMMENTS
	for (uint8_t i=0; i < FIELDCOUNT; i++) {
		strcpy_P(aField, (char *) pgm_read_word(&fields[i]));
		Serial.print(F("++++++++++++++++++++++++ Looking for Field: ")); Serial.println(aField);
		result=AudioPlayer.getFileComments(aField);
		if (result > 0) {Serial.print(aField); Serial.println((char *) AudioPlayer.getAudioDataBuffer());}
		else { Serial.print(F("NO ")); Serial.println(aField); }
		delay(500);
	}
	delay(500);
#endif
	// GET COMMENTS +++++++++++++++++++++++++++++++++++++++++++++++++++

	result=AudioPlayer.play();					 // ****** PLAY **************
	AudioPlayer.callCount=0;
	//Serial.print(F(" OFC:")); Serial.print(AudioPlayer.timer0OFCounter);
	now=millis();
	start=now;
	lastTransfer=now;
  if(result != 1) {
		Serial.print(F("Error trying to start Audio player. Result: "));
		Serial.println(result, DEC); Serial.flush();
		return;
	}
	if (AudioPlayer.fileError) { Serial.println("File Error"); Serial.flush(); AudioPlayer.cancel(); return; }
	Serial.println(F("START; ")); Serial.flush();
	if (duration == 0) {
		Serial.println(F("playMP3() returns; let the music play on!")); Serial.flush();
		return;
	}

	/****************************************************************************************/
	for (;;) {
		//Serial.print(F(" OFC:")); Serial.print(AudioPlayer.timer0OFCounter);
		//Serial.print(F(" TCNT1:")); Serial.print(TCNT1, DEC);
		now=millis();
		//Serial.print("."); Serial.print(TCNT1, DEC);
		temp=AudioPlayer.byteCount;
		if (bytes != temp)  {
			//Serial.print("^");
			//Serial.print(" Calls:"); Serial.print(AudioPlayer.callCount, DEC);
			//Serial.print(temp, DEC);
			//Serial.print("t:"); Serial.print(now, DEC);
			//Serial.print(F(" OFC:")); Serial.print(AudioPlayer.timer0OFCounter);
			//AudioPlayer.timer0OFCounter=0;
			AudioPlayer.callCount=0;
		}
		if (! AudioPlayer.isPlaying) {
			Serial.print(F(" BREAK: count: "));
			Serial.println(AudioPlayer.byteCount, DEC); Serial.flush();
			break;
		}
		if (duration != 0) {
			if ((now - start) > duration) {
				Serial.print(F("Stop because more than ")); Serial.print(duration, DEC);
				Serial.println(F(" millis.")); Serial.flush();
				AudioPlayer.cancel();
				return;
			}
			//Serial.print("."); Serial.print(TCNT1, DEC);
		}
		if (bytes != temp)  {
			//Serial.print("."); Serial.print(temp, DEC);
			//Serial.print(" t:"); Serial.print(millis(), DEC); Serial.print(":t ");
			//Serial.print(F("TCNT1:")); Serial.print(TCNT1, DEC);
			bytes=temp;
		}
		if (! USE_INTERRUPT) SFAudioShield::loadVS1053FIFO(false);
		//Serial.println("");
	}
	/****************************************************************************************/
}
// MPEG-1 and MPEG-2 Audio Layer III available sampling rates (Hz)
// MPEG-1						MPEG-2
// Audio Layer III		Audio Layer III
// -									16000 Hz
// -									22050 Hz
// -									24000 Hz
// 32000 Hz					-
// 44100 Hz					-
// 48000 Hz					-

//#define LOOPVOLUME DEFAULT_VOLUME
#define LOOPVOLUME 30

// ================================================================
// === SETUP ======================================================
// ================================================================
void setup() {
	uint8_t result;

	pinMode(MYLED, OUTPUT);
  Serial.begin(115200);
	TCCR1A=0; TCCR1B=0; // Turns counter 1 off
	TCNT1=0x0000;
	TCCR1B=0x5; // Turns counter on, prescaler 1024.
	Serial.println("HOLA Baby");
  // === LEDs ==================================================
	// printout:
	// HOLA Baby
	// Pin: 10 Port addr: 0x25 mask: 100 not mask: 11111011
  //leds.setup();
  
	// Example of accessing prog_char, from Print:
	/*
size_t Print::print(const __FlashStringHelper *ifsh)
{
  const prog_char *p = (const prog_char *)ifsh;
  size_t n = 0;
  while (1) {
    unsigned char c = pgm_read_byte(p++);
    if (c == 0) break;
    n += write(c);
  }
  return n;
}
*/

  // === MP3 ===================================================
	if ((result=AudioPlayer.init()) != 1) {
		Serial.print(F("Error trying to start MP3 player. Result: "));
		Serial.println(result, DEC);
		//leds.scroll(F("Error trying to start MP3 player."), 200, 1);
	} else {
		//
		// OPTIONAL files to play. If you uncomment this, it will play regardless
		// of the setting of of the TEST #defines, above
		//
		//AudioPlayer.sineTestActivate(3); // FsIDX=44100; S=6, so 44100 * 3 / 128 == 1033 Hz
		//delay(2000);
		//AudioPlayer.sineTestDeactivate();

	  AudioPlayer.setVolume(LOOPVOLUME);
		for (uint8_t i=0; i < 2; i++) {
			Serial.println(i, DEC);
			Serial.println(F("1: Goo Goo Dolls mp3"));
			playMP3("track001.mp3", 1);
			Serial.println(F("2: Lose Yourself ogg"));
			playMP3("loseyrsf.ogg", 2);
			Serial.println(F("1: Unbelievable AAC"));
			playMP3("unblvbl.m4a", 1);
			Serial.println(F("2: Gold ogg"));
			playMP3("gold.ogg", 2);
			Serial.println(F("2: Torch ogg"));
			playMP3("torch.ogg", 2);
			Serial.println(F("2: Wesnoth ogg"));
			playMP3("wesnoth.ogg", 2);
		}

	}
	//Serial.println("DONE tracking bytes");

  // === BEGIN =================================================
	//leds.scroll(F("...HOLA Baby...!"), 200, 1);
	//Serial.println(F("...HOLA Baby...!"));
	//Serial.println(F("Begin"));
}

void delayit(uint16_t deciseconds) {
	for (uint16_t i=0; i < deciseconds; i++) {
    if (! AudioPlayer.isPlaying) break;
		Serial.print("_");
		delay(50);
		//leds.scroll(F("o"), 10, 1);
		Serial.print("+");
		delay(50);
		//leds.scroll(F("o"), 10, -1);
	}
}

bool printed_u1=false;
bool printed_u2=false;
bool printed_c=false;
uint8_t playspeed=1;
uint8_t playiterations=0;
uint8_t terminateState=0;
// ================================================================
// === LOOP =======================================================
// === This keeps going endlessly, even if there's no song playing.
// ================================================================
void loop() {
theloop:
    if (! AudioPlayer.isPlaying)  {
	    AudioPlayer.setVolume(LOOPVOLUME);
      playiterations=0; playspeed=1;
		  Serial.print(F("GO: "));
		  Serial.println(DESCRIPTION);
#ifdef OGG_TEST
		  playMP3(ogg_filename, 0); // starts it up and returns immediately after filling buffer.
#endif
#ifdef MP3_TEST
		  playMP3(mp3_filename, 0); // starts it up and returns immediately after filling buffer.
#endif
#ifdef AAC_TEST
		  playMP3(aac_filename, 0); // starts it up and returns immediately after filling buffer.
#endif
  }
	if (! USE_INTERRUPT) {
		SFAudioShield::loadVS1053FIFO(false);
		if (millis() & 0x0100) { if (! printed_u1) { Serial.print("_"); printed_u1=true; } }
		else { printed_u1=false; }
		if (millis() & 0x0200) { if (! printed_u2) { Serial.print("^"); printed_u2=true; }}
		else { printed_u2=false; }
		if (millis() & 0x2000) {
      if (! printed_c) { Serial.println(F("Playing, non-interrupt style!")); printed_c=true; }
    } else { printed_c=false; }
	}
	else {
	delayit(20);
	AudioPlayer.setTrebleFrequency(3);
	for (int8_t i=0; i > -9; i--) {
    if (! AudioPlayer.isPlaying) break;
		Serial.println(F("Less Trebly"));
		AudioPlayer.setTrebleAmplitude(i);
		delayit(10);
	}
	for (int8_t i=-9; i < 8; i++) {
    if (! AudioPlayer.isPlaying) break;
		Serial.println(F("More Trebly"));
		AudioPlayer.setTrebleAmplitude(i);
		delayit(10);
	}
	Serial.println(F("Normal treble"));
	AudioPlayer.setTrebleFrequency(0);
	AudioPlayer.setTrebleAmplitude(0);
  if (! AudioPlayer.isPlaying) goto theloop;
	delayit(20);
	for (uint8_t i=0; i<2; i++) {
		Serial.println(F("Bassier"));
		AudioPlayer.setBassFrequency(15);
		AudioPlayer.setBassAmplitude(15);
		delayit(20);
		Serial.println(F("Normal bass"));
		AudioPlayer.setBassFrequency(0);
		AudioPlayer.setBassAmplitude(0);
		delayit(20);
	}
  if (! AudioPlayer.isPlaying) goto theloop;
	Serial.println(F("Faster"));
	AudioPlayer.setPlaySpeed(2);
	/*delayit(20);
	AudioPlayer.setPlaySpeed(1);
	delayit(20);
	AudioPlayer.setPlaySpeed(4); // Serial.print() loop slows down
	*/
	delayit(20);
	AudioPlayer.setPlaySpeed(1);
	//delayit(40);
	//AudioPlayer.setPlaySpeed(6); // this is hurting, though it works for an MP3. Makes 
															 // Serial.print() chunks go reallllly slow
	int8_t abs_balance=40;
	uint8_t volume, lrdelay=1;
	volume=LOOPVOLUME;
	AudioPlayer.setVolume(volume); AudioPlayer.setBalance(0);
	Serial.print("Volume at balance start: "); Serial.println(volume);
	// At the end, Volumes are the following:  20 for left, 40 for right.
	Serial.println(F("Move to left"));
	for (int8_t i=0; i>=-abs_balance; i--) {
		AudioPlayer.setBalance(i); delayit(lrdelay);  // left is louder
	}
	Serial.print(F("Left is louder, move to right, v:")); Serial.print(AudioPlayer.getVolumeDB()); Serial.print(F(" b:")); Serial.println(AudioPlayer.getBalanceDB());
	for (int8_t i=-abs_balance; i<=abs_balance; i++) {
		AudioPlayer.setBalance(i); delayit(lrdelay);  // right is louder
	}
	Serial.print(F("Right is louder. Move to equal v:")); Serial.print(AudioPlayer.getVolumeDB()); Serial.print(F(" b:")); Serial.println(AudioPlayer.getBalanceDB());
	for (int8_t i=abs_balance; i>=0; i--) {
		AudioPlayer.setBalance(i); delayit(lrdelay);  // back to equal
	}
	Serial.print(F("Back to equal. v:")); Serial.print(AudioPlayer.getVolumeDB()); Serial.print(F(" b:")); Serial.println(AudioPlayer.getBalanceDB());

	Serial.print(F("Volume at balance test end: ")); Serial.println(AudioPlayer.getVolumeDB());
	AudioPlayer.setBalance(0);   	 // do this first
  if (! AudioPlayer.isPlaying) goto theloop;
	AudioPlayer.setVolume(LOOPVOLUME); // from Datasheet:
																 // 	In VS1053b bass and treble initialization and volume change is
																 //		delayed until the next batch of samples are sent to the audio FIFO.
	delayit(20);
  if (! AudioPlayer.isPlaying) goto theloop;
	Serial.println(F("QUIET!!!!"));
	for (uint8_t i=LOOPVOLUME; i < 75; i++) {
		delayit(1);
		AudioPlayer.setVolume(i);
		Serial.print("v");
	}
	Serial.println(F("Louder"));
	for (uint8_t i=75; i >= LOOPVOLUME; i--) {
		delayit(1);
		AudioPlayer.setVolume(i);
		Serial.print("^");
	}
	}
}
