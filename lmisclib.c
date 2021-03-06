/**
 * Lua extension compatible with PHP's pack/unpack, trim/ltrim/rtrim and etc. etc.
 * @author microwish@gmail.com
 */
#include <lua.h>
#include <lauxlib.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

/* Whether machine is little endian */
char machine_little_endian;

/* Mapping of byte from char (8bit) to long for machine endian */
static int byte_map[1];

/* Mappings of bytes from int (machine dependant) to int for machine endian */
static int int_map[sizeof(int)];

/* Mappings of bytes from shorts (16bit) for all endian environments */
static int machine_endian_short_map[2];
static int big_endian_short_map[2];
static int little_endian_short_map[2];

/* Mappings of bytes from longs (32bit) for all endian environments */
static int machine_endian_long_map[4];
static int big_endian_long_map[4];
static int little_endian_long_map[4];

static void check_endian(void)
{
	int machine_endian_check = 1;
	size_t i;

	machine_little_endian = ((char *)&machine_endian_check)[0];

	//little endian
	if (machine_little_endian) {
		//Where to get low to high bytes from
		byte_map[0] = 0;

		for (i = 0; i < sizeof(int); i++)
			int_map[i] = i;

		machine_endian_short_map[0] = 0;
		machine_endian_short_map[1] = 1;
		big_endian_short_map[0] = 1;
		big_endian_short_map[1] = 0;
		little_endian_short_map[0] = 0;
		little_endian_short_map[1] = 1;

		machine_endian_long_map[0] = 0;
		machine_endian_long_map[1] = 1;
		machine_endian_long_map[2] = 2;
		machine_endian_long_map[3] = 3;
		big_endian_long_map[0] = 3;
		big_endian_long_map[1] = 2;
		big_endian_long_map[2] = 1;
		big_endian_long_map[3] = 0;
		little_endian_long_map[0] = 0;
		little_endian_long_map[1] = 1;
		little_endian_long_map[2] = 2;
		little_endian_long_map[3] = 3;
	} else {
		size_t size = sizeof(long);

		//Where to get high to low bytes from
		byte_map[0] = size - 1;

		for (i = 0; i < sizeof(int); i++)
			int_map[i] = size - (sizeof(int) - i);

		machine_endian_short_map[0] = size - 2;
		machine_endian_short_map[1] = size - 1;
		big_endian_short_map[0] = size - 2;
		big_endian_short_map[1] = size - 1;
		little_endian_short_map[0] = size - 1;
		little_endian_short_map[1] = size - 2;

		machine_endian_long_map[0] = size - 4;
		machine_endian_long_map[1] = size - 3;
		machine_endian_long_map[2] = size - 2;
		machine_endian_long_map[3] = size - 1;
		big_endian_long_map[0] = size - 4;
		big_endian_long_map[1] = size - 3;
		big_endian_long_map[2] = size - 2;
		big_endian_long_map[3] = size - 1;
		little_endian_long_map[0] = size - 1;
		little_endian_long_map[1] = size - 2;
		little_endian_long_map[2] = size - 3;
		little_endian_long_map[3] = size - 4;
	}
}

//XXX:
//this macro's parameters were refered more than once within the macro definition
#define INC_OUTPUT_POS(a, b) \
	if ((a) < 0 || ((INT_MAX - output_pos) / (int)(b)) < (a)) { \
		free(fmt_codes); \
		free(fmt_args); \
		lua_pushboolean(L, 0); \
		lua_pushfstring(L, "Type %c: integer overflow in format string", code); \
		return 2; \
	} \
	output_pos += (a) * (b);

//compromise with PHP
static inline long convert_double_to_long(double d)
{
	if (sizeof(long) < sizeof(double)) {
		if (d > LONG_MAX || d < LONG_MIN)
			return (long)(unsigned long)(long long)d;
	} else {
		if (d > LONG_MAX)
			return (long)(unsigned long)d;
	}
	return (long)d;
}

static void do_pack(long val, size_t size, int *map, char *output)
{
	size_t i;
	char *v;

	v = (char *)&val;
	for (i = 0; i < size; i++)
		*output++ = v[map[i]];
}

