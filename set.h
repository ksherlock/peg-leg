/* Copyright (c) 2007 by Ian Piumarta
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the 'Software'),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, provided that the above copyright notice(s) and this
 * permission notice appear in all copies of the Software.  Acknowledgement
 * of the use of this Software in supporting documentation would be
 * appreciated but is not required.
 * 
 * THE SOFTWARE IS PROVIDED 'AS IS'.  USE ENTIRELY AT YOUR OWN RISK.
 * 
 */

#ifndef __SET_H__
#define __SET_H__

void charClassSet(unsigned char bits[], int c);
void charClassClear(unsigned char bits[], int c);

int charClassIsSet(unsigned char bits[], int c);
int charClassIsClear(unsigned char bits[], int c);

unsigned char* charClassAnd(unsigned char a[], const unsigned char b[]);
unsigned char* charClassOr(unsigned char a[], const unsigned char b[]);
unsigned char* charClassXor(unsigned char a[], const unsigned char b[]);

unsigned char *charClassMake(const char *cclass, unsigned char bits[]);
char *charClassToString(unsigned char bits[]);

#endif
