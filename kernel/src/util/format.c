/*
	Glidix kernel

	Copyright (c) 2021, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <glidix/util/common.h>
#include <glidix/util/string.h>
#include <glidix/util/format.h>

/**
 * Represents parsed flags from a conversion specification.
 */
#define	FLAG_LEFTADJ			(1 << 0)			/* - */
#define	FLAG_SIGN			(1 << 1)			/* + */
#define	FLAG_SPACE			(1 << 2)			/* <space> */
#define	FLAG_ALT			(1 << 3)			/* # */
#define	FLAG_ZEROPAD			(1 << 4)			/* 0 */

/**
 * Parsed length modifiers.
 */
enum
{
	_LM_NONE,
	_LM_hh,
	_LM_h,
	_LM_l,
	_LM_ll,
	_LM_j,
	_LM_z,
	_LM_t,
	_LM_L,
};

/**
 * Formatter state.
 */
typedef struct
{
	/**
	 * Points to where the next character should be stored.
	 */
	char *put;

	/**
	 * The number of characters we can still output without truncating
	 * (EXCLUDING the NUL byte).
	 */
	size_t charsLeft;

	/**
	 * The number of characters we have outputted, including truncated chars,
	 * but EXCLUDING the NUL byte.
	 */
	size_t charsPrinted;
} FormatterState;

static void __putc(FormatterState *state, char c)
{
	if (state->charsLeft != 0)
	{
		*state->put++ = c;
		state->charsLeft--;
	};

	state->charsPrinted++;
};

static void __putbuf(FormatterState *state, const char *s, size_t sz)
{
	while (sz--)
	{
		__putc(state, *s++);
	};
};

static int __parse_flag(const char **fmtptr, int *flagptr)
{
	switch (**fmtptr)
	{
	case '-':
		(*flagptr) |= FLAG_LEFTADJ;
		(*fmtptr)++;
		return 1;
	case '+':
		(*flagptr) |= FLAG_SIGN;
		(*fmtptr)++;
		return 1;
	case ' ':
		(*flagptr) |= FLAG_SPACE;
		(*fmtptr)++;
		return 1;
	case '#':
		(*flagptr) |= FLAG_ALT;
		(*fmtptr)++;
		return 1;
	case '0':
		(*flagptr) |= FLAG_ZEROPAD;
		(*fmtptr)++;
		return 1;
	default:
		return 0;
	};
};

static void __printf_putfield(FormatterState *state, const char *data, size_t len, int flags, int fieldWidth)
{
	if ((fieldWidth == -1) || (len > fieldWidth))
	{
		__putbuf(state, data, len);
		return;
	};
	
	if (flags & FLAG_LEFTADJ)
	{
		__putbuf(state, data, len);
		size_t toPad = fieldWidth - len;
		
		while (toPad--)
		{
			__putc(state, ' ');
		};
	}
	else
	{
		size_t toPad = fieldWidth - len;
		
		while (toPad--)
		{
			__putc(state, ' ');
		};

		__putbuf(state, data, len);
	};
};

/**
 * The "firstLetter" argument is the letter used to represent 10 if base > 10.
 */
