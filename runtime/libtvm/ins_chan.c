/*
tvm - ins_chan.c
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

#include "tvm.h"
#include "instructions.h"
#include "ext_chan.h"
#include "scheduler.h"
#include "ins_chan.h"

TVM_HELPER int chan_io_begin(ECTX ectx, WORD altable, WORDPTR chan_ptr, BYTEPTR data_ptr, WORDPTR *chan_val)
{
	WORDPTR other_WPTR = *chan_val = (WORDPTR) read_word(chan_ptr);

	if(other_WPTR == NOT_PROCESS_P)
	{
		/* ...Put this process('s WPTR) into the channel word, */
		write_word(chan_ptr, (WORD)WPTR);
		/* store our state */
		WORKSPACE_SET(WPTR, WS_POINTER, (WORD)data_ptr);
		WORKSPACE_SET(WPTR, WS_ECTX, (WORD)ectx);
		WORKSPACE_SET(WPTR, WS_IPTR, (WORD)IPTR);

		RUN_NEXT_ON_QUEUE_RET();
	}
	else if(altable)
	{
		WORD alt_state = WORKSPACE_GET(other_WPTR, WS_STATE);

		/* Check if alt_state == MIN_INT, MIN_INT +1, MI_INT +2, MIN_INT +3 */
		if((alt_state & (~3)) == MIN_INT)
		{
			WORKSPACE_SET(WPTR, WS_POINTER, (WORD)data_ptr);
			WORKSPACE_SET(WPTR, WS_ECTX, (WORD)ectx);
			WORKSPACE_SET(WPTR, WS_IPTR, (WORD)IPTR);

			write_word(chan_ptr, (WORD)WPTR);

			*chan_val = NOT_PROCESS_P;
			
			switch(alt_state) {
				case WAITING_P:
					WORKSPACE_SET(other_WPTR, WS_STATE, DISABLING_P);
					WPTR = other_WPTR;
					IPTR = (BYTEPTR)WORKSPACE_GET(WPTR, WS_IPTR);
					break;
				case ENABLING_P:
					WORKSPACE_SET(other_WPTR, WS_STATE, DISABLING_P);
					/* Fall through */
				case DISABLING_P:
					RUN_NEXT_ON_QUEUE_RET();
				default:
					SET_ERROR_FLAG_RET(EFLAG_CHAN);
			}
		}
	}

	return ECTX_CONTINUE;
}

TVM_HELPER int chan_io_end(ECTX ectx, WORDPTR chan_ptr, WORDPTR other_WPTR)
{
	ECTX other_ectx = (ECTX) WORKSPACE_GET(other_WPTR, WS_ECTX);

	/* Set the channel word to NotProcess.p */
	write_word(chan_ptr, NOT_PROCESS_P);

	if (other_ectx == ectx) {
		/* Add ourselves to the back of the runqueue */
		ADD_TO_QUEUE_IPTR(WPTR, IPTR);
		/* Reschedule the process at the other end of the channel */
		WPTR = other_WPTR;
		/* Load the newly scheduled processes instruction pointer */
		IPTR = (BYTEPTR)WORKSPACE_GET(WPTR, WS_IPTR);
		/* Continue */
		return ECTX_CONTINUE;
	} else {
		return other_ectx->add_to_queue_external(other_ectx, ectx, other_WPTR);
	}
}

TVM_HELPER int chan_in(ECTX ectx, WORD num_bytes, WORDPTR chan_ptr, BYTEPTR write_start)
{
	WORDPTR other_WPTR;
	int ret;
	
	UNDEFINE_STACK();

	if((ret = chan_io_begin(ectx, 0, chan_ptr, write_start, &other_WPTR)))
	{
		return ret;
	}
	else if (other_WPTR != NOT_PROCESS_P)
	{
		/* Where we start reading from */
		BYTEPTR read_start = (BYTEPTR)WORKSPACE_GET(other_WPTR, WS_POINTER);
		/* Copy the data */
		tvm_copy_data(write_start, read_start, num_bytes);
		/* Complete channel operation */
		return chan_io_end(ectx, chan_ptr, other_WPTR);
	}

	return ECTX_CONTINUE;
}

TVM_HELPER int chan_out(ECTX ectx, WORD num_bytes, WORDPTR chan_ptr, BYTEPTR read_start)
{
	WORDPTR other_WPTR;
	int ret;
	
	UNDEFINE_STACK();

	if((ret = chan_io_begin(ectx, 1, chan_ptr, read_start, &other_WPTR)))
	{
		return ret;
	}
	else if(other_WPTR != NOT_PROCESS_P)
	{
		/* Normal communication */
		BYTEPTR write_start = (BYTEPTR)WORKSPACE_GET(other_WPTR, WS_POINTER);
		/* Copy the data */
		tvm_copy_data(write_start, read_start, num_bytes);
		/* Complete channel operation */
		return chan_io_end(ectx, chan_ptr, other_WPTR);
	}
		
	return ECTX_CONTINUE;
}

TVM_HELPER int chan_swap(ECTX ectx, WORDPTR chan_ptr, WORDPTR data_ptr)
{
	WORDPTR other_WPTR;
	int ret;
	
	UNDEFINE_STACK();

	if((ret = chan_io_begin(ectx, 1, chan_ptr, (BYTEPTR)data_ptr, &other_WPTR)))
	{
		return ret;
	}
	else if(other_WPTR != NOT_PROCESS_P)
	{
		/* Normal communication */
		WORDPTR other_ptr = (WORDPTR)WORKSPACE_GET(other_WPTR, WS_POINTER);
		/* Swap data */
		swap_data_word(data_ptr, other_ptr);
		/* Complete channel operation */
		return chan_io_end(ectx, chan_ptr, other_WPTR);
	}

	return ECTX_CONTINUE;
}

/****************************************************************************
 *                   0xF_              0xF_              0xF_               *
 ****************************************************************************/

/* 0x07 - 0xF7 - in - input message */
TVM_INSTRUCTION (ins_in)
{
	BYTEPTR data_ptr	= (BYTEPTR)CREG;
	WORDPTR chan_ptr	= (WORDPTR)BREG;
	WORD bytes		= AREG;

	return chan_in(ectx, bytes, chan_ptr, data_ptr);
}

/* 0x0B - 0xFB - out - output message */
TVM_INSTRUCTION (ins_out)
{
	BYTEPTR data_ptr	= (BYTEPTR)CREG;
	WORDPTR chan_ptr	= (WORDPTR)BREG;
	WORD bytes		= AREG;

	return chan_out(ectx, bytes, chan_ptr, data_ptr);
}

/* 0x0E - 0xFE - outbyte - output byte */
TVM_INSTRUCTION (ins_outbyte)
{
	BYTEPTR data_ptr	= (BYTEPTR)WPTR;
	WORDPTR chan_ptr	= (WORDPTR)BREG;
	BYTE data		= (BYTE)AREG;

	/* Put the byte to be transfered at the top of the workspace */
	write_byte(data_ptr, data);

	return chan_out(ectx, 1, chan_ptr, data_ptr);
}

/* 0x0F - 0xFF - outword - output word */
TVM_INSTRUCTION (ins_outword)
{
	BYTEPTR data_ptr	= (BYTEPTR)WPTR;
	WORDPTR chan_ptr	= (WORDPTR)BREG;
	WORD data		= AREG;

	/* Put the word to be transfered at the top of the workspace */
	write_word((WORDPTR)data_ptr, data);

	return chan_out(ectx, TVM_WORD_LENGTH, chan_ptr, data_ptr);
}

