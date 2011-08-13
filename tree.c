/*
 * Copyright (c) 2007 by Ian Piumarta All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the 'Software'),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, provided that the above copyright notice(s) and this
 * permission notice appear in all copies of the Software.  Acknowledgement of 
 * the use of this Software in supporting documentation would be appreciated
 * but is not required.
 * 
 * THE SOFTWARE IS PROVIDED 'AS IS'.  USE ENTIRELY AT YOUR OWN RISK.
 * 
 * Last edited: 2007-05-15 10:32:09 by piumarta on emilia 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "tree.h"
#include "set.h"

Node *actions = 0;

Node *rules = 0;

Node *thisRule = 0;

Node *start = 0;

FILE *output = 0;

int actionCount = 0;

int ruleCount = 0;

int lastToken = -1;

static inline Node *_newNode(int type, int size)
{
    Node *node = calloc(1, size);

    node->type = type;
    return node;
}

#define newNode(T)	_newNode(T, sizeof(struct T))

void freeNode(Node * node)
{
    
    if (!node)
        return;
    switch (node->type)
    {
    case Rule:
        free(node->rule.name);
        break;

    case Character:
        free(node->character.value);
        break;

    case String:
        free(node->string.value);
        free(node->string.rawString);
        break;

    case Class:
        free(node->cclass.value);
        break;
        
    case StringTable:
        {
            int i;
            for (i = 0; i < node->table.value.count; ++i)
            {
                free(node->table.value.strings[i]);
            }
        }
        break;
        
    }
    free(node);
}


struct RawString *unescape(const char *cp)
{
    struct RawString *out;

    int l;

    int st = 0;

    int xval = 0;

    char c;

    l = strlen(cp);
    out = (struct RawString *)malloc(sizeof(struct RawString) + l + 1);
    l = 0;

    while ((c = *cp++))
    {

        if (st == 1)
        {
            // after escape.
            st = 0;
            switch (c)
            {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
                st = 2;
                // octal escape
                xval = c - '0';
                break;
            case 'x':
                // hex escape
                st = 3;
                xval = 0;
                break;
            case 'a':
                out->string[l++] = '\a';
                break;
            case 'b':
                out->string[l++] = '\b';
                break;
            case 'e':
                out->string[l++] = '\e';
                break;
            case 'f':
                out->string[l++] = '\f';
                break;
            case 'n':
                out->string[l++] = '\n';
                break;
            case 'r':
                out->string[l++] = '\r';
                break;
            case 't':
                out->string[l++] = '\t';
                break;
            case 'v':
                out->string[l++] = '\v';
                break;

            default:
                out->string[l++] = c;
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
            out->string[l++] = xval;
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
                if (c >= '0' && c <= '9')
                    tmp += c - '0';
                else if (c >= 'a' && c <= 'f')
                    tmp += c + 10 - 'a';
                else if (c >= 'A' && c <= 'F')
                    tmp += c + 10 - 'A';

                if (xval <= 255)
                {
                    xval = tmp;
                    continue;
                }
            }
            out->string[l++] = xval;
            st = 0;
            // drop through.
        }

        if (st == 0)
        {
            if (c == '\\')
                st = 1;
            else
                out->string[l++] = c;
            continue;
        }
    }

    if (st == 2 || st == 3)
    {
        //ends on octal/hex escape
        out->string[l++] = xval;
    }
    out->string[l] = 0;
    // nil terminate
    out->length = l;

    return out;
}




Node *makeRule(char *name)
{
    Node *node = newNode(Rule);

    node->rule.name = strdup(name);
    node->rule.id = ++ruleCount;
    node->rule.flags = 0;
    node->rule.next = rules;
    rules = node;
    return node;
}

Node *findRule(char *name)
{
    Node *n;

    char *ptr;

    for (ptr = name; *ptr; ptr++)
        if ('-' == *ptr)
            *ptr = '_';
    for (n = rules; n; n = n->any.next)
    {
        assert(Rule == n->type);
        if (!strcmp(name, n->rule.name))
            return n;
    }
    return makeRule(name);
}

Node *beginRule(Node * rule)
{
    actionCount = 0;
    return thisRule = rule;
}

void Rule_setExpression(Node * node, Node * expression)
{
    assert(node);

#ifdef DEBUG
    Node_print(node);
    fprintf(stderr, " [%d]<- ", node->type);
    Node_print(expression);
    fprintf(stderr, "\n");
#endif

    assert(Rule == node->type);
    node->rule.expression = expression;
    if (!start || !strcmp(node->rule.name, "start"))
        start = node;
}

Node *makeVariable(char *name)
{
    Node *node;

    assert(thisRule);
    for (node = thisRule->rule.variables; node; node = node->variable.next)
        if (!strcmp(name, node->variable.name))
            return node;
    node = newNode(Variable);
    node->variable.name = strdup(name);
    node->variable.next = thisRule->rule.variables;
    thisRule->rule.variables = node;
    return node;
}

Node *makeName(Node * rule)
{
    Node *node = newNode(Name);

    node->name.rule = rule;
    node->name.variable = 0;
    rule->rule.flags |= RuleUsed;
    return node;
}

Node *makeDot(void)
{
    return newNode(Dot);
}

Node *makeString(char *text)
{
    Node *node;
    struct RawString *string;
    
    text = strdup(text);
    string = unescape(text);

    if (string->length == 1)
    {
        node = newNode(Character);
        node->character.value = text;
        node->character.cValue = string->string[0];
        free(string);
    }
    else
    {
        node = newNode(String);
        node->string.value = text;
        node->string.rawString = string;
    }
    return node;
}

Node *makeClass(char *text)
{
    Node *node = newNode(Class);

    node->cclass.value = text ? (unsigned char *)strdup(text) : NULL;
    charClassMake(text, node->cclass.bits);

    return node;
}

Node *makeAction(char *text)
{
    Node *node = newNode(Action);

    char name[1024];

    assert(thisRule);
    sprintf(name, "_%d_%s", ++actionCount, thisRule->rule.name);
    node->action.name = strdup(name);
    node->action.text = strdup(text);
    node->action.list = actions;
    node->action.rule = thisRule;
    actions = node;
    {
        char *ptr;

        for (ptr = node->action.text; *ptr; ++ptr)
            if ('$' == ptr[0] && '$' == ptr[1])
                ptr[1] = ptr[0] = 'y';
    }
    return node;
}

Node *makePredicate(char *text)
{
    Node *node = newNode(Predicate);

    node->predicate.text = strdup(text);
    return node;
}

Node *makeAlternate(Node * e)
{
    if (Alternate != e->type)
    {
        Node *node = newNode(Alternate);

        assert(e);
        assert(!e->any.next);
        node->alternate.first = node->alternate.last = e;
        return node;
    }
    return e;
}

Node *Alternate_append(Node * a, Node * e)
{
    assert(a);
    a = makeAlternate(a);
    assert(a->alternate.last);
    assert(e);
    a->alternate.last->any.next = e;
    a->alternate.last = e;
    return a;
}

Node *makeSequence(Node * e)
{
    if (Sequence != e->type)
    {
        Node *node = newNode(Sequence);

        assert(e);
        assert(!e->any.next);
        node->sequence.first = node->sequence.last = e;
        return node;
    }
    return e;
}

Node *Sequence_append(Node * a, Node * e)
{
    assert(a);
    a = makeSequence(a);
    assert(a->sequence.last);
    assert(e);
    a->sequence.last->any.next = e;
    a->sequence.last = e;
    return a;
}

Node *makePeekFor(Node * e)
{
    Node *node = newNode(PeekFor);

    node->peekFor.element = e;
    return node;
}

Node *makePeekNot(Node * e)
{
    Node *node = newNode(PeekNot);

    node->peekNot.element = e;
    return node;
}

Node *makeQuery(Node * e)
{
    Node *node = newNode(Query);

    node->query.element = e;
    return node;
}

Node *makeStar(Node * e)
{
    Node *node = newNode(Star);

    node->star.element = e;
    return node;
}

Node *makePlus(Node * e)
{
    Node *node = newNode(Plus);

    node->plus.element = e;
    return node;
}

Node *makeStringTable(int count)
{
    Node *node =
        _newNode(StringTable,
                 sizeof(struct StringTable) +
                 count * sizeof(struct RawString *));

    node->table.value.count = count;
    return node;
}


static Node *stack[1024];

static Node **stackPointer = stack;


#ifdef DEBUG
static void dumpStack(void)
{
    Node **p;

    for (p = stack + 1; p <= stackPointer; ++p)
    {
        fprintf(stderr, "### %d\t", p - stack);
        Node_print(*p);
        fprintf(stderr, "\n");
    }
}

#endif

Node *push(Node * node)
{
    assert(node);
    assert(stackPointer < stack + 1023);

#ifdef DEBUG
    dumpStack();
    fprintf(stderr, " PUSH ");
    Node_print(node);
    fprintf(stderr, "\n");
#endif

    return *++stackPointer = node;
}

Node *top(void)
{
    assert(stackPointer > stack);
    return *stackPointer;
}

Node *pop(void)
{
    assert(stackPointer > stack);

#ifdef DEBUG
    dumpStack();
    fprintf(stderr, " POP\n");
#endif

    return *stackPointer--;
}


static void Node_fprint(FILE * stream, Node * node)
{
    assert(node);
    switch (node->type)
    {
    case Rule:
        fprintf(stream, " %s", node->rule.name);
        break;
    case Name:
        fprintf(stream, " %s", node->name.rule->rule.name);
        break;
    case Dot:
        fprintf(stream, " .");
        break;
    case Character:
        fprintf(stream, " '%s'", node->character.value);
        break;
    case String:
        fprintf(stream, " \"%s\"", node->string.value);
        break;
    case Class:
        fprintf(stream, " [%s]", node->cclass.value);
        break;
    case Action:
        fprintf(stream, " { %s }", node->action.text);
        break;
    case Predicate:
        fprintf(stream, " ?{ %s }", node->action.text);
        break;

    case Alternate:
        node = node->alternate.first;
        fprintf(stream, " (");
        Node_fprint(stream, node);
        while ((node = node->any.next))
        {
            fprintf(stream, " |");
            Node_fprint(stream, node);
        }
        fprintf(stream, " )");
        break;

    case Sequence:
        node = node->sequence.first;
        fprintf(stream, " (");
        Node_fprint(stream, node);
        while ((node = node->any.next))
            Node_fprint(stream, node);
        fprintf(stream, " )");
        break;

    case PeekFor:
        fprintf(stream, "&");
        Node_fprint(stream, node->query.element);
        break;
    case PeekNot:
        fprintf(stream, "!");
        Node_fprint(stream, node->query.element);
        break;
    case Query:
        Node_fprint(stream, node->query.element);
        fprintf(stream, "?");
        break;
    case Star:
        Node_fprint(stream, node->query.element);
        fprintf(stream, "*");
        break;
    case Plus:
        Node_fprint(stream, node->query.element);
        fprintf(stream, "+");
        break;
    default:
        fprintf(stream, "\nunknown node type %d\n", node->type);
        exit(1);
    }
}

void Node_print(Node * node)
{
    Node_fprint(stderr, node);
}

static void Rule_fprint(FILE * stream, Node * node)
{
    assert(node);
    assert(Rule == node->type);
    fprintf(stream, "%s.%d =", node->rule.name, node->rule.id);
    if (node->rule.expression)
        Node_fprint(stream, node->rule.expression);
    else
        fprintf(stream, " UNDEFINED");
    fprintf(stream, " ;\n");
}

void Rule_print(Node * node)
{
    Rule_fprint(stderr, node);
}
