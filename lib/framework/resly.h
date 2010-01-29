/*
	This file is part of Warzone 2100.
	Copyright (C) 1999-2004  Eidos Interactive
	Copyright (C) 2005-2009  Warzone Resurrection Project

	Warzone 2100 is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Warzone 2100 is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Warzone 2100; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
*/
/*! \file resly.h
 *  \brief Interface to the RES file lex and yacc functions.
 */
#ifndef _resly_h
#define _resly_h

#include "lib/framework/lexer_input.h"

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

/* The initial resource directory and the current resource directory */
extern char aResDir[PATH_MAX];
extern char aCurrResDir[PATH_MAX];

/** Set the current input buffer for the lexer
 */
extern void res_set_extra(YY_EXTRA_TYPE user_defined);

/* Call the yacc parser */
extern int res_parse(void);

/* Destroy the lexer */
extern int res_lex_destroy(void);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif
