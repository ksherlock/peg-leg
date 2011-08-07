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


#include "set.h"
#include <stdio.h>
#include <string.h>

void charClassSet  (unsigned char bits[], int c) { bits[c >> 3] |=  (1 << (c & 7)); }
void charClassClear(unsigned char bits[], int c) { bits[c >> 3] &= ~(1 << (c & 7)); }


int charClassIsSet(unsigned char bits[], int c)
{
  return bits[c >> 3] & ( 1 << (c & 07));
}

int charClassIsClear(unsigned char bits[], int c)
{
  return !charClassIsSet(bits, c);
}

typedef void (*setter)(unsigned char bits[], int c);

unsigned char* charClassOr(unsigned char a[], const unsigned char b[])
{
  unsigned i;

  for (i = 0; i < 32; ++i)
  {
    a[i] |= b[i];
  }
  return a;
}

unsigned char* charClassAnd(unsigned char a[], const unsigned char b[])
{
  unsigned i;

  for (i = 0; i < 32; ++i)
  {
    a[i] &= b[i];
  }
  return a;
}

unsigned char* charClassXor(unsigned char a[], const unsigned char b[])
{
  unsigned i;

  for (i = 0; i < 32; ++i)
  {
    a[i] ^= b[i];
  }
  return a;
}


unsigned char *charClassMake(const char *cclass, unsigned char bits[])
{
  setter set;
  int c, prev= -1;

  if (!cclass)
  {
     memset(bits, 0, 32);
     return bits;
  }

  if ('^' == *cclass)
    {
      memset(bits, 255, 32);
      set= charClassClear;
      ++cclass;
    }
  else
    {
      memset(bits, 0, 32);
      set= charClassSet;
    }
  while ((c= *cclass++))
    {
      if ('-' == c && *cclass && prev >= 0)
        {
          for (c= *cclass++;  prev <= c;  ++prev)
            set(bits, prev);
          prev= -1;
        }
      else if ('\\' == c && *cclass)
        {
          switch (c= *cclass++)
            {
            case 'a':  c= '\a'; break;  /* bel */
            case 'b':  c= '\b'; break;  /* bs */
            case 'e':  c= '\e'; break;  /* esc */
            case 'f':  c= '\f'; break;  /* ff */
            case 'n':  c= '\n'; break;  /* nl */
            case 'r':  c= '\r'; break;  /* cr */
            case 't':  c= '\t'; break;  /* ht */
            case 'v':  c= '\v'; break;  /* vt */
            default:            break;
            }
          set(bits, prev= c);
        }
      else
        set(bits, prev= c);
    }

    return bits;
}


char *charClassToString(unsigned char bits[])
{
  static char string[256];
  int c;
  char *ptr;


  for (c = 0, ptr = string; c < 32; ++c)
  {
    ptr += sprintf(ptr, "\\x%02x", bits[c]);
  }

  return string;
}

