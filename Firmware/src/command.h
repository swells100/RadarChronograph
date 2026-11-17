// command.h - tiny text command interpreter (USB serial and the TCP link)
//
//   arm | a        arm for ARM_DURATION_MS (again while armed: restart the timer)
//   disarm | d     disarm now
//   status | s | ? print the current state
//   band           show the band-pass filter edges
//   band LOW HIGH  set them in Hz (0 = no limit on that side), e.g.  band 3000 300000
//   band off       filter off
//   stream usb     send the data packets over USB serial (until the port closes)
//   stream off     back to plain text on USB serial
//
// Commands end with a newline, or are taken after 50 ms without further
// characters, so a bare "a" typed in a terminal without a line ending works.
#pragma once
#include <stdint.h>

// Feed one received character (from either source).
void command_feed(int c, int source);   // source: a CmdSource value

// Call every loop(): runs an unterminated command after a short pause.
void command_poll();