static void __printf_conv_signed(FormatterState *state, int flags, int lenmod, int fieldWidth, int precision, int base, char firstLetter, va_list ap)
{
	size_t bufsize = 64;
	char *convbuf;
	if (precision > 60)
	{
		convbuf = (char*) kalloca(precision+4);
		bufsize = precision + 4;
	}
	else
	{
		convbuf = (char*) kalloca(64);
	};
	
	memset(convbuf, 0, bufsize);
	
	int64_t num = 0;
	switch (lenmod)
	{
	case _LM_NONE:
		num = (int64_t) va_arg(ap, int);
		break;
	case _LM_hh:
		num = (int64_t) va_arg(ap, int);	// char
		break;
	case _LM_h:
		num = (int64_t) va_arg(ap, int);	// short
		break;
	case _LM_l:
		num = (int64_t) va_arg(ap, long);
		break;
	case _LM_ll:
		num = (int64_t) va_arg(ap, long long);
		break;
	case _LM_j:
		num = (int64_t) va_arg(ap, intmax_t);
		break;
	case _LM_z:
		num = (int64_t) va_arg(ap, ssize_t);
		break;
	case _LM_t:
		num = (int64_t) va_arg(ap, ptrdiff_t);
		break;
	default:
		// standard says it's ok to have undefined behavior here
		num = (int64_t) va_arg(ap, int);
		break;
	};
	
	char sign = 0;
	if (num < 0)
	{
		sign = '-';
		num = -num;
	};
	
	if (precision == -1)
	{
		// "The default precision is 1"
		precision = 1;
	};
	
	char *put = convbuf + bufsize - 1;
	while (num != 0)
	{
		int64_t digit = num % base;
		num /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + firstLetter;
		};
		
		*(--put) = c;
	};
	
	if (sign == 0)
	{
		if (flags & FLAG_SIGN)
		{
			sign = '+';
		}
		else if (flags & FLAG_SPACE)
		{
			sign = ' ';
		};
	};
	
	int zerofill = 0;
	if (strlen(put) < precision)
	{
		zerofill = precision - strlen(put);
	};
	
	if ((flags & FLAG_ZEROPAD) && ((flags & FLAG_LEFTADJ) == 0))
	{
		if ((zerofill+strlen(put)) < fieldWidth)
		{
			zerofill = fieldWidth - strlen(put);
		};
	};
	
	if (sign != 0)
	{
		zerofill--;
	};
	
	if (zerofill < 0) zerofill = 0;
	while (zerofill--)
	{
		(*--put) = '0';
	};
	
	if (sign != 0)
	{
		*(--put) = sign;
	};
	
	__printf_putfield(state, put, strlen(put), flags, fieldWidth);
};

static void __printf_conv_unsigned(FormatterState *state, int flags, int lenmod, int fieldWidth, int precision, int base, char firstLetter, va_list ap)
{
	size_t bufsize = 64;
	char *convbuf;
	if (precision > 60)
	{
		convbuf = (char*) kalloca(precision+4);
		bufsize = precision + 4;
	}
	else
	{
		convbuf = (char*) kalloca(64);
	};
	
	memset(convbuf, 0, bufsize);
	
	uint64_t num = 0;
	switch (lenmod)
	{
	case _LM_NONE:
		num = (uint64_t) va_arg(ap, unsigned int);
		break;
	case _LM_hh:
		num = (uint64_t) va_arg(ap, unsigned int);	// char
		break;
	case _LM_h:
		num = (uint64_t) va_arg(ap, unsigned int);	// short
		break;
	case _LM_l:
		num = (uint64_t) va_arg(ap, unsigned long);
		break;
	case _LM_ll:
		num = (uint64_t) va_arg(ap, unsigned long long);
		break;
	case _LM_j:
		num = (uint64_t) va_arg(ap, uintmax_t);
		break;
	case _LM_z:
		num = (uint64_t) va_arg(ap, size_t);
		break;
	case _LM_t:
		num = (uint64_t) va_arg(ap, uint64_t);
		break;
	default:
		// standard says it's ok to have undefined behavior here
		num = (uint64_t) va_arg(ap, unsigned int);
		break;
	};
	
	if (precision == -1)
	{
		// "The default precision is 1"
		precision = 1;
	};
	
	char *put = convbuf + bufsize - 1;
	while (num != 0)
	{
		uint64_t digit = num % base;
		num /= base;
		
		char c;
		if (digit < 10)
		{
			c = (char) digit + '0';
		}
		else
		{
			c = (char) digit - 10 + firstLetter;
		};
		
		*(--put) = c;
	};
	
	int zerofill = 0;
	if (strlen(put) < precision)
	{
		zerofill = precision - strlen(put);
	};
	
	if ((flags & FLAG_ZEROPAD) && ((flags & FLAG_LEFTADJ) == 0))
	{
		if ((zerofill+strlen(put)) < fieldWidth)
		{
			zerofill = fieldWidth - strlen(put);
		};
	};
	
	if (zerofill < 0) zerofill = 0;
	while (zerofill--)
	{
		(*--put) = '0';
	};
	
	__printf_putfield(state, put, strlen(put), flags, fieldWidth);
};

