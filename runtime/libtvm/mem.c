/*
tvm - mem.c
The Transterpreter - a portable virtual machine for Transputer bytecode
Copyright (C) 2004-2008 Christian L. Jacobsen, Matthew C. Jadud, Carl G. Ritson

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

#include "tvm_mem.h"

#if defined(TVM_CUSTOM_COPY_DATA)
void (*tvm_copy_data)(BYTEPTR write_start, BYTEPTR read_start, UWORD num_bytes);
#elif defined(TVM_USE_MEMCPY) && defined(WORDPTRS_REAL)
/* define nothing */
#else
void tvm_copy_data(BYTEPTR write_start, BYTEPTR read_start, UWORD num_bytes)
{
	/* -- Bit twiddling lesson --
	 * 
	 * For values on a power of two, that value -1 gives you a mask of bits
	 * which will be zero in things which are a multiple of that type.
	 *
	 * e.g. 
	 *   4 = 0100b
	 *   4 - 1 => 3
	 *   3 = 0011b
	 *
	 * Taking the bitwise-AND of a number and this mask is the same as doing a
	 * modulo operation.
	 *
	 * e.g.
	 *   4 & 3 = 4 % 4 = 0
	 *   3 & 3 = 3 % 4 = 3
	 *   5 & 3 = 5 % 4 = 1
	 *
	 * The difference is that modulo is typically compiled to integer division
	 * which is 10s of times slower than bitwise logic, and sadly not all
	 * compilers optimise fixed modulos to bitwise ops.
	 */

	/* If everything is word aligned then copy words, else copy
	 * the bytes one by one.
	 * 
	 * Please see above bit twiddling lesson if confused.
	 */
	if(!((num_bytes | (UWORD) write_start | (UWORD) read_start) & (TVM_WORD_LENGTH - 1)))
	{
		/* Reduce bytes to words */
		num_bytes >>= WSH;
		/* This count is now on words */
		while(num_bytes--)
		{
			write_word((WORDPTR) write_start, read_word((WORDPTR) read_start));
			read_start = (BYTEPTR) wordptr_plus((WORDPTR) read_start, 1);
			write_start = (BYTEPTR) wordptr_plus((WORDPTR) write_start, 1);
		}
	}
	else
	{
		while(num_bytes--)
		{
			write_byte(write_start, read_byte(read_start));
			read_start = byteptr_plus(read_start, 1);
			write_start = byteptr_plus(write_start, 1);
		}
	}
}
#endif /* !defined(TVM_USE_MEMCPY) */

