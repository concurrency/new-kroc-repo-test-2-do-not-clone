/*
 * main.c - POSIX libtvm Wrapper
 *
 * Copyright (C) 2004-2008  Christian L. Jacobsen, Matthew C. Jadud
 *
 */

#include "tvm_posix.h"
#include <tvm_tbc.h>

/*{{{  Global State */
char			*prog_name 	= NULL;
int			tvm_argc	= 0;
char			**tvm_argv	= NULL;
/*}}}*/

/*{{{  Static State  */
static tvm_t		tvm;
static tvm_ectx_t	firmware_ctx, user_ctx;
static volatile WORD	alarm_set	= 0;
static volatile WORD	alarm_time	= 0;
/*}}}*/

/*{{{  Firmware */
static WORDPTR		firmware_memory;
static WORD		firmware_memory_len;
static WORD		kyb_channel, scr_channel, err_channel;
static WORD 		tlp_argv[] = { (WORD) &kyb_channel, (WORD) &scr_channel, (WORD) &err_channel };

#define ws_size		firmware_ws_size
#define vs_size		firmware_vs_size
#define ms_size		firmware_ms_size
#define inst_size	firmware_bytecode_len
#define transputercode	firmware_bytecode
#include "firmware.h"
#undef ws_size
#undef vs_size
#undef ms_size
#undef inst_size
#undef transputercode
/*}}}*/

/*{{{  User */
static tbc_t		*tbc;
static BYTEPTR		tbc_data;
static WORD		tbc_length;

static WORDPTR		user_memory;
static WORD		user_memory_len;
/*}}}*/

/*{{{  tvm_get_time */
#if defined(HAVE_GETTIMEOFDAY)
static WORD tvm_get_time (ECTX ectx)
{
	struct timeval t;

	gettimeofday (&t, 0);

	return (WORD) ((t.tv_sec * 1000000) + t.tv_usec);
}
#elif defined(WIN32)
static WORD tvm_get_time (ECTX ectx)
{
	/*
	http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/filetime_str.asp
	"The FILETIME structure is a 64-bit value representing the number of 
	 100-nanosecond intervals since January 1, 1601 (UTC)."
	*/
	ULARGE_INTEGER usecs;
	FILETIME time;
	
	/* Get the system time */
	GetSystemTimeAsFileTime (&time);
	
	/* Put it into a structure we can work with, and turn it into usecs
	 * See: 
	 *  http://msdn.microsoft.com/library/default.asp?url=/library/en-us/winprog/winprog/ularge_integer_str.asp
	 */
	memcpy (&usecs, &time, sizeof(FILETIME));
	usecs.QuadPart = usecs.QuadPart / 10;

	/* Return the clock, just the lower parts thankyou */
	return usecs.LowPart;
}
#elif defined(HAVE_FTIME)
static WORD tvm_get_time (ECTX ectx)
{
	struct timeb milli;

	ftime (&milli);
	
	return (WORD) (milli.millitm + (milli.time * 1000));
}
#endif
/*}}}*/

/*{{{  tvm_set_alarm */
#if defined(HAVE_SETITIMER)
#define HAVE_SET_ALARM
static void tvm_set_alarm (ECTX ectx)
{
	struct itimerval timeout;
	unsigned int t;
	WORD now;

	if (alarm_set && TIME_AFTER(ectx->tnext, alarm_time)) {
		return;
	}

	now = ectx->get_time (ectx);
	t = ectx->tnext - now;

	if (TIME_AFTER(now, ectx->tnext) || (t == 0)) {
		ectx->modify_sync_flags (ectx, SFLAG_TQ, 0);
		alarm_set = 0;
	}

	timeout.it_interval.tv_sec	= 0;
	timeout.it_interval.tv_usec	= 0;
	timeout.it_value.tv_sec		= t / 1000000;
	timeout.it_value.tv_usec	= t % 1000000;

	alarm_set = 1;
	alarm_time = ectx->tnext;

	if (setitimer (ITIMER_REAL, &timeout, NULL) != 0) {
		/* Behave like busy-wait if setitimer fails. */
		ectx->modify_sync_flags (ectx, SFLAG_TQ, 0);
		alarm_set = 0;
	}
}
#endif
/*}}}*/