//XXX
#define CONVERT_TO_STRING(L, arg_n) \
	switch (lua_type((L), (arg_n))) { \
		case LUA_TSTRING: \
			break; \
		case LUA_TNUMBER: \
		{ \
			double v = lua_tonumber((L), (arg_n)); \
			/* FIXME: possible overflow and bad return value from sprintf */ \
			char tmp[32]; \
			int l = sprintf(tmp, "%.*G", 6, v); \
			lua_pushlstring((L), tmp, (size_t)l); \
			lua_replace((L), (arg_n)); \
			break; \
		} \
		case LUA_TBOOLEAN: \
			if (lua_toboolean((L), (arg_n))) { \
				lua_pushliteral((L), "1"); \
			} else { \
				lua_pushliteral((L), ""); \
			} \
			lua_replace((L), (arg_n)); \
			break; \
		case LUA_TTABLE: \
			break; \
		case LUA_TNIL: \
			lua_pushliteral((L), ""); \
			lua_replace((L), (arg_n)); \
			break; \
	}

//XXX
#define CONVERT_TO_LONG(L, arg_n) \
	switch (lua_type((L), (arg_n))) { \
		case LUA_TSTRING: \
			v = strtol(lua_tostring((L), (arg_n)), NULL, 10); \
			break; \
		case LUA_TNUMBER: { \
			lua_Number tmp = lua_tonumber((L), (arg_n)); \
			v = convert_double_to_long(tmp); \
		} \
			break; \
		case LUA_TNIL: \
			v = 0; \
			break; \
		case LUA_TBOOLEAN: \
			v = (long)lua_toboolean((L), (arg_n)); \
			break; \
		case LUA_TTABLE: \
			lua_pushnil(L); \
			if (lua_next((L), (arg_n))) { \
				lua_pop((L), 2); \
				v = 1; \
			} else { \
				v = 0; \
			} \
			break; \
		case LUA_TUSERDATA: \
			v = (long)lua_touserdata((L), (arg_n)); \
			break; \
	}

#if 0
//XXX
#define CONVERT_TO_DOUBLE(L, arg_n) \
	switch (lua_type((L), (arg_n))) { \
		case LUA_TSTRING: \
			v = strtod(lua_tostring((L), (arg_n)), NULL); \
			break; \
		case LUA_TNUMBER: \
			v = lua_tonumber((L), (arg_n)); \
			break; \
		case LUA_TNIL: \
			v = 0.0; \
			break; \
		case LUA_TBOOLEAN: \
			v = lua_toboolean((L), (arg_n)); \
			break; \
		case LUA_TTABLE: \
			lua_pushnil(L); \
			if (lua_next((L), (arg_n))) { \
				lua_pop((L), 2); \
				v = 1; \
			} else { \
				v = 0; \
			} \
			break; \
		case LUA_TUSERDATA: \
			v = lua_touserdata((L), (arg_n)); \
			break; \
	}
#endif

