
/* Compiler implementation of the D programming language
 * Copyright (c) 1999-2014 by Digital Mars
 * All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/scope.h
 */

#ifndef DMD_SCOPE_H
#define DMD_SCOPE_H

#ifdef __DMC__
#pragma once
#endif

class Dsymbol;
class ScopeDsymbol;
class Identifier;
class Module;
class Statement;
class SwitchStatement;
class TryFinallyStatement;
class LabelStatement;
class ForeachStatement;
class ClassDeclaration;
class AggregateDeclaration;
class FuncDeclaration;
class UserAttributeDeclaration;
struct DocComment;
class TemplateInstance;

#if IN_LLVM
struct EnclosingHandler;
class AnonDeclaration;
#endif

#if __GNUC__
// Requires a full definition for PROT and LINK
#include "dsymbol.h"
#include "mars.h"
#else
enum LINK;
enum PROT;
#endif

#define CSXthis_ctor    1       // called this()
#define CSXsuper_ctor   2       // called super()
#define CSXthis         4       // referenced this
#define CSXsuper        8       // referenced super
#define CSXlabel        0x10    // seen a label
#define CSXreturn       0x20    // seen a return statement
#define CSXany_ctor     0x40    // either this() or super() was called

#define SCOPEctor           0x0001  // constructor type
#define SCOPEstaticif       0x0002  // inside static if
#define SCOPEfree           0x0004  // is on free list
#define SCOPEstaticassert   0x0008  // inside static assert
#define SCOPEdebug          0x0010  // inside debug conditional

#define SCOPEinvariant      0x0020  // inside invariant code
#define SCOPErequire        0x0040  // inside in contract code
#define SCOPEensure         0x0060  // inside out contract code
#define SCOPEcontract       0x0060  // [mask] we're inside contract code

#define SCOPEctfe           0x0080  // inside a ctfe-only expression
#define SCOPEnoaccesscheck  0x0100  // don't do access checks
#define SCOPEcompile        0x0200  // inside __traits(compile)

struct Scope
{
    Scope *enclosing;           // enclosing Scope

    Module *module;             // Root module
    ScopeDsymbol *scopesym;     // current symbol
    ScopeDsymbol *sds;          // if in static if, and declaring new symbols,
                                // sds gets the addMember()
    FuncDeclaration *func;      // function we are in
    Dsymbol *parent;            // parent to use
    LabelStatement *slabel;     // enclosing labelled statement
    SwitchStatement *sw;        // enclosing switch statement
    TryFinallyStatement *tf;    // enclosing try finally statement
    OnScopeStatement *os;       // enclosing scope(xxx) statement
    TemplateInstance *tinst;    // enclosing template instance
    Statement *enclosingScopeExit; // enclosing statement that wants to do something on scope exit
    Statement *sbreak;          // enclosing statement that supports "break"
    Statement *scontinue;       // enclosing statement that supports "continue"
    ForeachStatement *fes;      // if nested function for ForeachStatement, this is it
    Scope *callsc;              // used for __FUNCTION__, __PRETTY_FUNCTION__ and __MODULE__
    int inunion;                // we're processing members of a union
    int nofree;                 // set if shouldn't free it
    int noctor;                 // set if constructor calls aren't allowed
    int intypeof;               // in typeof(exp)
    bool speculative;            // in __traits(compiles) or typeof(exp)
    VarDeclaration *lastVar;    // Previous symbol used to prevent goto-skips-init

    unsigned callSuper;         // primitive flow analysis for constructors
    unsigned *fieldinit;
    size_t fieldinit_dim;

    structalign_t structalign;       // alignment for struct members
    LINK linkage;          // linkage for external functions

    PROT protection;       // protection for class members
    int explicitProtection;     // set if in an explicit protection attribute

    StorageClass stc;           // storage class
    char *depmsg;               // customized deprecation message

    unsigned flags;

    UserAttributeDeclaration *userAttribDecl;   // user defined attributes

    DocComment *lastdc;         // documentation comment for last symbol at this scope
    size_t lastoffset;          // offset in docbuf of where to insert next dec (for ditto)
    size_t lastoffset2;         // offset in docbuf of where to insert next dec (for unittest)
    OutBuffer *docbuf;          // buffer for documentation output

    static Scope *freelist;
    static Scope *alloc();
    static Scope *createGlobal(Module *module);

    Scope();

    Scope *copy();

    Scope *push();
    Scope *push(ScopeDsymbol *ss);
    Scope *pop();

    Scope *startCTFE();
    Scope *endCTFE();

    void mergeCallSuper(Loc loc, unsigned cs);

    unsigned *saveFieldInit();
    void mergeFieldInit(Loc loc, unsigned *cses);

    Module *instantiatingModule();

    Dsymbol *search(Loc loc, Identifier *ident, Dsymbol **pscopesym);
    Dsymbol *search_correct(Identifier *ident);
    Dsymbol *insert(Dsymbol *s);

    ClassDeclaration *getClassScope();
    AggregateDeclaration *getStructClassScope();
    void setNoFree();
};

#endif /* DMD_SCOPE_H */
