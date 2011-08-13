
#ifdef __linux
#define __USE_GNU
#endif

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "optimize.h"
#include "set.h"
#include "tree.h"


void optimizeAlternate(Node * node);

void optimizeAlternateStrings(Node * node);




char *escape(const char *cp, int length)
{
    int i;

    int count;

    char *str;

    for (i = 0, count = 0; i < length; ++i)
    {
        char c = cp[i];

        switch (c)
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
            if ((c & 0x80) == 0 && isprint(c))
                count++;
            else
                count += 4;
            // octal escape.
        }
    }

    count++;
    // trailing null.

    str = (char *)malloc(count);

    for (i = 0, count = 0; i < length; ++i)
    {
        char c = cp[i];

        switch (c)
        {
        case '\\':
            str[count++] = '\\';
            str[count++] = '\\';
            break;
        case '\'':
            str[count++] = '\\';
            str[count++] = '\'';
            break;
        case '"':
            str[count++] = '\\';
            str[count++] = '"';
            break;
        case '\a':
            str[count++] = '\\';
            str[count++] = 'a';
            break;
        case '\b':
            str[count++] = '\\';
            str[count++] = 'b';
            break;
        case '\e':
            str[count++] = '\\';
            str[count++] = 'e';
            break;
        case '\f':
            str[count++] = '\\';
            str[count++] = 'f';
            break;
        case '\n':
            str[count++] = '\\';
            str[count++] = 'n';
            break;
        case '\r':
            str[count++] = '\\';
            str[count++] = 'r';
            break;
        case '\t':
            str[count++] = '\\';
            str[count++] = 't';
            break;
        case '\v':
            str[count++] = '\\';
            str[count++] = 'v';
            break;
        default:
            if ((c & 0x80) == 0 && isprint(c))
                str[count++] = c;
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


struct LNode
{
    struct LNode *next;
    struct RawString *string;
};

void optimizeAlternateStrings(Node * node)
{
    struct LNode *table[256]; // hash by first character of string.
    unsigned char bits[32];
    
    Node *n;
    Node *prevNode;

    int i;
    int emptyString = 0;

    assert(node);
    assert(node->type == Alternate);

    /*
     * since alternates match in the order listed, a string cannot match
     * if it is a super-string of a previous string.
     * eg:
     * "a" | "aa"
     *
     * This has two functions:
     * 1. identifying errors
     * 2. making table conversion easier.
     *
     */

    memset(table, 0, sizeof(table));
    memset(bits, 0, sizeof(bits));

    prevNode = NULL;
    for (n = node->alternate.first; n;)
    {
        Node *nextNode;
        int t;
        int remove = 0;

        t = n->type;
        nextNode = n->any.next;

        
        remove = 0;

        // todo -- if emptyString, these will all fail.

        switch (t)
        {
        
        case Class:
            charClassOr(bits, n->cclass.bits);
            break;
        
        case Dot:     

            // dot = character class of everything.
            // (probably an error if anything comes afterwards)
            memset(bits, 0xff, sizeof(bits));
            break;

        case Character:
            {
                char c;
            
                c = n->character.cValue;
                
                if (charClassIsSet(bits, c))
                    remove = 1;
                else
                    charClassSet(bits, c);
             }   
             break;

        case String:
            {
                struct RawString *string;
                int c;
                int length;
    
                /*
                 * a string of length 0 will always match, 
                 * so everything afterwards needs to be removed.
                 * memcmp works fine, but the table is hashed by 
                 * the first char, so we need to special case it.
                 */
    
            
                string = n->string.rawString;
                length = string->length;
    
                if (length)
                {
                    c = string->string[0];
                    if (charClassIsSet(bits, c))
                        remove = 1;
                }
                else
                {
                    c = 0;
                    if (emptyString) remove = 1;
                    emptyString = 1;
                }
                
                if (length && !remove)
                {
                    // check previous strings to see if this one should be removed.
                    struct LNode *ln;
    
                    for (ln = table[c]; ln; ln = ln->next)
                    {
                        if (ln->string->length <= length)
                        {
                            if (memcmp(ln->string->string, string->string, ln->string->length) == 0)
                            {
                                remove = 1;
                                break;
                            }
                        }
                    }
                }
                    
                    
                // add it to the table.
                if (!remove)
                {
                    struct LNode *ln;

                    ln = (struct LNode *)malloc(sizeof(struct LNode));
                    ln->next = table[c];
                    ln->string = string;
                    table[c] = ln;
                }                
            }
            break;
        }

        if (remove)
        {
            fprintf(stderr, "Warning: ``%s'' can never be matched\n", n->string.value);
            if (prevNode == NULL)
                node->alternate.first = nextNode;
            // should never happen.
            else
                prevNode->any.next = nextNode;
            freeNode(n);
            n = nextNode;
            continue;
        }
        

        prevNode = n;
        n = nextNode;
        continue;

    }
    node->alternate.last = prevNode;

    // free - up memory
    for (i = 0; i < 256; ++i)
    {
        struct LNode *ln;

        ln = table[i];
        while (ln)
        {
            struct LNode *next = ln->next;

            free(ln);
            ln = next;
        }
    }

}

/*
 * combine [a] / [b] to [ab]
 * (characters are equivalent to a class of one)
 * (only works with adjacent classes)
 */
void optimizeAlternateClass(Node * node)
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
        // TODO -- just set a remove current/remove next flag, one set of removal code @ bottom.
        // TODO -- support for .
        nextNode = n->any.next;

        t1 = n->type;
        t2 = nextNode ? nextNode->type : -1;

        if (t1 == Class && t2 == Class)
        {
            charClassOr(n->cclass.bits, nextNode->cclass.bits);
            n->any.next = nextNode->any.next;
            freeNode(nextNode);

            // prevNode = n;
            // n = n->any.next;
            continue;
        }

        if (t1 == Class && t2 == Character)
        {
            charClassSet(n->cclass.bits, nextNode->character.cValue);
            n->any.next = nextNode->any.next;
            freeNode(nextNode);

            // prevNode = n;
            // n = n->any.next;
            continue;
        }

        if (t1 == Character && t2 == Class)
        {
            // like above, but current node is deleted, prevNode must be
            // updated.
            charClassSet(nextNode->cclass.bits, n->character.cValue);

            freeNode(n);
            n = nextNode;
            if (prevNode)
                prevNode->any.next = n;
            else
                node->alternate.first = n;

            continue;
        }

        if (t1 == Character && t2 == Character)
        {
            Node *newNode = makeClass(NULL);

            charClassSet(newNode->cclass.bits, n->character.cValue);
            charClassSet(newNode->cclass.bits, nextNode->character.cValue);

            // newNode is inserted, both n and nextNode are deleted.

            if (prevNode)
                prevNode->any.next = newNode;
            else
                node->alternate.first = newNode;
            newNode->any.next = nextNode->any.next;

            freeNode(n);
            freeNode(nextNode);
            n = newNode;

            continue;
        }

        // 
        else
            prevNode = n;
        n = n->any.next;
    }
    // reset the last node
    node->alternate.last = prevNode;

}

