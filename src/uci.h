#ifndef UCI_H
#define UCI_H

/* Runs the UCI (Universal Chess Interface) command loop on stdin/stdout
 * until "quit" is received or stdin closes. Returns a process exit code. */
int runUci(void);

#endif
