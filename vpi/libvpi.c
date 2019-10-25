/*
 * Copyright (c) 2019 Martin Whitaker (icarus@martin-whitaker.me.uk)
 *
 *    This source code is free software; you can redistribute it
 *    and/or modify it in source code form under the terms of the GNU
 *    General Public License as published by the Free Software
 *    Foundation; either version 2 of the License, or (at your option)
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if defined(__MINGW32__) || defined (__CYGWIN32__)

#include "vpi_user.h"

vpip_routines_s*vpip_routines = 0;

DLLEXPORT void vpip_set_callback(vpip_routines_s*routines)
{
      vpip_routines = routines;
}

#else

void __libvpi_c_dummy_function(void)
{
}

#endif