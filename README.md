SFAudioShield
=============

Arduino library to control the SparkFun MP3 Player shield audio player (which doesn't just play MP3s).

This library plays Ogg, MP3, and AAC (iTunes m4a) files.

Get the player at https://www.sparkfun.com/products/10628

Requires #define SD_FAT_VERSION 20111205 of the Arduino SdFat library. The Arduino's builtin
SdFat library will likely not work. :-(

Also requires the digitalWriteFast library.

The good news is that these things are easily findable on the web.

The bad news is that this library comes with NO SUPPORT. I am the author of a few Arduino libraries,
such as the PinChangeInt library. For those, I am commited to making them work for people- even
though it takes time away from my other projects. If you want a supported library, see
http://www.billporter.info/2012/01/28/sparkfun-mp3-shield-arduino-library/
That is a very nice effort which inspired me greatly in writing this library. I borrowed some of
his concepts, although this code is based on a different codebase which is IMHO closer to the model
of the VS1053b. Not that it's any better, mind you, it's just that I found Christian Schmiljun's code
more aligned with the letter of the law as written in the VS1053 datasheet, so I decided to follow it
as I wanted to learn more about the capabilities of the VS1053. And I wanted to decode OGG files,
at least.

For this one, I am committed to nothing. No bug fixes, no updates, no nothin'. What I do here is
at my own leisure and for my own project (and pleasure). I don't have time to support this thing
and help you make it work. 

THIS IS A WORK IN PROGRESS. IT'S FILLED WITH CRAZY PRINT STATEMENTS, CRUFT, STREAM-OF-CONSCIOUSNESS
CODE CRAP AND GARBAGE. DON'T DIVE INTO THIS UNLESS YOU ARE PREPARED TO MAKE IT WORK ON YOUR OWN.

But it may be interesting to you, and it may work for you, or it may help you move along in
your own project. That would be great; that's why I put it up on github. But it's not here to
be a nice giftwrapped library.

I need help with this thing, if anything. So if you're willing to contribute updates, fixes,
or any other help at all, that would be great. Let me know with an email.