static int pack(lua_State *L)
{
	int num_args,//total number of args
		arg_n,//# for current arg
		i;
	const char *fmtstr = NULL;//arg#1, format string
	size_t fmtlen;//length of the format string
	char *fmt_codes = NULL;//redundant container for format codes
	int *fmt_args = NULL;//redundant container for repeater arg of format codes
	int fmtcnt;

	if ((num_args = lua_gettop(L)) < 2) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "2 arguments required at least");
		return 2;
	}

	if (LUA_TSTRING != lua_type(L, 1)) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#1 must be a string");
		return 2;
	}

	fmtstr = lua_tolstring(L, 1, &fmtlen);
	if (fmtlen < 1) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "arg#1 empty");
		return 2;
	}
	arg_n = 1;

	if (!(fmt_codes = calloc(fmtlen, sizeof(char)))) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Allocating buffer for format codes failed");
		return 2;
	}
	if (!(fmt_args = calloc(fmtlen, sizeof(int)))) {
		free(fmt_codes);
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Allocating buffer for format args failed");
		return 2;
	}

	for (i = 0, fmtcnt = 0; i < fmtlen; fmtcnt++) {
		char code = fmtstr[i++];
		int arg = 1;//repeat 1 by default

		//handle format args if any
		if (i < fmtlen) {
			if ('*' == fmtstr[i]) {
				i++;
				arg = -1;
			} else if (fmtstr[i] >= '0' && fmtstr[i] <= '9') {
				arg = atoi(fmtstr + i++);
				//skip all digits consumed by atoi
				while (fmtstr[i] >= '0' && fmtstr[i] <= '9' && i < fmtlen)
					i++;
			}
		}

		switch ((int)code) {
			//never uses any args
			case 'x':
			case 'X':
			case '@':
				if (arg < 0)
					arg = 1;//i.e. repeat once
				break;
			case 'a':
			case 'A':
			case 'h':
			case 'H':
				if (arg_n >= num_args) {
					free(fmt_codes);
					free(fmt_args);
					lua_pushboolean(L, 0);
					lua_pushfstring(L, "Type %c: not enought arguments", code);
					return 2;
				}

				arg_n++;
				luaL_checkany(L, arg_n);
				if (arg < 0) {
					CONVERT_TO_STRING(L, arg_n)
					arg = lua_objlen(L, arg_n);
				}
				arg_n++;
				break;

			//use as many args as specified
			case 'c':
			case 'C':
			case 's':
			case 'S':
			case 'n':
			case 'v':
			case 'i':
			case 'I':
			case 'l':
			case 'L':
			case 'N':
			case 'V':
			case 'f':
			case 'd':
				if (arg < 0)
					arg = num_args - arg_n;

				if ((arg_n += arg) > num_args) {
					free(fmt_codes);
					free(fmt_args);
					lua_pushboolean(L, 0);
					lua_pushfstring(L, "Type %c: too few arguments", code);
					return 2;
				}
				break;

			default:
				free(fmt_codes);
				free(fmt_args);
				lua_pushboolean(L, 0);
				lua_pushfstring(L, "Type %c: unknown format code", code);
				return 2;
		}

		fmt_codes[fmtcnt] = code;
		fmt_args[fmtcnt] = arg;
	}

	int output_pos = 0, output_size = 0;

	for (i = 0; i < fmtcnt; i++) {
		char code = fmt_codes[i];
		int arg = fmt_args[i];

		switch ((int)code) {
			case 'h':
			case 'H':
				INC_OUTPUT_POS((arg + arg % 2) / 2, 1)
				break;
			case 'a':
			case 'A':
			case 'c':
			case 'C':
			case 'x':
				INC_OUTPUT_POS(arg, 1)
				break;
			case 's':
			case 'S':
			case 'n':
			case 'v':
				INC_OUTPUT_POS(arg, 2)
				break;
			case 'i':
			case 'I':
				INC_OUTPUT_POS(arg, sizeof(int))
				break;
			case 'l':
			case 'L':
			case 'N':
			case 'V':
				INC_OUTPUT_POS(arg, 4)
				break;
			case 'f':
				INC_OUTPUT_POS(arg, sizeof(float))
				break;
			case 'd':
				INC_OUTPUT_POS(arg, sizeof(double))
				break;
			case 'X':
				if ((output_pos -= arg) < 0)
					output_pos = 0;
				break;
			case '@':
				output_pos = arg;
				break;
		}

		if (output_size < output_pos)
			output_size = output_pos;
	}

	char *output = malloc(output_size);
	if (!output) {
		free(fmt_codes);
		free(fmt_args);
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Allocating for output failed");
		return 2;
	}

	output_pos = 0;
	arg_n = 2;
	for (i = 0; i < fmtcnt; i++) {
		char code = fmt_codes[i];
		int arg = fmt_args[i];
		const char *data = NULL;
		size_t l;

		switch ((int)code) {
			case 'a':
			case 'A':
				memset(output + output_pos, code == 'a' ? '\0' : ' ', arg);
				data = lua_tolstring(L, arg_n++, &l);
				memcpy(output + output_pos, data, l < arg ? l : arg);
				output_pos += arg;
				break;

			case 'h':
			case 'H':
			{
				int nibbleshift = code == 'h' ? 0 : 4;
				int first = 1;

				data = lua_tolstring(L, arg_n++, &l);
				output_pos--;
				if (arg > l)
					arg = l;

				while (arg-- > 0) {
					char n = *data++;

					if (n >= '0' && n <= '9') {
						n -= '0';
					} else if (n >= 'A' && n <= 'Z') {
						n -= (n - 'A');
					} else if (n >= 'a' && n <= 'z') {
						n -= (n - 'a');
					} else {
						n = 0;
					}

					if (first--) {
						output[++output_pos] = 0;
					} else {
						first = 1;
					}

					output[output_pos] |= (n << nibbleshift);
					nibbleshift = (nibbleshift + 4) & 7;
				}

				output_pos++;
				break;
			}

			case 'c':
			case 'C':
			{
				long v;
				while (arg-- > 0) {
					CONVERT_TO_LONG(L, arg_n)
					arg_n++;
					do_pack(v, 1, byte_map, output + output_pos);
					output_pos++;
				}
				break;
			}

			case 's':
			case 'S':
			case 'n':
			case 'v':
			{
				int *map = machine_endian_short_map;
				long v;

				if (code == 'n') {
					map = big_endian_short_map;
				} else if (code == 'v') {
					map = little_endian_short_map;
				}

				while (arg-- > 0) {
					CONVERT_TO_LONG(L, arg_n)
					arg_n++;
					do_pack(v, 2, map, output + output_pos);
					output_pos += 2;
				}
				break;
			}

			case 'i':
			case 'I':
			{
				long v;
				while (arg-- > 0) {
					CONVERT_TO_LONG(L, arg_n)
					arg_n++;
					do_pack(v, sizeof(int), int_map, output + output_pos);
					output_pos += sizeof(int);
				}
			}
				break;
			case 'l':
			case 'L':
			case 'N':
			case 'V': {
				int *map = machine_endian_long_map;
				long v;

				if (code == 'N') {
					map = big_endian_long_map;
				} else if (code == 'V') {
					map = little_endian_long_map;
				}

				while (arg-- > 0) {
					CONVERT_TO_LONG(L, arg_n)
					arg_n++;
					do_pack(v, 4, map, output + output_pos);
					output_pos += 4;
				}
				 break;
			}
			case 'f':
			{
				float v;

				while (arg-- > 0) {
					//CONVERT_TO_DOUBLE(L, arg_n);
					//v = (float)v;
					v = (float)strtod(lua_tostring(L, arg_n), NULL);
					memcpy(output + output_pos, &v, sizeof(v));
					output_pos += sizeof(v);
				}
				break;
			}

			case 'd':
			{
				double v;

				while (arg-- > 0) {
					//CONVERT_TO_DOUBLE(L, arg_n);
					v = strtod(lua_tostring(L, arg_n), NULL);
					memcpy(output + output_pos, &v, sizeof(v));
					output_pos += sizeof(v);
				}
				break;
			}

			case 'x':
				memset(output + output_pos, '\0', arg);
				output_pos += arg;
				break;
			case 'X':
				if ((output_pos -= arg) < 0)
					output_pos = 0;
				break;
			case '@':
				if (arg > output_pos)
					memset(output + output_pos, '\0', arg - output_pos);
				output_pos = arg;
				break;
		}
	}

	free(fmt_codes);
	free(fmt_args);
	output[output_pos] = '\0';
	lua_pushlstring(L, output, output_pos);
	free(output);

	return 1;
}

