/*
 * Copyright (c) 2012-2020 Stephen Williams (steve@icarus.com)
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

# include  "PExpr.h"
# include  "pform_types.h"
# include  "netlist.h"
# include  "netclass.h"
# include  "netdarray.h"
# include  "netenum.h"
# include  "netqueue.h"
# include  "netparray.h"
# include  "netscalar.h"
# include  "netstruct.h"
# include  "netvector.h"
# include  "netmisc.h"
# include  <typeinfo>
# include  "ivl_assert.h"

using namespace std;

/*
 * Elaborations of types may vary depending on the scope that it is
 * done in, so keep a per-scope cache of the results.
 */
ivl_type_t data_type_t::elaborate_type(Design*des, NetScope*scope)
{
	// User-defined types must be elaborated in the context
	// where they were defined.
      if (!name.nil())
	    scope = scope->find_typedef_scope(des, this);

      ivl_assert(*this, scope);
      Definitions*use_definitions = scope;

      map<Definitions*,ivl_type_t>::iterator pos = cache_type_elaborate_.lower_bound(use_definitions);
	  if (pos != cache_type_elaborate_.end() && pos->first == use_definitions)
	     return pos->second;

      ivl_type_t tmp = elaborate_type_raw(des, scope);
      cache_type_elaborate_.insert(pos, pair<NetScope*,ivl_type_t>(scope, tmp));
      return tmp;
}

ivl_type_t data_type_t::elaborate_type_raw(Design*des, NetScope*) const
{
      cerr << get_fileline() << ": internal error: "
	   << "Elaborate method not implemented for " << typeid(*this).name()
	   << "." << endl;
      des->errors += 1;
      return 0;
}

ivl_type_t atom2_type_t::elaborate_type_raw(Design*des, NetScope*) const
{
      switch (type_code) {
	  case 64:
	    if (signed_flag)
		  return &netvector_t::atom2s64;
	    else
		  return &netvector_t::atom2u64;

	  case 32:
	    if (signed_flag)
		  return &netvector_t::atom2s32;
	    else
		  return &netvector_t::atom2u32;

	  case 16:
	    if (signed_flag)
		  return &netvector_t::atom2s16;
	    else
		  return &netvector_t::atom2u16;

	  case 8:
	    if (signed_flag)
		  return &netvector_t::atom2s8;
	    else
		  return &netvector_t::atom2u8;

	  default:
	    cerr << get_fileline() << ": internal error: "
		 << "atom2_type_t type_code=" << type_code << "." << endl;
	    des->errors += 1;
	    return 0;
      }
}

ivl_type_t class_type_t::elaborate_type_raw(Design*des, NetScope*scope) const
{
      if (save_elaborated_type)
	    return save_elaborated_type;
      return scope->find_class(des, name);
}

/*
 * elaborate_type_raw for enumerations is actually mostly performed
 * during scope elaboration so that the enumeration literals are
 * available at the right time. At that time, the netenum_t* object is
 * stashed in the scope so that I can retrieve it here.
 */
ivl_type_t enum_type_t::elaborate_type_raw(Design*, NetScope*scope) const
{
      ivl_assert(*this, scope);
      ivl_type_t tmp = scope->enumeration_for_key(this);
      if (tmp == 0 && scope->unit())
	    tmp = scope->unit()->enumeration_for_key(this);
      return tmp;
}

ivl_type_t vector_type_t::elaborate_type_raw(Design*des, NetScope*scope) const
{
      vector<netrange_t> packed;
      if (pdims.get())
	    evaluate_ranges(des, scope, this, packed, *pdims);

      netvector_t*tmp = new netvector_t(packed, base_type);
      tmp->set_signed(signed_flag);
      tmp->set_isint(integer_flag);

      return tmp;
}

ivl_type_t real_type_t::elaborate_type_raw(Design*, NetScope*) const
{
      switch (type_code_) {
	  case REAL:
	    return &netreal_t::type_real;
	  case SHORTREAL:
	    return &netreal_t::type_shortreal;
      }
      return 0;
}

ivl_type_t string_type_t::elaborate_type_raw(Design*, NetScope*) const
{
      return &netstring_t::type_string;
}

ivl_type_t parray_type_t::elaborate_type_raw(Design*des, NetScope*scope) const
{
      vector<netrange_t>packed;
      if (dims.get())
	    evaluate_ranges(des, scope, this, packed, *dims);

      if (base_type->figure_packed_base_type() == IVL_VT_NO_TYPE) {
		cerr << this->get_fileline() << " error: Packed array ";
		if (!name.nil())
		      cerr << "`" << name << "` ";
		cerr << "base-type `";
		if (base_type->name.nil())
		      cerr << *base_type;
		else
		      cerr << base_type->name;
		cerr << "` is not packed." << endl;
		des->errors++;
      }
      ivl_type_t etype = base_type->elaborate_type(des, scope);

      return new netparray_t(packed, etype);
}

ivl_type_t struct_type_t::elaborate_type_raw(Design*des, NetScope*scope) const
{
      netstruct_t*res = new netstruct_t;

      res->set_line(*this);

      res->packed(packed_flag);
      res->set_signed(signed_flag);

      if (union_flag)
	    res->union_flag(true);

      for (list<struct_member_t*>::iterator cur = members->begin()
		 ; cur != members->end() ; ++ cur) {

	      // Elaborate the type of the member.
	    struct_member_t*curp = *cur;
	    ivl_type_t mem_vec = curp->type->elaborate_type(des, scope);
	    if (mem_vec == 0)
		  continue;

	      // There may be several names that are the same type:
	      //   <data_type> name1, name2, ...;
	      // Process all the member, and give them a type.
	    for (list<decl_assignment_t*>::iterator cur_name = curp->names->begin()
		       ; cur_name != curp->names->end() ;  ++ cur_name) {
		  decl_assignment_t*namep = *cur_name;

		  netstruct_t::member_t memb;
		  memb.name = namep->name;
		  memb.net_type = mem_vec;
		  res->append_member(des, memb);
	    }
      }

      return res;
}

ivl_type_t uarray_type_t::elaborate_type_raw(Design*des, NetScope*scope) const
{

      ivl_type_t btype = base_type->elaborate_type(des, scope);

      assert(dims->size() >= 1);
      list<pform_range_t>::const_iterator cur = dims->begin();

	// Special case: if the dimension is nil:nil, this is a
	// dynamic array. Note that we only know how to handle dynamic
	// arrays with 1 dimension at a time.
      if (cur->first==0 && cur->second==0) {
	    assert(dims->size()==1);
	    ivl_type_s*res = new netdarray_t(btype);
	    return res;
      }

	// Special case: if the dimension is null:nil. this is a queue.
      if (dynamic_cast<PENull*>(cur->first)) {
	    cerr << get_fileline() << ": sorry: "
		 << "SV queues inside classes are not yet supported." << endl;
	    des->errors += 1;

	      // FIXME: Need to set the max size if cur->second is defined
	    ivl_type_s*res = new netqueue_t(btype, -1);
	    return res;
      }

      vector<netrange_t> dimensions;
      bool dimensions_ok = evaluate_ranges(des, scope, this, dimensions, *dims);

      if (!dimensions_ok) {
	    cerr << get_fileline() << " : warning: "
		 << "Bad dimensions for type here." << endl;
      }

      ivl_assert(*this, btype);
      ivl_type_s*res = new netuarray_t(dimensions, btype);
      return res;
}