/*
 * convert adjacents strings (or characters or classes) into a string table 
 *
 */
static int min(int a, int b)
{
    return a < b ? a : b;
}


int STcompare_s(const void *a, const void *b, void *context)
{
    struct RawString *A = *(struct RawString **)a;
    struct RawString *B = *(struct RawString **)b;
    int offset = context ? *((const int *)context) : 0;

    int rv;

    rv = memcmp(A->string + offset, B->string + offset, min(A->length, B->length) - offset);

    if (rv == 0)
        rv = A->length - B->length;
    // if A < B if length(A) < length(B)

    return rv;
}

#if 0
static int STcompare(const void *a, const void *b)
{
    return STcompare_s(a, b, 0);
}
#endif

static int STcompare_s_backwards(void *context, const void *a, const void *b)
{
    return STcompare_s(a, b, context);
}


#if 0
int STcompare(const void *a, const void *b)
{
    struct StringArrayString *A = (struct StringArrayString *)a;

    struct StringArrayString *B = (struct StringArrayString *)b;

    int rv;

    rv = memcmp(A->string, B->string, min(A->length, B->length));

    if (rv == 0)
        rv = A->length - B->length;
    // if A < B if length(A) < length(B)

    return rv;
}
#endif


void STsort(struct StringArray *array)
{
    /*
     * OS X has qsort_r
     * GNU has qsort_r
     * Microsoft has qsort_s
     * C1x draft has qsort_s 
     *
     * All but apple take context as the final parameter.
     * For the sort function, Microsoft and Apple take context as the first parameter,
     * GNU and C1x take it as the last parameter.
     *
     * almost makes one yearn for c++ functors.
     *
     */
    
    #if defined __USE_GNU
    qsort_r(array->strings, array->count, sizeof(struct RawString *), STcompare_s, &array->offset);
    #elif defined __APPLE__
    qsort_r(array->strings, array->count, sizeof(struct RawString *), &array->offset, STcompare_s_backwards);
    #else
    #error "qsort_r / qsort_s needed"
    #endif
}

