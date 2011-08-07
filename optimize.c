#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "optimize.h"
#include "set.h"
#include "tree.h"


void optimizeAlternate(Node *node);
void optimizeAlternateStrings(Node *node);


char *unescape(const char *cp, int *length)
{
  char *out;
  int l;
  int st = 0;
  int xval = 0;
  char c;

  l = strlen(cp);
  out = (char *)malloc(l + 1);
  l = 0;

  while ((c = *cp++))
  {

    if (st == 1)
    {
      // after escape.
      st = 0;
      switch(c)
      {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
          st = 2; // octal escape
          xval = c - '0';
          break;
        case 'x': // hex escape
          st = 3;
          xval = 0;
          break;
        case 'a':
          out[l++] = '\a';
          break;
        case 'b':
          out[l++] = '\b';
          break;
        case 'e':
          out[l++] = '\e';
          break;
        case 'f':
          out[l++] = '\f';
          break;
        case 'n':
          out[l++] = '\n';
          break;
        case 'r':
          out[l++] = '\r';
          break;
        case 't':
          out[l++] = '\t';
          break;
        case 'v':
          out[l++] = '\v';
          break;

        default:
          out[l++] = c;
          break;
      }
      continue;
    }
  
    if (st == 2)
    {
      // octal escape.
      if (c >= '0' && c <= '7')
      {
        int tmp;
        tmp = (xval << 3) + c - '0';
        if (tmp <= 255)
        {
          xval = tmp;
          continue;
        }
      }
      out[l++] = xval;
      st = 0;
      // drop through.
    }
  
    if (st == 3)
    {
      // hex escape.
      if (isxdigit(c))
      {
        int tmp;
        tmp = xval << 4;
        if (c >= '0' && c <= '9') tmp += c - '0';
        else if (c >= 'a' && c <= 'f') tmp += c + 10 - 'a';
        else if (c >= 'A' && c <= 'F') tmp += c + 10 - 'A';

        if (xval <= 255)
        {
          xval = tmp;
          continue;
        }
      }
      out[l++] = xval;
      st = 0;
      // drop through.
    }

    if (st == 0)
    {
      if (c == '\\') st = 1;
      else out[l++] = c;
      continue;
    }
  }

  if (st == 2 || st == 3)
  {
    out[l++] = xval;
  }
  out[l] = 0; // nil terminate
  if (length) *length = l;

  return out;
}


char *escape(const char *cp, int length)
{
  int i;
  int count;
  char *str;

  for (i = 0, count = 0; i < length; ++i)
  {
    char c = cp[i];
    switch(c)
    {
      case '\\':
      case '\'':
      case '"':
      case '\a':
      case '\b':
      case '\e':
      case '\f':
      case '\n':
      case '\r':
      case '\t':
      case '\v':
        count += 2;
        break;
      default:
       if ((c & 0x80) == 0 && isprint(c)) count++;
       else count += 4; // octal escape.
    }
  }
 
  count++; // trailing null.
 
  str = (char *)malloc(count);

  for (i = 0, count = 0; i < length; ++i)
  {
    char c = cp[i];
    switch(c)
    {
      case '\\': str[count++] = '\\'; str[count++] = '\\'; break;
      case '\'': str[count++] = '\\'; str[count++] = '\''; break;
      case '"':  str[count++] = '\\'; str[count++] = '"';  break;
      case '\a': str[count++] = '\\'; str[count++] = 'a';  break;
      case '\b': str[count++] = '\\'; str[count++] = 'b';  break;
      case '\e': str[count++] = '\\'; str[count++] = 'e';  break;
      case '\f': str[count++] = '\\'; str[count++] = 'f';  break;
      case '\n': str[count++] = '\\'; str[count++] = 'n';  break;
      case '\r': str[count++] = '\\'; str[count++] = 'r';  break;
      case '\t': str[count++] = '\\'; str[count++] = 't';  break;
      case '\v': str[count++] = '\\'; str[count++] = 'v';  break;
     default:
       if ((c & 0x80) == 0 && isprint(c)) str[count++] = c;
       else
       {
         sprintf(&str[count], "\\%03o", c);
         count += 4;
       }
    }
  }
  str[i] = 0;

  return str;
}


struct LNode {
  struct LNode *next;
  int length;
  char *string;
};

