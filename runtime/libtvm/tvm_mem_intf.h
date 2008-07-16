/*
tvm - tvm_mem_intf.h
The Transterpreter - a portable virtual machine for Transputer bytecode
Copyright (C) 2004-2008 Christian L. Jacobsen, Matthew C. Jadud

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef TVM_MEM_INTF_H
#define TVM_MEM_INTF_H

#ifdef TVM_MEM_INTF_ARRAY
/*{{{  Array memory interface */
extern WORD read_word(WORDPTR ptr);
extern void write_word(WORDPTR ptr, WORD val);
static TVM_INLINE WORDPTR wordptr_minus(WORDPTR ptr, WORD inc)
{
	return ptr - inc * TVM_WORD_LENGTH;
}
static TVM_INLINE WORDPTR wordptr_plus(WORDPTR ptr, WORD inc)
{
	return ptr + inc * TVM_WORD_LENGTH;
}

extern INT16 read_int16(INT16PTR ptr);
extern void write_int16(INT16PTR ptr, INT16 val);
static TVM_INLINE INT16PTR int16ptr_minus(WORDPTR ptr, WORD inc)
{
	return ptr - inc * (TVM_WORD_LENGTH / 2);
}
static TVM_INLINE INT16PTR int16ptr_plus(WORDPTR ptr, WORD inc)
{
	return ptr + inc * (TVM_WORD_LENGTH / 2);
}

extern BYTE read_byte(BYTEPTR ptr);
extern void write_byte(BYTEPTR ptr, BYTE val);
static TVM_INLINE BYTEPTR byteptr_minus(BYTEPTR ptr, WORD inc)
{
	return ptr - inc;
}
static TVM_INLINE BYTEPTR byteptr_plus(BYTEPTR ptr, WORD inc)
{
	return ptr + inc;
}

extern float read_wordf(WORDPTR ptr);
extern void write_wordf(WORDPTR ptr, float val);

extern double read_wordd(WORDPTR ptr);
extern void write_wordd(WORDPTR ptr, double val);

extern int *wordptr_real_address(WORDPTR ptr);
/*}}}*/
#elif defined(TVM_MEM_INTF_BIGENDIAN)
/*{{{  (deprecated) Big-endian memory interface */
extern WORD read_word(WORDPTR ptr);
extern void write_word(WORDPTR ptr, WORD val);
extern WORDPTR wordptr_plus(WORDPTR ptr, WORD inc);
extern WORDPTR wordptr_minus(WORDPTR ptr, WORD inc);

extern INT16 read_int16(INT16PTR ptr);
extern void write_int16(INT16PTR ptr, INT16 val);
extern INT16PTR int16ptr_plus(INT16PTR ptr, WORD inc);
extern INT16PTR int16ptr_minus(INT16PTR ptr, WORD inc);

extern BYTE read_byte(BYTEPTR ptr);
extern void write_byte(BYTEPTR ptr, BYTE val);
extern BYTEPTR byteptr_plus(BYTEPTR ptr, WORD inc);
extern BYTEPTR byteptr_minus(BYTEPTR ptr, WORD inc);

extern void write_wordf(WORDPTR ptr, float val);
extern float read_wordf(WORDPTR ptr);
extern void write_wordd(WORDPTR ptr, double val);
extern double read_wordd(WORDPTR ptr);

#define wordptr_real_address(LOC)	( LOC )
/*}}}*/
#else
/*{{{  Native memory interface */
#define read_word(LOC)			( *((WORDPTR)(LOC)) )
#define write_word(LOC,VAL)		( *((WORDPTR)(LOC)) ) = (VAL)
#define wordptr_minus(PTR,LOC)		( ((WORDPTR)PTR) - (LOC) )
#define wordptr_plus(PTR,LOC)		( ((WORDPTR)PTR) + (LOC) )

#define read_int16(LOC)			( *((INT16PTR)(LOC)) )
#define write_int16(LOC,VAL)		( *((INT16PTR)(LOC)) ) = (VAL)
#define int16ptr_minus(PTR,LOC)		( ((INT16PTR)PTR) - (LOC) )
#define int16ptr_plus(PTR,LOC)		( ((INT16PTR)PTR) + (LOC) )

#define read_byte(LOC)			( *((BYTEPTR)(LOC)) )
#define write_byte(LOC,VAL)		( *((BYTEPTR)(LOC)) ) = (VAL)
#define byteptr_minus(PTR,LOC)		( ((BYTEPTR)PTR) - (LOC) )
#define byteptr_plus(PTR,LOC)		( ((BYTEPTR)PTR) + (LOC) )

#define read_wordf(LOC)			( *((float *)(LOC)) )
#define write_wordf(LOC,VAL)		( *((float *)(LOC)) ) = (VAL)

#define read_wordd(LOC)			( *((double *)(LOC)) )
#define write_wordd(LOC,VAL)		( *((double *)(LOC)) ) = (VAL)

#define wordptr_real_address(LOC)	( LOC )
/*}}}*/
#endif

/*{{{  Common operations */
/* stddef.h is freestanding, so should be safe in any build */
#include <stddef.h>
#ifndef offsetof
#define offsetof(t,f) ((WORD) (&((((t *)(0))->f))))
#endif

#define word_offset(type, field) \
	(offsetof (type, field) >> WSH)
#define wordptr_offset(ptr, type, field) \
	wordptr_plus ((ptr), word_offset (type, field))
#define read_offset(ptr, type, field) \
	read_word (wordptr_offset (ptr, type, field))
#define write_offset(ptr, type, field, data) \
	write_word (wordptr_offset (ptr, type, field), (WORD) (data))
#define read_mt_type(ptr) \
	(read_word (wordptr_minus ((ptr), 1)))
/*}}}*/

#endif /* !TVM_MEM_INTF */