static long do_unpack(const char *data, int size, int issigned, int *map)
{
	long result;
	char *cresult = (char *)&result;
	int i;

	result = issigned ? -1 : 0;

	for (i = 0; i < size; i++)
		cresult[map[i]] = *data++;

	return result;
}

//compromise with PHP escaped characters
//XXX: maybe refer lobject.c:luaO_str2d?
static int str_to_bin(const char *str, char **bin)
{
	if (!str || !bin || !str[0])
		return 0;

	int i;
	size_t len = strlen(str);
	long l;
	const char *cp;

	if (len < 2)
		return 0;

	//hex-like string \x04\x00\xa0\x00 of PHP-style
	//\\x04\\x00\\xa0\\x00 of Lua-style
	//TODO
	if (str[0] == '\\'
			&& str[1] == 'x'
			&& len > 2
			&& ((str[2] >= '0' && str[2] <= '9')
				|| (str[2] >= 'a' && str[2] <= 'f')
				|| (str[2] >= 'A' && str[2] <= 'F'))) {

		int x, pos[len / 3 + 1]/*c99*/;

		memset(pos, 0, sizeof(pos));
		pos[0] = 1;//where first x occured
		for (i = 3, x = 1; str[i]; i++) {
			if (str[i] == 'x')
				pos[x++] = i;
		}

		//XXX: which is better, stack or heap
		if (!(*bin = calloc(x, sizeof(char))))
			return 0;

		for (x = 0; pos[x] > 0; x++) {
			//TODO: byte order, here supposing little endian
			l = strtol(&str[pos[x] + 1], NULL, 16);
			cp = (const char *)&l;
			memcpy((*bin) + x, &cp[0], sizeof(char));
		}

		return x;
	}

	return 0;
}

