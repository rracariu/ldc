
/* Compiler implementation of the D programming language
 * Copyright (c) 1999-2014 by Digital Mars
 * All Rights Reserved
 * written by Walter Bright
 * http://www.digitalmars.com
 * Distributed under the Boost Software License, Version 1.0.
 * http://www.boost.org/LICENSE_1_0.txt
 * https://github.com/D-Programming-Language/dmd/blob/master/src/builtin.c
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>                     // strcmp()
#include <math.h>

#include "mars.h"
#include "declaration.h"
#include "attrib.h"
#include "expression.h"
#include "scope.h"
#include "mtype.h"
#include "aggregate.h"
#include "identifier.h"
#include "id.h"
#include "module.h"
#if IN_LLVM
#include "template.h"
#include "gen/pragma.h"
#endif

StringTable builtins;

void add_builtin(const char *mangle, builtin_fp fp)
{
    builtins.insert(mangle, strlen(mangle))->ptrvalue = (void *)fp;
}

builtin_fp builtin_lookup(const char *mangle)
{
    if (StringValue *sv = builtins.lookup(mangle, strlen(mangle)))
        return (builtin_fp)sv->ptrvalue;
    return NULL;
}

Expression *eval_unimp(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    return NULL;
}

Expression *eval_sin(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKfloat64);
    return new RealExp(loc, sinl(arg0->toReal()), arg0->type);
}

Expression *eval_cos(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKfloat64);
    return new RealExp(loc, cosl(arg0->toReal()), arg0->type);
}

Expression *eval_tan(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKfloat64);
    return new RealExp(loc, tanl(arg0->toReal()), arg0->type);
}

Expression *eval_sqrt(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKfloat64);
    return new RealExp(loc, Port::sqrt(arg0->toReal()), arg0->type);
}

Expression *eval_fabs(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKfloat64);
    return new RealExp(loc, fabsl(arg0->toReal()), arg0->type);
}

#if IN_LLVM

static inline Type *getTypeOfOverloadedIntrinsic(FuncDeclaration *fd)
{
    // Depending on the state of the code generation we have to look at
    // the template instance or the function declaration.
    assert(fd->parent && "function declaration requires parent");
    TemplateInstance* tinst = fd->parent->isTemplateInstance();
    if (tinst)
    {
        // See DtoOverloadedIntrinsicName
        assert(tinst->tdtypes.dim == 1);
        return static_cast<Type*>(tinst->tdtypes.data[0]);
    }
    else
    {
        assert(fd->type->ty == Tfunction);
        TypeFunction *tf = static_cast<TypeFunction *>(fd->type);
        assert(tf->parameters->dim >= 1);
        return tf->parameters->data[0]->type;
    }
}

static inline int getBitsizeOfType(Loc loc, Type *type)
{
    switch (type->toBasetype()->ty)
    {
      case Tint64:
      case Tuns64: return 64;
      case Tint32:
      case Tuns32: return 32;
      case Tint16:
      case Tuns16: return 16;
      case Tint128:
      case Tuns128:
          error(loc, "cent/ucent not supported");
          break;
      default:
          error(loc, "unsupported type");
          break;
    }
    return 32; // in case of error
}

Expression *eval_cttz(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Type* type = getTypeOfOverloadedIntrinsic(fd);

    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKint64);
    uinteger_t x = arg0->toInteger();

    int n = getBitsizeOfType(loc, type);

    if (x == 0)
    {
        if ((*arguments)[1]->toInteger())
            error(loc, "llvm.cttz.i#(0) is undefined");
    }
    else
    {
        int c = n >> 1;
        n -= 1;
        const uinteger_t mask = (static_cast<uinteger_t>(1L) << n) 
                                | (static_cast<uinteger_t>(1L) << n)-1;
        do {
            uinteger_t y = (x << c) & mask; if (y != 0) { n -= c; x = y; }
            c = c >> 1;
        } while (c != 0);
    }

    return new IntegerExp(loc, n, type);
}

Expression *eval_ctlz(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Type* type = getTypeOfOverloadedIntrinsic(fd);

    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKint64);
    uinteger_t x = arg0->toInteger();
    if (x == 0 && (*arguments)[1]->toInteger())
        error(loc, "llvm.ctlz.i#(0) is undefined");

    int n = getBitsizeOfType(loc, type);
    int c = n >> 1;
    do {
        uinteger_t y = x >> c; if (y != 0) { n -= c; x = y; }
        c = c >> 1;
    } while (c != 0);

    return new IntegerExp(loc, n - x, type);
}

Expression *eval_bswap(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Type* type = getTypeOfOverloadedIntrinsic(fd);

    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKint64);
    uinteger_t n = arg0->toInteger();
    #define BYTEMASK  0x00FF00FF00FF00FFLL
    #define SHORTMASK 0x0000FFFF0000FFFFLL
    #define INTMASK 0x0000FFFF0000FFFFLL
    switch (type->toBasetype()->ty)
    {
      case Tint64:
      case Tuns64:
          // swap high and low uints
          n = ((n >> 32) & INTMASK) | ((n & INTMASK) << 32);
      case Tint32:
      case Tuns32:
          // swap adjacent ushorts
          n = ((n >> 16) & SHORTMASK) | ((n & SHORTMASK) << 16);
      case Tint16:
      case Tuns16:
          // swap adjacent ubytes
          n = ((n >> 8 ) & BYTEMASK)  | ((n & BYTEMASK) << 8 );
          break;
      case Tint128:
      case Tuns128:
          error(loc, "cent/ucent not supported");
          break;
      default:
          error(loc, "unsupported type");
          break;
    }
    return new IntegerExp(loc, n, type);
}

Expression *eval_ctpop(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    // FIXME Does not work for cent/ucent
    Type* type = getTypeOfOverloadedIntrinsic(fd);

    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKint64);
    uinteger_t n = arg0->toInteger();
    n = n - ((n >> 1) & 0x5555555555555555);
    n = (n & 0x3333333333333333) + ((n >> 2) & 0x3333333333333333);
    n = n  + ((n >> 4) & 0x0F0F0F0F0F0F0F0F);
    n = n + (n >> 8);
    n = n + (n >> 16);
    n = n + (n >> 32);
    return new IntegerExp(loc, n, type);
}
#else

Expression *eval_bsf(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKint64);
    uinteger_t n = arg0->toInteger();
    if (n == 0)
        error(loc, "bsf(0) is undefined");
    n = (n ^ (n - 1)) >> 1;  // convert trailing 0s to 1, and zero rest
    int k = 0;
    while( n )
    {   ++k;
        n >>=1;
    }
    return new IntegerExp(loc, k, Type::tint32);
}

Expression *eval_bsr(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKint64);
    uinteger_t n = arg0->toInteger();
    if (n == 0)
        error(loc, "bsr(0) is undefined");
    int k = 0;
    while(n >>= 1)
    {
        ++k;
    }
    return new IntegerExp(loc, k, Type::tint32);
}

Expression *eval_bswap(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    Expression *arg0 = (*arguments)[0];
    assert(arg0->op == TOKint64);
    uinteger_t n = arg0->toInteger();
    #define BYTEMASK  0x00FF00FF00FF00FFLL
    #define SHORTMASK 0x0000FFFF0000FFFFLL
    #define INTMASK 0x0000FFFF0000FFFFLL
    // swap adjacent ubytes
    n = ((n >> 8 ) & BYTEMASK)  | ((n & BYTEMASK) << 8 );
    // swap adjacent ushorts
    n = ((n >> 16) & SHORTMASK) | ((n & SHORTMASK) << 16);
    TY ty = arg0->type->toBasetype()->ty;
    // If 64 bits, we need to swap high and low uints
    if (ty == Tint64 || ty == Tuns64)
        n = ((n >> 32) & INTMASK) | ((n & INTMASK) << 32);
    return new IntegerExp(loc, n, arg0->type);
}
#endif

void builtin_init()
{
#if IN_LLVM
    builtins._init(67); // Prime number like default value
#else
    builtins._init(45);
#endif

    // @safe @nogc pure nothrow real function(real)
    add_builtin("_D4core4math3sinFNaNbNiNfeZe", &eval_sin);
    add_builtin("_D4core4math3cosFNaNbNiNfeZe", &eval_cos);
    add_builtin("_D4core4math3tanFNaNbNiNfeZe", &eval_tan);
    add_builtin("_D4core4math4sqrtFNaNbNiNfeZe", &eval_sqrt);
    add_builtin("_D4core4math4fabsFNaNbNiNfeZe", &eval_fabs);
    add_builtin("_D4core4math5expm1FNaNbNiNfeZe", &eval_unimp);
    add_builtin("_D4core4math4exp21FNaNbNiNfeZe", &eval_unimp);

    // @trusted @nogc pure nothrow real function(real)
    add_builtin("_D4core4math3sinFNaNbNiNeeZe", &eval_sin);
    add_builtin("_D4core4math3cosFNaNbNiNeeZe", &eval_cos);
    add_builtin("_D4core4math3tanFNaNbNiNeeZe", &eval_tan);
    add_builtin("_D4core4math4sqrtFNaNbNiNeeZe", &eval_sqrt);
    add_builtin("_D4core4math4fabsFNaNbNiNeeZe", &eval_fabs);
    add_builtin("_D4core4math5expm1FNaNbNiNeeZe", &eval_unimp);
    add_builtin("_D4core4math4exp21FNaNbNiNeeZe", &eval_unimp);

    // @safe @nogc pure nothrow double function(double)
    add_builtin("_D4core4math4sqrtFNaNbNiNfdZd", &eval_sqrt);
    // @safe @nogc pure nothrow float function(float)
    add_builtin("_D4core4math4sqrtFNaNbNiNffZf", &eval_sqrt);

    // @safe @nogc pure nothrow real function(real, real)
    add_builtin("_D4core4math5atan2FNaNbNiNfeeZe", &eval_unimp);
    add_builtin("_D4core4math4yl2xFNaNbNiNfeeZe", &eval_unimp);
    add_builtin("_D4core4math6yl2xp1FNaNbNiNfeeZe", &eval_unimp);

    // @safe @nogc pure nothrow long function(real)
    add_builtin("_D4core4math6rndtolFNaNbNiNfeZl", &eval_unimp);

    // @safe @nogc pure nothrow real function(real)
    add_builtin("_D3std4math3sinFNaNbNiNfeZe", &eval_sin);
    add_builtin("_D3std4math3cosFNaNbNiNfeZe", &eval_cos);
    add_builtin("_D3std4math3tanFNaNbNiNfeZe", &eval_tan);
    add_builtin("_D3std4math4sqrtFNaNbNiNfeZe", &eval_sqrt);
    add_builtin("_D3std4math4fabsFNaNbNiNfeZe", &eval_fabs);
    add_builtin("_D3std4math5expm1FNaNbNiNfeZe", &eval_unimp);
    add_builtin("_D3std4math4exp21FNaNbNiNfeZe", &eval_unimp);

    // @trusted @nogc pure nothrow real function(real)
    add_builtin("_D3std4math3sinFNaNbNiNeeZe", &eval_sin);
    add_builtin("_D3std4math3cosFNaNbNiNeeZe", &eval_cos);
    add_builtin("_D3std4math3tanFNaNbNiNeeZe", &eval_tan);
    add_builtin("_D3std4math4sqrtFNaNbNiNeeZe", &eval_sqrt);
    add_builtin("_D3std4math4fabsFNaNbNiNeeZe", &eval_fabs);
    add_builtin("_D3std4math5expm1FNaNbNiNeeZe", &eval_unimp);
    add_builtin("_D3std4math4exp21FNaNbNiNeeZe", &eval_unimp);

    // @safe @nogc pure nothrow double function(double)
    add_builtin("_D3std4math4sqrtFNaNbNiNfdZd", &eval_sqrt);
    // @safe @nogc pure nothrow float function(float)
    add_builtin("_D3std4math4sqrtFNaNbNiNffZf", &eval_sqrt);

    // @safe @nogc pure nothrow real function(real, real)
    add_builtin("_D3std4math5atan2FNaNbNiNfeeZe", &eval_unimp);
    add_builtin("_D3std4math4yl2xFNaNbNiNfeeZe", &eval_unimp);
    add_builtin("_D3std4math6yl2xp1FNaNbNiNfeeZe", &eval_unimp);

    // @safe @nogc pure nothrow long function(real)
    add_builtin("_D3std4math6rndtolFNaNbNiNfeZl", &eval_unimp);

#if IN_LLVM
    // intrinsic llvm.bswap.i16/i32/i64/i128
    add_builtin("llvm.bswap.i#", &eval_bswap);
    add_builtin("llvm.bswap.i16", &eval_bswap);
    add_builtin("llvm.bswap.i32", &eval_bswap);
    add_builtin("llvm.bswap.i64", &eval_bswap);
    add_builtin("llvm.bswap.i128", &eval_bswap);

    // intrinsic llvm.cttz.i8/i16/i32/i64/i128
    add_builtin("llvm.cttz.i#", &eval_cttz);
    add_builtin("llvm.cttz.i8", &eval_cttz);
    add_builtin("llvm.cttz.i16", &eval_cttz);
    add_builtin("llvm.cttz.i32", &eval_cttz);
    add_builtin("llvm.cttz.i64", &eval_cttz);
    add_builtin("llvm.cttz.i128", &eval_cttz);

    // intrinsic llvm.ctlz.i8/i16/i32/i64/i128
    add_builtin("llvm.ctlz.i#", &eval_ctlz);
    add_builtin("llvm.ctlz.i8", &eval_ctlz);
    add_builtin("llvm.ctlz.i16", &eval_ctlz);
    add_builtin("llvm.ctlz.i32", &eval_ctlz);
    add_builtin("llvm.ctlz.i64", &eval_ctlz);
    add_builtin("llvm.ctlz.i128", &eval_ctlz);

    // intrinsic llvm.ctpop.i8/i16/i32/i64/i128
    add_builtin("llvm.ctpop.i#", &eval_ctpop);
    add_builtin("llvm.ctpop.i8", &eval_ctpop);
    add_builtin("llvm.ctpop.i16", &eval_ctpop);
    add_builtin("llvm.ctpop.i32", &eval_ctpop);
    add_builtin("llvm.ctpop.i64", &eval_ctpop);
    add_builtin("llvm.ctpop.i128", &eval_ctpop);
#else

    // @safe @nogc pure nothrow int function(uint)
    add_builtin("_D4core5bitop3bsfFNaNbNiNfkZi", &eval_bsf);
    add_builtin("_D4core5bitop3bsrFNaNbNiNfkZi", &eval_bsr);

    // @safe @nogc pure nothrow int function(ulong)
    add_builtin("_D4core5bitop3bsfFNaNbNiNfmZi", &eval_bsf);
    add_builtin("_D4core5bitop3bsrFNaNbNiNfmZi", &eval_bsr);

    // @safe @nogc pure nothrow uint function(uint)
    add_builtin("_D4core5bitop5bswapFNaNbNiNfkZk", &eval_bswap);
#endif
}

/**********************************
 * Determine if function is a builtin one that we can
 * evaluate at compile time.
 */
BUILTIN isBuiltin(FuncDeclaration *fd)
{
    if (fd->builtin == BUILTINunknown)
    {
#if IN_LLVM
        const char *name = fd->llvmInternal == LLVMintrinsic
                ? fd->intrinsicName.c_str()
                : mangleExact(fd);
        builtin_fp fp = builtin_lookup(name);
#else
        builtin_fp fp = builtin_lookup(mangleExact(fd));
#endif
        fd->builtin = fp ? BUILTINyes : BUILTINno;
    }
    return fd->builtin;
}

/**************************************
 * Evaluate builtin function.
 * Return result; NULL if cannot evaluate it.
 */

Expression *eval_builtin(Loc loc, FuncDeclaration *fd, Expressions *arguments)
{
    if (fd->builtin == BUILTINyes)
    {
#if IN_LLVM
        const char *name = fd->llvmInternal == LLVMintrinsic
                ? fd->intrinsicName.c_str()
                : mangleExact(fd);
        builtin_fp fp = builtin_lookup(name);
#else
        builtin_fp fp = builtin_lookup(fd->mangleExact(fd));
#endif
        assert(fp);
        return fp(loc, fd, arguments);
    }
    return NULL;
}