void optimizeAlternateStrings(Node *node)
{
  struct LNode *table[256]; // hash by first character of string.
  unsigned char bits[32];
  Node *n;
  Node *prevNode;
  
  int i;

  assert(node);
  assert(node->type == Alternate);

  /*
   * since alternates match in the order listed, a string cannot match
   * if it is a super-string of a previous string.
   * eg:
   * "a" | "aa"
   * (this is primarily to make conversion to a regex table easier)
   */

  memset(table, 0, sizeof(table));
  memset(bits, 0, sizeof(bits));

  prevNode = NULL;
  for (n = node->alternate.first; n;)
  {
    Node *nextNode;
    char *string;
    struct LNode *ln;

    int c;
    int t;
    int length;
    int remove = 0;

    t = n->type;
    nextNode = n->any.next;
    if (t == Class)
    { 
      charClassOr(bits, n->cclass.bits);
      prevNode = n;
      n = nextNode;
      continue;
    }

    string = NULL;
    length = 0;
    if (t == Character) string = unescape(n->character.value, &length);
    if (t == String) string = unescape(n->string.value, &length);

    if (string == NULL) 
    {
      prevNode = n;
      n = nextNode;
      continue;
    }

    if (length == 0) c = 0; // empty string
    else
    {
      c = string[0];
      if (charClassIsSet(bits, c))
        remove = 1;
    }

    // check all nodes looking for a substring.
    if (!remove) for (ln = table[c]; ln; ln = ln->next)
    {
      if (ln->length <= length)
      {
        if (strncmp(ln->string, string, ln->length) == 0)
        {
          remove = 1;
          break;
        }
      } 
    }

    if (remove)
    {
      fprintf(stderr, "Warning: ``%s'' can never be matched\n", n->string.value);
      if (prevNode == NULL) node->alternate.first = nextNode; // should never happen.
      else prevNode->any.next = nextNode;
      freeNode(n);
      free(string);

      n = nextNode;
    }
    else
    {
      ln = (struct LNode *)malloc(sizeof(struct LNode));
      ln->next = table[c];
      ln->string = string;
      ln->length = length;
      table[c] = ln;
      prevNode = n;
      n = nextNode;
      continue;
    }
  }
  node->alternate.last = prevNode;
 
  // free-up memory
  for (i = 0; i < 256; ++i)
  {
    struct LNode *n;

    n = table[i];
    while (n)
    {  
      struct LNode *next = n->next;
      free(n->string);
      free(n);
      n = next;
    }
  }

}

/*
 * combine [a] / [b] to [ab]
 * (characters are equivalent to a class of one)
 * (only works with adjacent classes)
 */
void optimizeAlternateClass(Node *node)
{
  Node *n;
  Node *prevNode = NULL;
  Node *nextNode = NULL;

  int t1, t2;

  assert(node);
  assert(node->type == Alternate);


  n = node->alternate.first;
  while (n)
  {
    nextNode = n->any.next;

    t1 = n->type;
    t2 = nextNode ? nextNode->type : -1;

    if (t1 == Class && t2 == Class)
    {
      charClassOr(n->cclass.bits, nextNode->cclass.bits);
      n->any.next = nextNode->any.next;
      freeNode(nextNode);

      //prevNode = n;
      //n = n->any.next;
      continue;
    }

    if (t1 == Class && t2 == Character)
    {
      charClassSet(n->cclass.bits, nextNode->character.cValue);
      n->any.next = nextNode->any.next;
      freeNode(nextNode);

      //prevNode = n;
      //n = n->any.next;
      continue;
    }

    if (t1 == Character && t2 == Class)
    {
      //  like above, but current node is deleted, prevNode must be updated.
      charClassSet(nextNode->cclass.bits, n->character.cValue);

      freeNode(n);
      n = nextNode;
      if (prevNode) prevNode->any.next = n;
      else node->alternate.first = n;

      continue;
    }

    if (t1 == Character && t2 == Character)
    {
      Node *newNode = makeClass(NULL);
      charClassSet(newNode->cclass.bits, n->character.cValue);
      charClassSet(newNode->cclass.bits, nextNode->character.cValue);

      // newNode is inserted, both n and nextNode are deleted.

      if (prevNode) prevNode->any.next = newNode;
      else node->alternate.first = newNode;
      newNode->any.next = nextNode->any.next;

      freeNode(n);
      freeNode(nextNode);
      n = newNode;

      continue;
    }

    // else

    prevNode = n;
    n = n->any.next;
  }
  // reset the last node
  node->alternate.last = prevNode;

}

void optimize(Node *node)
{
    Node *n;
    if (!node) return;
    
    switch(node->type)
    {
    case Rule:
        return optimize(node->rule.expression);
        break;
    
    case Sequence:
        for (n = node->alternate.first; n; n = n->alternate.next)
        optimize(n);
        break;
    
    case Alternate:
        optimizeAlternateClass(node);
        optimizeAlternateStrings(node);
        // now run through a second time, optimizing any children.
        for (n = node->alternate.first; n; n = n->any.next);
            optimize(n);
        break;
    }

}