static int unpack(lua_State *L)
{
	if (lua_gettop(L) != 2//strictly two
		|| lua_type(L, 1) != LUA_TSTRING
		|| lua_type(L, 2) != LUA_TSTRING) {
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "2 string arguments required");
		return 2;
	}

	const char *format/*format string*/, *input/*packed string to be unpakced*/;
	int formatlen, inputlen, inputpos/*cursor while processing packed string*/;
	char *input_bin = NULL;

	//TODO: lua_tolstring's bug??
	format = lua_tolstring(L, 1, (size_t *)&formatlen);
	input = lua_tolstring(L, 2, NULL);
	//format = lua_tolstring(L, 1, (size_t *)&formatlen);
#if 0
	return luaL_error(L, "format: %s, len: %d; input: %s, len: %d", format, formatlen, input, inputlen);
	//return luaL_error(L, "strlen: %d; format: %s, len: %d; input: %s, len: %d", strlen(format), format, formatlen, input, inputlen);
	//return luaL_error(L, "format: %s, len: %d; input: %s, len: %d, for strlen: %d", format, formatlen, input, inputlen, strlen(input));
#endif
	int tmp = 0;
	char s[256] = {0};
	while (format[tmp]) {
		s[tmp] = format[tmp];
		tmp++;
	}
	formatlen = tmp;
	//return luaL_error(L, "format: %s, len: %d; input: %s, len: %d", format, formatlen, input, inputlen);

	inputlen = str_to_bin(input, &input_bin);
	if (!inputlen) {
		if (input_bin) {
			free(input_bin);
			input_bin = NULL;
		}
		lua_pushboolean(L, 0);
		lua_pushliteral(L, "Internal error");
		return 2;
	}

	lua_pop(L, 2);//XXX: pop or not? chances are that they are gc-ed?
	lua_newtable(L);

	inputpos = 0;
	while (formatlen-- > 0) {
		char type = *(format++);//format codes, same as pack
		int arg = 1, argb, i;
		const char *name;//keys of table container for resulted unpacked data
		int namelen, size/*how many bytes from packed input is for current format code*/;

		if (formatlen > 0) {
			if (*format >= '0' && *format <= '9') {
				arg = atoi(format);
				format++;
				formatlen--;
				//skip all that consumed by atoi
				while (*format >= '0' && *format <= '9') {
					format++;
					formatlen--;
				}
			} else if (*format == '*') {
				arg = -1;
				format++;
				formatlen--;
			}
		}

		argb = arg;

		name = format;
		while (formatlen > 0 && *format != '/') {
			format++;
			formatlen--;
		}
		namelen = format - name;
		if (namelen > 200)//hardcoded
			namelen = 200;

		switch ((int)type) {
			case 'X':
				size = -1;
				break;
			case '@':
				size = 0;
				break;
			case 'a':
			case 'A':
				size = arg;
				arg = 1;
				break;
			case 'h'://nibble
			case 'H':
				size = arg > 0 ? (arg + arg % 2) / 2 : arg;
				arg = 1;
				break;
			case 'c':
			case 'C':
			case 'x':
				size = 1;//consume one byte from the input
				break;
			case 's':
			case 'S':
			case 'n':
			case 'v':
				size = 2;
				break;
			case 'i':
			case 'I':
				size = sizeof(int);
				break;
			case 'l':
			case 'L':
			case 'N':
			case 'V':
				size = 4;
				break;
			case 'f':
				size = sizeof(float);
				break;
			case 'd':
				size = sizeof(double);
				break;
			default:
				lua_pop(L, 1);
				if (input_bin) {
					free(input_bin);
					input_bin = NULL;
				}
				lua_pushboolean(L, 0);
				lua_pushfstring(L, "Invalid format type: %c", type);
				return 2;
		}

		//do actual unpacking
		for (i = 0; i != arg; i++) {
			//buffer for name + number, 256 safe as name's length <= 200
			char n[256];
			int l;

			if (arg != 1//squences of key names
				|| namelen == 0/*did not name a key, using index starting from 1*/) {
				l = snprintf(n, sizeof(n), "%.*s%d", namelen, name, i + 1);
			} else {
				l = snprintf(n, sizeof(n), "%.*s", namelen, name);
			}

			if (size != 0 && size != -1 && INT_MAX - size + 1 < inputpos)
				inputpos = 0;

			if (inputpos + size <= inputlen) {
				switch ((int)type) {
					case 'a':
					case 'A':
					{
						char pad = type == 'a' ? '\0' : ' ';
						int len = inputlen - inputpos;//remaining bytes of input

						if (size > 0 && len > size)
							len = size;

						size = len;//real size

						while (--len >= 0) {
							//rtrim padding chars from input packed
							if (input_bin[inputpos + len] != pad)
								break;
						}

						lua_pushlstring(L, n, l);
						lua_pushlstring(L, input_bin + inputpos, len + 1/*should be >= 0*/);
						lua_rawset(L, 1);

						break;
					}
					case 'h':
					case 'H':
					{
						char *buf;
						int len = (inputlen - inputpos) * 2, ipos, opos;
						int nibbleshift, first = 1;

						if (size >= 0 && len > size * 2)
							len = size * 2;

						if (argb > 0)
							len -= argb % 2;

						if (!(buf = malloc(len + 1))) {
							lua_pop(L, 1);
							if (input_bin) {
								free(input_bin);
								input_bin = NULL;
							}
							lua_pushboolean(L, 0);
							lua_pushliteral(L, "Internal error");
							return 2;
						}

						nibbleshift = type == 'h' ? 0 : 4;
						for (ipos = opos = 0; opos < len; opos++) {
							char c = (input_bin[inputpos + ipos] >> nibbleshift) & 0xf;

							c += (c < 10 ? '0' : ('a' - 10));
							buf[opos] = c;

							nibbleshift = (nibbleshift + 4) & 7;

							if (first-- == 0) {
								ipos++;
								first = 1;
							}
						}
						buf[len] = '\0';

						lua_pushlstring(L, n, l);
						lua_pushlstring(L, buf, len);
						lua_rawset(L, 1);

						free(buf);
						break;
					}
					case 'c':
					case 'C':
					{
						int issigned = type == 'c' ? input_bin[inputpos] & 0x80 : 0;
						long v = do_unpack(input_bin + inputpos, 1, issigned, byte_map);

						lua_pushlstring(L, n, l);
						lua_pushinteger(L, v);
						lua_rawset(L, 1);

						break;
					}
					case 's':
					case 'S':
					case 'n':
					case 'v':
					{
						long v;
						int issigned = 0;
						int *map = machine_endian_short_map;

						if (type == 's') {
							issigned = input_bin[inputpos + (machine_little_endian ? 1 : 0)] & 0x80;
						} else if (type == 'n') {
							map = big_endian_short_map;
						} else if (type == 'v') {
							map = little_endian_short_map;
						}

						v = do_unpack(input_bin + inputpos, 2, issigned, map);

						lua_pushlstring(L, n, l);
						lua_pushinteger(L, v);
						lua_rawset(L, 1);

						break;
					}
					case 'i':
					case 'I':
					{
						long v;
						int issigned = 0;

						if (type == 'i')
							issigned = input_bin[inputpos + (machine_little_endian ? sizeof(int) - 1 : 0)] & 0x80;

						v = do_unpack(input_bin + inputpos, sizeof(int), issigned, int_map);

						lua_pushlstring(L, n, l);
						lua_pushnumber(L, v);
						lua_rawset(L, 1);

						break;
					}
					case 'l':
					case 'L':
					case 'N':
					case 'V':
					{
						int issigned = 0;
						int *map = machine_endian_long_map;
						long v = 0;

						if (type == 'l' || type == 'L') {
							issigned = input_bin[inputpos + (machine_little_endian ? 3 : 0)] & 0x80;
						} else if (type == 'N') {
							issigned = input_bin[inputpos] & 0x80;
							map = big_endian_long_map;
						} else if (type == 'V') {
							issigned = input_bin[inputpos + 3] & 0x80;
							map = little_endian_long_map;
						}

						if (sizeof(long) > 4 && issigned)
							v = ~INT_MAX;

						v |= do_unpack(input_bin + inputpos, 4, issigned, map);

						if (sizeof(long) > 4) {
 							if (type == 'l') {
								v = (signed int)v; 
							} else {
								v = (unsigned int)v;
							}
						}

						lua_pushlstring(L, n, l);
						lua_pushnumber(L, v);
						lua_rawset(L, 1);

						break;
					}
					case 'f':
					{
						float v;

						memcpy(&v, input_bin + inputpos, sizeof(float));

						lua_pushlstring(L, n, l);
						lua_pushnumber(L, v);
						lua_rawset(L, 1);

						break;
					}
					case 'd':
					{
						double v;

						memcpy(&v, input_bin + inputpos, sizeof(double));

						lua_pushlstring(L, n, l);
						lua_pushnumber(L, v);
						lua_rawset(L, 1);

						break;
					}
					case 'x':
						break;
					case 'X':
						if (inputpos < size) {
							inputpos = -size;
							i = arg - 1;//Break out of for loop
						}
						break;
					case '@':
						if (arg <= inputlen)
							inputpos = arg;
						i = arg - 1;
						break;
				}

				if ((inputpos += size) < 0) {
					inputpos = 0;
				}
			} else if (arg < 0) {
				break;
			} else {
				lua_pop(L, 1);
				if (input_bin) {
					free(input_bin);
					input_bin = NULL;
				}
				lua_pushboolean(L, 0);
				lua_pushfstring(L, "Type %c: not enough input, need %d, have %d", type, size, inputlen - inputpos);
				return 2;
			}
		}

		formatlen--;
		format++;
	}

	free(input_bin);

	return 1;
}