/*{{{  tvm_sleep */
static void tvm_sleep (void)
{
	WORD now	= firmware_ctx.get_time (&firmware_ctx);
	WORD timeout	= 0;
	int set		= 0;

	if (firmware_ctx.tptr != NOT_PROCESS_P) {
		timeout = firmware_ctx.tnext;
		set++;
	}

	if (user_ctx.tptr != NOT_PROCESS_P) {
		if (!set || TIME_BEFORE(user_ctx.tnext, timeout)) {
			timeout = user_ctx.tnext;
			set++;
		}
	}

	if (set && TIME_AFTER(timeout, now)) {
		unsigned int period = timeout - now;
		
		if (period > 0) {
			#if defined(HAVE_NANOSLEEP)
			struct timespec to;

			to.tv_sec = (period / 1000000);
			to.tv_nsec = ((period % 1000000) * 1000);
			
			nanosleep (&to, 0);
			#elif defined(HAVE_SLEEP)
			Sleep (period / 1000);
			#else
			#warning "Don't know how to sleep..."
			#endif
		}
	}
}
/*}}}*/

/*{{{  Old stuff for recycling */
/* Not sure where to define these, they could be platform specific */
#define EXIT_SIGSEGV 139
#define EXIT_SIGBUS 138

#if 0
static void sigsegv_handler(int num)
{
	/* Set the signal back to the default so we dont get into
	 * some kind of ugly loop should we segfault in here */
	signal(SIGSEGV, SIG_DFL);

	print_crash(EXIT_SIGSEGV);
	printf("Segmentation fault (%d)\n", num);
	printf("\n");
	print_state();
	printf("exiting... (segmentation fault)\n");

	if(code_start <= iptr && iptr < code_end)
	{
		print_debug_info(filename);
	}

	/* 139 */
	exit(EXIT_SIGSEGV); /* This seems to be what a segfaulted program returns */
}

#if !defined(WIN32) && !defined(CYGWIN)
/* Borland does not have sigbus */
static void sigbus_handler(int num)
{
	/* Set the signal back to the default so we dont get into
	 * some kind of ugly loop should we segfault in here */
	signal(SIGBUS, SIG_DFL);

	print_crash(EXIT_SIGBUS);
	printf("Bus error (%d)\n", num);
	printf("\n");
	print_state();
	printf("exiting... (bus error)\n");

	if(code_start <= iptr && iptr < code_end)
	{
		print_debug_info(filename);
	}

	/* 138 */
	exit(EXIT_SIGBUS); /* This seems to be what a buserrored program returns */
}
#endif
#endif

/*{{{  sigalarm_handler */
static void sigalrm_handler (int num)
{
	firmware_ctx.sflags	|= SFLAG_TQ;
	user_ctx.sflags		|= SFLAG_TQ;
	alarm_set		= 0;
}
/*}}}*/

static void v_error_out (const char *fmt, va_list ap)
{
	fprintf (stderr, "%s: ", prog_name);
	vfprintf (stderr, fmt, ap);
	if (errno != 0) {
		fprintf (stderr, ": ");
		perror ("");
	} else {
		fprintf (stderr, ".\n");
	}
}

static int error_out (const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	v_error_out (fmt, ap);
	va_end (ap);

	return -1;
}

static int error_out_no_errno (const char *fmt, ...)
{
	va_list ap;

	errno = 0;

	va_start (ap, fmt);
	v_error_out (fmt, ap);
	va_end (ap);

	return -1;
}

static void usage (FILE *out)
{
	fprintf (out, "Usage: %s <filename>\n", prog_name);
}

static void add_system_functions (ECTX ectx)
{
	ectx->get_time = tvm_get_time;
	#ifdef HAVE_SET_ALARM
	ectx->set_alarm = tvm_set_alarm;
	#endif
}

static int install_firmware_ctx (void)
{
	WORDPTR ws, vs, ms;
	ECTX firmware = &firmware_ctx;

	tvm_ectx_init (&tvm, firmware);
	add_system_functions (firmware);
	install_sffi (firmware);

	firmware_memory_len = tvm_ectx_memory_size (
		firmware,
		"!??", 3,
		firmware_ws_size, 
		firmware_vs_size, 
		firmware_ms_size
	);

	firmware_memory = malloc (sizeof(WORD) * firmware_memory_len);
	if (firmware_memory == NULL) {
		return error_out ("unable to allocate firmware memory (%d words)", firmware_memory_len);
	}

	tvm_ectx_layout (
		firmware, firmware_memory,
		"!??", 3,
		firmware_ws_size, 
		firmware_vs_size, 
		firmware_ms_size,
		&ws, &vs, &ms
	);
	
	return tvm_ectx_install_tlp (
		firmware, (BYTEPTR) firmware_bytecode, ws, vs, ms,
		"!??", 3, tlp_argv
	);
}

