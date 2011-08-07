This fork contains some matching optimizations, where safe.  Specifically:

1. Adjacent character class alternates are merged together.  A single-character string
is considered to be a character class with 1 element for purposes of this optimization
(because it is).

Eg::

Rule <- "a" / "b" / "c" / [xyz] 
is equivalent to:
Rule <- [abcxyz]

2. Elimination of impossible-to-match alternate strings.  This is primarily to warn
the user as well as to simplify for optimization 3.

Eg::

Rule <- "a" / "aa"

"aa" can never match because "a" will match first.


3. switch-based alternate string matches

Eg::

Rule <- "cat" / "dog" / "goat" / ...

is matched in order, requiring N string matches to fail.

::

if (stringMatch("cat")) return 1;
if (stringMatch("dog")) return 1;
if (stringMatch("goat")) return 1;
return 0;

This can compiled into a switch table::

switch (yybuf[yypos++])
{
case 'c': if (stringMatch("at")) return 1; else goto fail;
case 'd': if (stringMatch("og")) return 1; else goto fail;
case 'g': if (stringMatch("oat")) return 1; else goto fail;
default:
  goto fail;
}



--- previous readme ---

This is my (nddrylliog) fork of Ian Piumarta's peg/leg, with a small but major bugfix:
make the parse stack size adjustable by #define(s). It's needed for grammars that are
the least bit complicated ;)
