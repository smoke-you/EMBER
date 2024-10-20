/* Copyright (C) 1991-2024 Free Software Foundation, Inc.

NOTE: This source is derived from an old version taken from the GNU C
Library (glibc).

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 51 Franklin Street - Fifth Floor,
Boston, MA 02110-1301, USA.  */

/*===============================================
 includes
 ===============================================*/

#include	<fnmatch.h>
#include	<ctype.h>

/*===============================================
 private constants
 ===============================================*/

#define	FNM_LEADING_DIR	0x08	/* Ignore /<tail> after Imatch. */
#define	FNM_CASEFOLD	0x10	/* Case insensitive search. */
#define	FNM_IGNORECASE	FNM_CASEFOLD
#define	FNM_FILE_NAME	FNM_PATHNAME
#define	TOLOWER(c)	tolower(c)

/*===============================================
 private data prototypes
 ===============================================*/

/*===============================================
 private function prototypes
 ===============================================*/

/*===============================================
 private global variables
 ===============================================*/

/*===============================================
 public objects
 ===============================================*/

/*===============================================
 external objects
 ===============================================*/

/*===============================================
 public functions
 ===============================================*/

/* Match STRING against the filename pattern PATTERN, returning zero if
 it matches, nonzero if not.  */
int
fnmatch(const char *pattern, const char *string, int flags) {
	register const char *p = pattern, *n = string;
	register unsigned char c;

#define FOLD(c)	((flags & FNM_CASEFOLD) ? TOLOWER (c) : (c))

	while ((c = *p++) != '\0') {
		c = FOLD(c);

		switch (c)
		{
		case '?':
			if (*n == '\0')
			return FNM_NOMATCH;
			else if ((flags & FNM_FILE_NAME) && *n == '/')
			return FNM_NOMATCH;
			else if ((flags & FNM_PERIOD) && *n == '.' &&
			    (n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
			  return FNM_NOMATCH;
			break;

		case '\\':
			if (!(flags & FNM_NOESCAPE))
			{
				c = *p++;
				c = FOLD(c);
			}
			if (FOLD ((unsigned char)*n) != c)
			  return FNM_NOMATCH;
			break;

		case '*':
			if ((flags & FNM_PERIOD) && *n == '.' &&
			    (n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
			  return FNM_NOMATCH;

			for (c = *p++; c == '?' || c == '*'; c = *p++, ++n)
				if (((flags & FNM_FILE_NAME) && *n == '/') ||
				    (c == '?' && *n == '\0'))
				  return FNM_NOMATCH;

			if (c == '\0')
			  return 0;

			{
				unsigned char c1 = (!(flags & FNM_NOESCAPE) && c == '\\') ? *p : c;
				c1 = FOLD(c1);
				for (--p; *n != '\0'; ++n)
					if ((c == '[' || FOLD ((unsigned char)*n) == c1) &&
					    fnmatch(p, n, flags & ~FNM_PERIOD) == 0)
					  return 0;
				return FNM_NOMATCH;
			}

		case '[':
			{
			/* Nonzero if the sense of the character class is inverted.  */
			register int negate;

			if (*n == '\0')
			  return FNM_NOMATCH;

			if ((flags & FNM_PERIOD) && *n == '.' &&
			    (n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
			  return FNM_NOMATCH;

			negate = (*p == '!' || *p == '^');
			if (negate)
			  ++p;

			c = *p++;
			for (;;)
			    {
				register unsigned char cstart = c, cend = c;

				if (!(flags & FNM_NOESCAPE) && c == '\\')
				  cstart = cend = *p++;

				cstart = cend = FOLD(cstart);

				if (c == '\0')
				  /* [ (unterminated) loses.  */
				  return FNM_NOMATCH;

				c = *p++;
				c = FOLD(c);

				if ((flags & FNM_FILE_NAME) && c == '/')
				  /* [/] can never match.  */
				  return FNM_NOMATCH;

				if (c == '-' && *p != ']')
				    {
					cend = *p++;
					if (!(flags & FNM_NOESCAPE) && cend == '\\')
					  cend = *p++;
					if (cend == '\0')
					  return FNM_NOMATCH;
					cend = FOLD(cend);

					c = *p++;
				}

				if (FOLD ((unsigned char)*n) >= cstart
				    && FOLD ((unsigned char)*n) <= cend)
				  goto matched;

				if (c == ']')
				  break;
			}
			if (!negate)
			  return FNM_NOMATCH;
			break;

			matched:
			;
			/* Skip the rest of the [...] that already matched.  */
			while (c != ']')
			{
				if (c == '\0')
				  /* [... (unterminated) loses.  */
				  return FNM_NOMATCH;

				c = *p++;
				if (!(flags & FNM_NOESCAPE) && c == '\\')
				  /* XXX 1003.2d11 is unclear if this is right.  */
				  ++p;
			}
			if (negate)
			  return FNM_NOMATCH;
		}
			break;

		default:
			if (c != FOLD((unsigned char )*n))
			  return FNM_NOMATCH;
		}

		++n;
	}

	if (*n == '\0')
	  return 0;

	if ((flags & FNM_LEADING_DIR) && *n == '/')
	  /* The FNM_LEADING_DIR flag says that "foo*" matches "foobar/frobozz".  */
	  return 0;

	return FNM_NOMATCH;
}

/*===============================================
 private functions
 ===============================================*/