static int read_tbc (const char *fn)
{
	FILE *fh = fopen (fn, "rb");
	int ret;

	if (fh == NULL) {
		return error_out ("unable to open \"%s\"", fn);
	}

	if (fseek (fh, 0, SEEK_END) < 0) {
		return error_out ("unable to seek to file end", fn);
	}

	tbc_length	= (int) ftell (fh);
	tbc_data 	= (BYTEPTR) malloc (tbc_length);
	
	if (tbc_data == NULL) {
		return error_out ("unable to allocate TBC memory (%d bytes)", tbc_length);
	}
	
	if (fseek (fh, 0, SEEK_SET) < 0) {
		return error_out ("unable to seek to file start", fn);
	}

	if ((ret = fread (tbc_data, 1, tbc_length, fh)) != tbc_length) {
		return error_out ("failed reading TBC (%d of %d bytes)", ret, tbc_length);
	}

	fclose (fh);

	return 0;
}

static int load_tbc (const char *fn)
{
	tenc_element_t	element;
	int		length;
	BYTE		*data;

	if (read_tbc (fn)) {
		error_out_no_errno ("unable to read TBC");
		return -1;
	}

	/* CGR FIXME: this function from here onward should move to libtvm. */

	data	= tbc_data;
	length	= tbc_length;

	if (tenc_decode_element (data, &length, &element)) {
		error_out_no_errno ("unable to decode TBC header");
		return -1;
	}

	#if TVM_WORD_LENGTH == 2
	if (memcmp (element.id, "tenc", 4) != 0)
	#else
	if (memcmp (element.id, "TEnc", 4) != 0)
	#endif
	{
		error_out_no_errno ("unable to decode TBC is not TEncode");
		return -1;
	}

	data	= element.data.bytes;
	length	= element.length;

	if (tenc_walk_to_element (data, &length, "tbcL", &element) < 0) {
		error_out_no_errno ("TBC does not contain bytecode headers");
		return -1;
	}

	if (tbc_decode (element.data.bytes, element.length, &tbc)) {
		error_out_no_errno ("unable to decode TBC");
		return -1;
	}

	return 0;
}

static int install_user_ctx (const char *fn)
{
	#if defined(TVM_DYNAMIC_MEMORY) && defined(TVM_OCCAM_PI)
	const char *const tlp_fmt = "?!!F";
	#else
	const char *const tlp_fmt = "?!!";
	#endif /* !TVM_DYNAMIC_MEMORY || !TVM_OCCAM_PI */
	const char *tlp;

	WORDPTR ws, vs, ms;
	ECTX user = &user_ctx;

	if (load_tbc (fn)) {
		return -1;
	}

	if (tbc->tlp != NULL) {
		tlp = tbc->tlp->fmt;
		if (tlp == NULL) {
			tlp = tlp_fmt;
		} else if (strcmp (tlp, "") == 0) {
			/* OK */
		} else if (strcmp (tlp, "?!!F") == 0) {
			/* OK */
		} else if (strcmp (tlp, "?!!") == 0) {
			/* OK */
		} else {
			error_out_no_errno ("unsupported top-level-process format: \"%s\"", tlp);
			return -1;
		}
	} else {
		tlp = tlp_fmt;
	}

	tvm_ectx_init (&tvm, user);
	user->pri = 1; /* lower priority */
	add_system_functions (user);
	install_sffi (user);

	user_memory_len = tvm_ectx_memory_size (
		user,
		tlp, strlen (tlp), 
		tbc->ws, tbc->vs, tbc->ms
	);

	user_memory = (WORDPTR) malloc (sizeof (WORD) * user_memory_len);
	if (user_memory == NULL) {
		return error_out ("failed to allocate user memory (%d words)", user_memory_len);
	}

	tvm_ectx_layout (
		user, user_memory,
		tlp, strlen (tlp), 
		tbc->ws, tbc->vs, tbc->ms, 
		&ws, &vs, &ms
	);

	return tvm_ectx_install_tlp (
		user, tbc->bytecode, ws, vs, ms,
		tlp, strlen (tlp), tlp_argv
	);
}

static int run_firmware (void)
{
	int ret = tvm_run (&firmware_ctx);

	if (ret == ECTX_SLEEP) {
		return ret; /* OK - timer sleep */
	} else if (ret == ECTX_EMPTY) {
		/* FIXME: check deadlock */
		return ret;
	}

	/* Being here means something unexpected happened... */
	fprintf (stderr, "Firmware failed; state = %c\n", firmware_ctx.state);
	
	return ECTX_ERROR;
}