static inline int setcharmask(const unsigned char *input, int len, char *mask)
{
	//adopted from zend.h
#define SUCCESS 0
#define FAILURE -1

	const unsigned char *end;
	unsigned char c;
	int result = SUCCESS;

	memset(mask, 0, 256);//256 hardcoded. EASCII?

	for (end = input + len; input < end; input++) {
		c = *input;
		//e.g. range of a..z
		if (input + 3 < end && input[1] == '.' && input[2] == '.' && input[3] >= c) {
			memset(mask + c, 1, input[3] - c + 1);
			input += 3;
		} else if (input + 1 < end && input[0] == '.' && input[1] == '.') {
			//to be friendly
			if (end - len >= input//at beginning of input, so no chars to the left of ..
					|| input + 2 >= end//no chars after the ..
					|| input[-1] > input[2]) {//must no desc order
				result = FAILURE;
				continue;
			}
			//other possible error
			result = FAILURE;
			continue;
		} else {
			//the most common case
			mask[c] = 1;
		}
	}

	return result;
}

static const char *do_trim(const char *c, int *lenp, const char *what, int what_len, int mode)
{
	register int i;
	int trimmed = 0, len = *lenp;
	char mask[256];

	if (what) {
		//ignore return value
		setcharmask((const unsigned char *)what, what_len, mask);
	} else {
		setcharmask((const unsigned char *)" \n\r\t\v\0", 6, mask);
	}

	//ltrim
	if (mode & 1) {
		for (i = 0; i < len; i++) {
			if (mask[(unsigned char)c[i]])
				trimmed++;
			else
				break;
		}
		len -= trimmed;
		c += trimmed;
	}
	//rtrim
	if (mode & 2) {
		for (i = len - 1; i >= 0; i--) {
			if (mask[(unsigned char)c[i]])
				len--;
			else
				break;
		}
	}

	*lenp = len;

	return c;
}