/*
 * duplicates must be removed first 
 */
void optimizeAlternateStringTable(Node * node)
{
    Node *st;

    unsigned char bits[32];
    Node *n;

    unsigned count = 0;
    int hasCC = 0;
    int hasEmptyString = 0;

    // for now, only kick in if all children are strings, characters, or
    // ranges.

    assert(node);
    assert(node->type == Alternate);

    memset(bits, 0, sizeof(bits));

    for (n = node->alternate.first; n; n = n->any.next)
    {
        int t = n->type;

        if (t == Class)
        {
            charClassOr(bits, n->cclass.bits);
            ++hasCC;
            continue;
        }
        
        if (t == Character)
        {
            ++hasCC;
            charClassSet(bits, n->character.cValue);
            continue;
        }
        
        if (t == Dot)
        {
            // todo -- separate flag since
            // this could be handled as default?
            ++hasCC;
            memset(bits, 0xff, 32);
            continue;
        }

        if (t == String)
        {
            if (n->string.rawString->length == 0) hasEmptyString = 1;
            else ++count;

            continue;
        }

        return;
    }

    if (!count)
        return;
    if (count + hasCC < 2)
        return;

    st = makeStringTable(count);
    if (hasCC)
    {
        st->table.bits = (unsigned char *)malloc(32 * sizeof(char));
        memcpy(st->table.bits, bits, 32);
    }

    st->table.emptyString = hasEmptyString;
    
    count = 0;
    for (n = node->alternate.first; n; n = n->any.next)
    {
        int t = n->type;

        if (t != String)
            continue;

        if (n->string.rawString->length == 0) continue;
        
        //string = unescape(n->string.value, &length);
        st->table.value.strings[count] = n->string.rawString;
        count++;
        
        // prevent freeing.
        n->string.rawString = NULL;
    }


    STsort(&(st->table.value));
    
    // now sort...
    //qsort(st->table.value.strings, count, sizeof(struct RawString *),
    //      STcompare);
    //
    // insert it...
    n = node->alternate.first;
    while (n)
    {
        Node *next = n->any.next;

        freeNode(n);
        n = next;
    }

    node->alternate.first = st;
    node->alternate.last = st;
}


// todo -- add a simplification phase
// eg Node *simplify(Node *node) 
// returns NULL (to delete), node (if unchanged) or new Node
// called to simplify list elements
// eg:
// alternate with no entries -> NULL
// alternate with one entry -> child entry
// single char string -> character
// full class -> dot

void optimize(Node * node)
{
    Node *n;

    if (!node)
        return;

    switch (node->type)
    {
    case Rule:
        return optimize(node->rule.expression);
        break;

    case Sequence:
        for (n = node->alternate.first; n; n = n->alternate.next)
            optimize(n);
        break;

    case Alternate:
        // todo -- pass in/return a pair of start/end nodes
        // to simplify the insertion/removal logic
        
        optimizeAlternateClass(node);
        optimizeAlternateStrings(node);
        optimizeAlternateStringTable(node);

        // now run through a second time, optimizing any children.
        for (n = node->alternate.first; n; n = n->any.next)
            optimize(n);
        break;
    }

}