static int run_user (void)
{
	int ret = tvm_run_count (&user_ctx, 10000);

	switch (ret) {
		case ECTX_PREEMPT:
		case ECTX_SHUTDOWN:
		case ECTX_SLEEP:
		case ECTX_TIME_SLICE:
			return ret; /* OK */
		case ECTX_EMPTY:
			if (tvm_ectx_waiting_on (&user_ctx, user_memory, user_memory_len)) {
				return ret; /* OK - waiting for firmware */
			}
			break;
		default:
			break;
	}

	return ECTX_ERROR;
}

#ifdef TVM_PROFILING
static void output_profile (ECTX ectx)
{
	const int n_pri = sizeof(ectx->profile.pri) / sizeof(UWORD);
	const int n_sec = sizeof(ectx->profile.sec) / sizeof(UWORD);
	UWORD total = 0;
	int i;

	for (i = 0; i < n_pri; ++i) {
		total += ectx->profile.pri[i];
	}
	for (i = 0; i < n_sec; ++i) {
		total += ectx->profile.sec[i];
	}

	for (i = 0; i < n_pri; ++i) {
		if (ectx->profile.pri[i] > 0) {
			fprintf (stderr,
				"pri  0x%03x  %-12s = %-7d (%.2f%%)\n", 
				i, tvm_instr_pri_name (i), ectx->profile.pri[i],
				((double) ectx->profile.pri[i] / (double) total) * 100.0
			);
		}
	}
	for (i = 0; i < n_sec; ++i) {
		if (ectx->profile.sec[i] > 0) {
			fprintf (stderr, 
				"sec  0x%03x  %-12s = %-7d (%.2f%%)\n", 
				i, tvm_instr_sec_name (i), ectx->profile.sec[i],
				((double) ectx->profile.sec[i] / (double) total) * 100.0
			);
		}
	}
}

static void output_profiling (void)
{
	fprintf (stderr, "-- Firmware Bytecode Profile\n");
	output_profile (&firmware_ctx);
	fprintf (stderr, "-- User Bytecode Profile\n");
	output_profile (&user_ctx);
}
#endif /* TVM_PROFILING */

int main (int argc, char *argv[])
{
	char *fn;
	int f_ret, u_ret;

	prog_name	= argv[0]; 
	tvm_argc	= argc;
	tvm_argv	= argv;

	if (argc < 2) {
		usage (stderr);
		return 1;
	} else {
		fn = argv[1];
	}

	tvm_init (&tvm);

	if (install_firmware_ctx () < 0) {
		error_out_no_errno ("failed to install firmware");
		return 1;
	}
	if (install_user_ctx (fn) < 0) {
		error_out_no_errno ("failed to load user bytecode");
		return 1;
	}

	kyb_channel = NOT_PROCESS_P;
	scr_channel = NOT_PROCESS_P;
	err_channel = NOT_PROCESS_P;

	#if defined(SIGALRM) && defined(HAVE_SET_ALARM)
	signal (SIGALRM, sigalrm_handler);
	#endif

	for (;;) {
		f_ret = run_firmware ();
		u_ret = run_user ();

		if ((f_ret == ECTX_EMPTY || f_ret == ECTX_SLEEP) &&
			(u_ret == ECTX_EMPTY || u_ret == ECTX_SLEEP)) {
			if (firmware_ctx.fptr == NOT_PROCESS_P && user_ctx.fptr == NOT_PROCESS_P) {
				tvm_sleep ();
			}
		} else if (f_ret == ECTX_ERROR || u_ret == ECTX_ERROR) {
			break;
		} else if (u_ret == ECTX_SHUTDOWN) {
			/* Run firmware to clear buffers */
			run_firmware ();
			break;
		}
	}
	
	if (u_ret == ECTX_ERROR) {
		if (tbc->lni) {
			tbc_lni_entry_t	*ln;
			tbc_lni_t	*lni = tbc->lni;
			tenc_str_t 	*file;
			int offset = user_ctx.iptr - tbc->bytecode;
			int i = 0;

			while (i < lni->n_entries) {
				if (lni->entries[i].offset > offset) {
					break;
				}
				i++;
			}
			ln = &(lni->entries[i - 1]);

			file = lni->files;
			for (i = 0; i < ln->file; ++i) {
				file = file->next;
			}

			fprintf (stderr,
				"Error at %s:%d\n",
				file->str, ln->line
			);
		}

		/* FIXME: more debugging */
		fprintf (stderr, 
			"Program failed, state = %c, eflags = %08x\n",
			user_ctx.state, user_ctx.eflags
		);

		return 1;
	}
	
	free (firmware_memory);
	free (user_memory);
	free (tbc_data);

	#ifdef TVM_PROFILING
	output_profiling ();
	#endif

	return 0;
}

