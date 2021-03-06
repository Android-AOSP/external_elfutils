/* Return unsigned constant represented by attribute.
   Copyright (C) 2003-2012 Red Hat, Inc.
   This file is part of Red Hat elfutils.
   Written by Ulrich Drepper <drepper@redhat.com>, 2003.

   Red Hat elfutils is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by the
   Free Software Foundation; version 2 of the License.

   Red Hat elfutils is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with Red Hat elfutils; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301 USA.

   In addition, as a special exception, Red Hat, Inc. gives You the
   additional right to link the code of Red Hat elfutils with code licensed
   under any Open Source Initiative certified open source license
   (http://www.opensource.org/licenses/index.php) which requires the
   distribution of source code with any binary distribution and to
   distribute linked combinations of the two.  Non-GPL Code permitted under
   this exception must only link to the code of Red Hat elfutils through
   those well defined interfaces identified in the file named EXCEPTION
   found in the source code files (the "Approved Interfaces").  The files
   of Non-GPL Code may instantiate templates or use macros or inline
   functions from the Approved Interfaces without causing the resulting
   work to be covered by the GNU General Public License.  Only Red Hat,
   Inc. may make changes or additions to the list of Approved Interfaces.
   Red Hat's grant of this exception is conditioned upon your not adding
   any new exceptions.  If you wish to add a new Approved Interface or
   exception, please contact Red Hat.  You must obey the GNU General Public
   License in all respects for all of the Red Hat elfutils code and other
   code used in conjunction with Red Hat elfutils except the Non-GPL Code
   covered by this exception.  If you modify this file, you may extend this
   exception to your version of the file, but you are not obligated to do
   so.  If you do not wish to provide this exception without modification,
   you must delete this exception statement from your version and license
   this file solely under the GPL without exception.

   Red Hat elfutils is an included package of the Open Invention Network.
   An included package of the Open Invention Network is a package for which
   Open Invention Network licensees cross-license their patents.  No patent
   license is granted, either expressly or impliedly, by designation as an
   included package.  Should you wish to participate in the Open Invention
   Network licensing program, please visit www.openinventionnetwork.com
   <http://www.openinventionnetwork.com>.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <dwarf.h>
#include "libdwP.h"

internal_function unsigned char *
__libdw_formptr (Dwarf_Attribute *attr, int sec_index,
		 int err_nodata, unsigned char **endpp,
		 Dwarf_Off *offsetp)
{
  if (attr == NULL)
    return NULL;

  const Elf_Data *d = attr->cu->dbg->sectiondata[sec_index];
  if (unlikely (d == NULL))
    {
      __libdw_seterrno (err_nodata);
      return NULL;
    }

  Dwarf_Word offset;
  if (attr->form == DW_FORM_sec_offset)
    {
      if (__libdw_read_offset (attr->cu->dbg, cu_sec_idx (attr->cu), attr->valp,
			       attr->cu->offset_size, &offset, sec_index, 0))
	return NULL;
    }
  else if (attr->cu->version > 3)
    goto invalid;
  else
    switch (attr->form)
      {
      case DW_FORM_data4:
      case DW_FORM_data8:
	if (__libdw_read_offset (attr->cu->dbg, cu_sec_idx (attr->cu),
				 attr->valp,
				 attr->form == DW_FORM_data4 ? 4 : 8,
				 &offset, sec_index, 0))
	  return NULL;
	break;

      default:
	if (INTUSE(dwarf_formudata) (attr, &offset))
	  return NULL;
      };

  unsigned char *readp = d->d_buf + offset;
  unsigned char *endp = d->d_buf + d->d_size;
  if (unlikely (readp >= endp))
    {
    invalid:
      __libdw_seterrno (DWARF_E_INVALID_DWARF);
      return NULL;
    }

  if (endpp != NULL)
    *endpp = endp;
  if (offsetp != NULL)
    *offsetp = offset;
  return readp;
}

int
dwarf_formudata (attr, return_uval)
     Dwarf_Attribute *attr;
     Dwarf_Word *return_uval;
{
  if (attr == NULL)
    return -1;

  const unsigned char *datap;

  switch (attr->form)
    {
    case DW_FORM_data1:
      *return_uval = *attr->valp;
      break;

    case DW_FORM_data2:
      *return_uval = read_2ubyte_unaligned (attr->cu->dbg, attr->valp);
      break;

    case DW_FORM_data4:
    case DW_FORM_data8:
    case DW_FORM_sec_offset:
      /* Before DWARF4 data4 and data8 are pure constants unless the
	 attribute also allows offsets (*ptr classes), since DWARF4
	 they are always just constants (start_scope is special though,
	 since it only could express a rangelist since DWARF4).  */
      if (attr->form == DW_FORM_sec_offset
	  || (attr->cu->version < 4 && attr->code != DW_AT_start_scope))
	{
	  switch (attr->code)
	    {
	    case DW_AT_data_member_location:
	    case DW_AT_frame_base:
	    case DW_AT_location:
	    case DW_AT_return_addr:
	    case DW_AT_segment:
	    case DW_AT_static_link:
	    case DW_AT_string_length:
	    case DW_AT_use_location:
	    case DW_AT_vtable_elem_location:
	      /* loclistptr */
	      if (__libdw_formptr (attr, IDX_debug_loc,
				   DWARF_E_NO_LOCLIST, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    case DW_AT_macro_info:
	      /* macptr */
	      if (__libdw_formptr (attr, IDX_debug_macinfo,
				   DWARF_E_NO_ENTRY, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    case DW_AT_ranges:
	    case DW_AT_start_scope:
	      /* rangelistptr */
	      if (__libdw_formptr (attr, IDX_debug_ranges,
				   DWARF_E_NO_DEBUG_RANGES, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    case DW_AT_stmt_list:
	      /* lineptr */
	      if (__libdw_formptr (attr, IDX_debug_line,
				   DWARF_E_NO_DEBUG_LINE, NULL,
				   return_uval) == NULL)
		return -1;
	      break;

	    default:
	      /* sec_offset can only be used by one of the above attrs.  */
	      if (attr->form == DW_FORM_sec_offset)
		{
		  __libdw_seterrno (DWARF_E_INVALID_DWARF);
		  return -1;
		}

	      /* Not one of the special attributes, just a constant.  */
	      if (__libdw_read_address (attr->cu->dbg, cu_sec_idx (attr->cu),
					attr->valp,
					attr->form == DW_FORM_data4 ? 4 : 8,
					return_uval))
		return -1;
	      break;
	    }
	}
      else
	{
	  /* We are dealing with a constant data4 or data8.  */
	  if (__libdw_read_address (attr->cu->dbg, cu_sec_idx (attr->cu),
				    attr->valp,
				    attr->form == DW_FORM_data4 ? 4 : 8,
				    return_uval))
	    return -1;
	}
      break;

    case DW_FORM_sdata:
      datap = attr->valp;
      get_sleb128 (*return_uval, datap);
      break;

    case DW_FORM_udata:
      datap = attr->valp;
      get_uleb128 (*return_uval, datap);
      break;

    default:
      __libdw_seterrno (DWARF_E_NO_CONSTANT);
      return -1;
    }

  return 0;
}
INTDEF(dwarf_formudata)