static void __printf_conv_c(FormatterState *state, va_list ap)
{
	__putc(state, va_arg(ap, int));
};

static void __printf_conv_s(FormatterState *state, int flags, int lenmod, int fieldWidth, int precision, va_list ap)
{
	char *str = va_arg(ap, char*);
	
	size_t len = strlen(str);
	if ((size_t)precision < len)
	{
		len = (size_t) precision;
	};
	
	__printf_putfield(state, str, len, flags, fieldWidth);
};

static inline int __is_digit(char c)
{
	return c >= '0' && c <= '9';
};

static int __parse_width(const char **fmtptr)
{
	const char *fmt = *fmtptr;
	if (!__is_digit(*fmt))
	{
		return -1;
	};

	int result = 0;
	while (__is_digit(*fmt))
	{
		int digit = (*fmt) - '0';
		result = result * 10 + digit;
		fmt++;
	};

	*fmtptr = fmt;
	return result;
};

size_t kvsnprintf(char *buffer, size_t size, const char *fmt, va_list ap)
{
	FormatterState state;
	state.put = buffer;
	state.charsLeft = size-1;
	state.charsPrinted = 0;

	while ((*fmt) != 0)
	{
		if (*fmt != '%')
		{
			__putc(&state, *fmt);
			fmt++;
		}
		else
		{
			fmt++;
			if (*fmt == '%')
			{
				__putc(&state, '%');
				fmt++;
				continue;
			};
			
			int flags = 0;
			int lenmod = _LM_NONE;
			int fieldWidth = -1;
			int precision = -1;
			
			// parse all flags (this returns 0 when a non-flag character is seen)
			while (__parse_flag(&fmt, &flags));
			
			// field width and precision
			fieldWidth = __parse_width(&fmt);
			if ((*fmt) == '.')
			{
				fmt++;
				precision = __parse_width(&fmt);
			};
			
			// parse length modifiers if any
			// (only advance the fmt pointer if a modifier was parsed).
			switch (*fmt)
			{
			case 'h':
				lenmod = _LM_h;
				fmt++;
				if ((*fmt) == 'h')
				{
					lenmod = _LM_hh;
					fmt++;
				};
				break;
			case 'l':
				lenmod = _LM_l;
				fmt++;
				if ((*fmt) == 'l')
				{
					lenmod = _LM_ll;
					fmt++;
				};
				break;
			case 'j':
				lenmod = _LM_j;
				fmt++;
				break;
			case 'z':
				lenmod = _LM_z;
				fmt++;
				break;
			case 't':
				lenmod = _LM_t;
				fmt++;
				break;
			case 'L':
				lenmod = _LM_L;
				fmt++;
				break;
			};
			
			// finally the conversion specification part!
			// NOTE: we increase the fmt pointer at the very start, in the switch statement.
			// do not forget this!
			char spec = *fmt++;
			switch (spec)
			{
			case 'd':
			case 'i':
				__printf_conv_signed(&state, flags, lenmod, fieldWidth, precision, 10, 'a', ap);
				break;
			case 'o':
				__printf_conv_unsigned(&state, flags, lenmod, fieldWidth, precision, 8, 'a', ap);
				break;
			case 'u':
				__printf_conv_unsigned(&state, flags, lenmod, fieldWidth, precision, 10, 'a', ap);
				break;
			case 'x':
				__printf_conv_unsigned(&state, flags, lenmod, fieldWidth, precision, 16, 'a', ap);
				break;
			case 'X':
				__printf_conv_unsigned(&state, flags, lenmod, fieldWidth, precision, 16, 'A', ap);
				break;
			case 'c':
				__printf_conv_c(&state, ap);
				break;
			case 's':
				__printf_conv_s(&state, flags, lenmod, fieldWidth, precision, ap);
				break;
			case 'p':
				__printf_conv_unsigned(&state, flags, _LM_l, fieldWidth, precision, 16, 'a', ap);
				break;
			};
		};
	};

	*state.put = 0;
	return state.charsPrinted;
};

size_t ksnprintf(char *buffer, size_t size, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	size_t result = kvsnprintf(buffer, size, fmt, ap);
	va_end(ap);
	return result;
};