static int wrap_trim(lua_State *L, int mode)
{
	const char *charlist, *str;
	size_t listlen, len;
	int n;

	switch ((n = lua_gettop(L))) {
		case 2:
			if (lua_type(L, 2) != LUA_TSTRING) {
				lua_pushboolean(L, 0);
				lua_pushliteral(L, "arg#2 error");
				return 2;
			}
			charlist = lua_tolstring(L, 2, &listlen);
		case 1:
			if (lua_type(L, 1) != LUA_TSTRING) {
				lua_pushboolean(L, 0);
				lua_pushliteral(L, "arg#1 error");
				return 2;
			}
			str = lua_tolstring(L, 1, &len);
			break;
		default:
			lua_pushboolean(L, 0);
			lua_pushliteral(L, "Arguments error");
			return 2;
	}
	lua_pop(L, n);

	str = do_trim(str, (int *)&len, charlist, listlen, mode);

	lua_pushlstring(L, str, len);

	return 1;
}

static int trim(lua_State *L)
{
	return wrap_trim(L, 3);
}

static int ltrim(lua_State *L)
{
	return wrap_trim(L, 1);
}

static int rtrim(lua_State *L)
{
	return wrap_trim(L, 2);
}

static lua_State *getthread(lua_State *L, int *arg)
{
	if (lua_isthread(L, 1)) {
		*arg = 1;
		return lua_tothread(L, 1);
	} else {
		*arg = 0;
		return L;
	}
}

/**
 * debug.traceback originally in the form of string:
stack traceback:
	1.lua:23: in function 'p_dbg'
	1.lua:26: in main chunk
	[C]: ?

 * @proto LUA_TTABLE traceback_retarr([LUA_TTHREAD thread,] [LUA_TNUMBER level])
 * {
 * 	{"file":, "line":, "function":},
 * 	{"file":, "line":, "function":}
 * }
*/
static int traceback_retarr(lua_State *L)
{
#define LEVELS1	12 /* size of the first part of the stack */
#define LEVELS2	10 /* size of the second part of the stack */

	int level,//stack depth level*/
		firstpart = 1,//stack part
		arg, i;
	lua_State *L1 = getthread(L, &arg);//thread to inspect
	lua_Debug ar;

	if (lua_isnumber(L, arg + 1)) {
		level = (int)lua_tointeger(L, arg + 1);
		lua_pop(L, 1);
	} else {
		level = L1 == L ? 1 : 0;//level 0 stands for this own function
	}

	lua_newtable(L);

	i = 1;
	while (lua_getstack(L, level++, &ar)) {
		if (level > LEVELS1 && firstpart) {
			//no more than LEVELS2 levels
			if (!lua_getstack(L, level + LEVELS2, &ar)) {
				level--;
			} else {
				while (lua_getstack(L, level + LEVELS2, &ar))
					level++;
			}
			firstpart = 0;
			continue;
		}

		lua_createtable(L, 0, 3);

		lua_getinfo(L, "Snl", &ar);

		if (ar.currentline > 0) {
			lua_pushstring(L, ar.short_src);
			lua_setfield(L, -2, "file");
			lua_pushinteger(L, ar.currentline);
			lua_setfield(L, -2, "line");
			lua_pushstring(L, ar.name ? ar.name : "?");
			lua_setfield(L, -2, "function");
		}

		lua_rawseti(L, -2, i++);
	}

	return 1;
}

static const luaL_Reg misc_lib[] = {
	{ "pack", pack },
	{ "unpack", unpack },
	{ "trim", trim },
	{ "ltrim", ltrim },
	{ "rtrim", rtrim },
	{ "traceback_retarr", traceback_retarr },
	{ NULL, NULL }
};

static int settablereadonly(lua_State *L)
{
	return luaL_error(L, "Must not update a read-only table");
}

#define LUA_MISCLIBNAME "misc"

LUA_API int luaopen_misc(lua_State *L)
{
	check_endian();

	//main table for this module
	lua_newtable(L);

	//metatable for the main table
	lua_createtable(L, 0, 2);

	luaL_register(L, LUA_MISCLIBNAME, misc_lib);

	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, settablereadonly);
	lua_setfield(L, -2, "__newindex");

	lua_setmetatable(L, -2);

	return 1;
}
