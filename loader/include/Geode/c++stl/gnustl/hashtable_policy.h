// Internal policy header for unordered_set and unordered_map -*- C++ -*-

// Copyright (C) 2010-2014 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.

/** @file bits/hashtable_policy.h
 *  This is an internal header file, included by other library headers.
 *  Do not attempt to use it directly.
 *  @headername{unordered_map,unordered_set}
 */

#pragma once

#include "c++config.h"
#include "exception_defines.h"
#include "ext/aligned_buffer.h"
#include "stl_iterator_base_types.h"
#include "stl_iterator_base_funcs.h"
#include "initializer_list.h"
#include "ext/alloc_traits.h"
#include "type_traits.h"
#include "tuple.h"
#include "functexcept.h"
#include "utility.h"

namespace geode::stl {
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    class _Hashtable;

_GLIBCXX_END_NAMESPACE_VERSION

namespace __detail
{
_GLIBCXX_BEGIN_NAMESPACE_VERSION

  /**
   *  @defgroup hashtable-detail Base and Implementation Classes
   *  @ingroup unordered_associative_containers
   *  @{
   */
  template<typename _Key, typename _Value,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash, typename _Traits>
    struct _Hashtable_base;

  // Helper function: return distance(first, last) for forward
  // iterators, or 0 for input iterators.
  template<class _Iterator>
    inline typename iterator_traits<_Iterator>::difference_type
    __distance_fw(_Iterator __first, _Iterator __last,
		  input_iterator_tag)
    { return 0; }

  template<class _Iterator>
    inline typename iterator_traits<_Iterator>::difference_type
    __distance_fw(_Iterator __first, _Iterator __last,
		  forward_iterator_tag)
    { return distance(__first, __last); }

  template<class _Iterator>
    inline typename iterator_traits<_Iterator>::difference_type
    __distance_fw(_Iterator __first, _Iterator __last)
    {
      typedef typename iterator_traits<_Iterator>::iterator_category _Tag;
      return __distance_fw(__first, __last, _Tag());
    }

  // Helper type used to detect whether the hash functor is noexcept.
  template <typename _Key, typename _Hash>
    struct __is_noexcept_hash : integral_constant<bool,
	noexcept(std::declval<const _Hash&>()(std::declval<const _Key&>()))>
    { };

  struct _Identity
  {
    template<typename _Tp>
      _Tp&&
      operator()(_Tp&& __x) const
      { return std::forward<_Tp>(__x); }
  };

  struct _Select1st
  {
    template<typename _Tp>
      auto
      operator()(_Tp&& __x) const
      -> decltype(get<0>(std::forward<_Tp>(__x)))
      { return get<0>(std::forward<_Tp>(__x)); }
  };

  template<typename _NodeAlloc>
    struct _Hashtable_alloc;

  // Functor recycling a pool of nodes and using allocation once the pool is
  // empty.
  template<typename _NodeAlloc>
    struct _ReuseOrAllocNode
    {
    private:
      using __node_alloc_type = _NodeAlloc;
      using __hashtable_alloc = _Hashtable_alloc<__node_alloc_type>;
      using __value_alloc_type = typename __hashtable_alloc::__value_alloc_type;
      using __value_alloc_traits =
	typename __hashtable_alloc::__value_alloc_traits;
      using __node_alloc_traits =
	typename __hashtable_alloc::__node_alloc_traits;
      using __node_type = typename __hashtable_alloc::__node_type;

    public:
      _ReuseOrAllocNode(__node_type* __nodes, __hashtable_alloc& __h)
	: _M_nodes(__nodes), _M_h(__h) { }
      _ReuseOrAllocNode(const _ReuseOrAllocNode&) = delete;

      ~_ReuseOrAllocNode()
      { _M_h._M_deallocate_nodes(_M_nodes); }

      template<typename _Arg>
	__node_type*
	operator()(_Arg&& __arg) const
	{
	  if (_M_nodes)
	    {
	      __node_type* __node = _M_nodes;
	      _M_nodes = _M_nodes->_M_next();
	      __node->_M_nxt = nullptr;
	      __value_alloc_type __a(_M_h._M_node_allocator());
	      __value_alloc_traits::destroy(__a, __node->_M_valptr());
	      __try
		{
		  __value_alloc_traits::construct(__a, __node->_M_valptr(),
						  std::forward<_Arg>(__arg));
		}
	      __catch(...)
		{
		  __node->~__node_type();
		  __node_alloc_traits::deallocate(_M_h._M_node_allocator(),
						  __node, 1);
		  __throw_exception_again;
		}
	      return __node;
	    }
	  return _M_h._M_allocate_node(std::forward<_Arg>(__arg));
	}

    private:
      mutable __node_type* _M_nodes;
      __hashtable_alloc& _M_h;
    };

  // Functor similar to the previous one but without any pool of nodes to
  // recycle.
  template<typename _NodeAlloc>
    struct _AllocNode
    {
    private:
      using __hashtable_alloc = _Hashtable_alloc<_NodeAlloc>;
      using __node_type = typename __hashtable_alloc::__node_type;

    public:
      _AllocNode(__hashtable_alloc& __h)
	: _M_h(__h) { }

      template<typename _Arg>
	__node_type*
	operator()(_Arg&& __arg) const
	{ return _M_h._M_allocate_node(std::forward<_Arg>(__arg)); }

    private:
      __hashtable_alloc& _M_h;
    };

  // Auxiliary types used for all instantiations of _Hashtable nodes
  // and iterators.

  /**
   *  struct _Hashtable_traits
   *
   *  Important traits for hash tables.
   *
   *  @tparam _Cache_hash_code  Boolean value. True if the value of
   *  the hash function is stored along with the value. This is a
   *  time-space tradeoff.  Storing it may improve lookup speed by
   *  reducing the number of times we need to call the _Equal
   *  function.
   *
   *  @tparam _Constant_iterators  Boolean value. True if iterator and
   *  const_iterator are both constant iterator types. This is true
   *  for unordered_set and unordered_multiset, false for
   *  unordered_map and unordered_multimap.
   *
   *  @tparam _Unique_keys  Boolean value. True if the return value
   *  of _Hashtable::count(k) is always at most one, false if it may
   *  be an arbitrary number. This is true for unordered_set and
   *  unordered_map, false for unordered_multiset and
   *  unordered_multimap.
   */
  template<bool _Cache_hash_code, bool _Constant_iterators, bool _Unique_keys>
    struct _Hashtable_traits
    {
      template<bool _Cond>
	using __bool_constant = integral_constant<bool, _Cond>;

      using __hash_cached = __bool_constant<_Cache_hash_code>;
      using __constant_iterators = __bool_constant<_Constant_iterators>;
      using __unique_keys = __bool_constant<_Unique_keys>;
    };

  /**
   *  struct _Hash_node_base
   *
   *  Nodes, used to wrap elements stored in the hash table.  A policy
   *  template parameter of class template _Hashtable controls whether
   *  nodes also store a hash code. In some cases (e.g. strings) this
   *  may be a performance win.
   */
  struct _Hash_node_base
  {
    _Hash_node_base* _M_nxt;

    _Hash_node_base() noexcept : _M_nxt() { }

    _Hash_node_base(_Hash_node_base* __next) noexcept : _M_nxt(__next) { }
  };

  /**
   *  struct _Hash_node_value_base
   *
   *  Node type with the value to store.
   */
  template<typename _Value>
    struct _Hash_node_value_base : _Hash_node_base
    {
      typedef _Value value_type;

      __gnu_cxx::__aligned_buffer<_Value> _M_storage;

      _Value*
      _M_valptr() noexcept
      { return _M_storage._M_ptr(); }

      const _Value*
      _M_valptr() const noexcept
      { return _M_storage._M_ptr(); }

      _Value&
      _M_v() noexcept
      { return *_M_valptr(); }

      const _Value&
      _M_v() const noexcept
      { return *_M_valptr(); }
    };

  /**
   *  Primary template struct _Hash_node.
   */
  template<typename _Value, bool _Cache_hash_code>
    struct _Hash_node;

  /**
   *  Specialization for nodes with caches, struct _Hash_node.
   *
   *  Base class is __detail::_Hash_node_value_base.
   */
  template<typename _Value>
    struct _Hash_node<_Value, true> : _Hash_node_value_base<_Value>
    {
      std::size_t  _M_hash_code;

      _Hash_node*
      _M_next() const noexcept
      { return static_cast<_Hash_node*>(this->_M_nxt); }
    };

  /**
   *  Specialization for nodes without caches, struct _Hash_node.
   *
   *  Base class is __detail::_Hash_node_value_base.
   */
  template<typename _Value>
    struct _Hash_node<_Value, false> : _Hash_node_value_base<_Value>
    {
      _Hash_node*
      _M_next() const noexcept
      { return static_cast<_Hash_node*>(this->_M_nxt); }
    };

  /// Base class for node iterators.
  template<typename _Value, bool _Cache_hash_code>
    struct _Node_iterator_base
    {
      using __node_type = _Hash_node<_Value, _Cache_hash_code>;

      __node_type*  _M_cur;

      _Node_iterator_base(__node_type* __p) noexcept
      : _M_cur(__p) { }

      void
      _M_incr() noexcept
      { _M_cur = _M_cur->_M_next(); }
    };

  template<typename _Value, bool _Cache_hash_code>
    inline bool
    operator==(const _Node_iterator_base<_Value, _Cache_hash_code>& __x,
	       const _Node_iterator_base<_Value, _Cache_hash_code >& __y)
    noexcept
    { return __x._M_cur == __y._M_cur; }

  template<typename _Value, bool _Cache_hash_code>
    inline bool
    operator!=(const _Node_iterator_base<_Value, _Cache_hash_code>& __x,
	       const _Node_iterator_base<_Value, _Cache_hash_code>& __y)
    noexcept
    { return __x._M_cur != __y._M_cur; }

  /// Node iterators, used to iterate through all the hashtable.
  template<typename _Value, bool __constant_iterators, bool __cache>
    struct _Node_iterator
    : public _Node_iterator_base<_Value, __cache>
    {
    private:
      using __base_type = _Node_iterator_base<_Value, __cache>;
      using __node_type = typename __base_type::__node_type;

    public:
      typedef _Value					value_type;
      typedef std::ptrdiff_t				difference_type;
      typedef forward_iterator_tag			iterator_category;

      using pointer = typename conditional<__constant_iterators,
						const _Value*, _Value*>::type;

      using reference = typename conditional<__constant_iterators,
						  const _Value&, _Value&>::type;

      _Node_iterator() noexcept
      : __base_type(0) { }

      explicit
      _Node_iterator(__node_type* __p) noexcept
      : __base_type(__p) { }

      reference
      operator*() const noexcept
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const noexcept
      { return this->_M_cur->_M_valptr(); }

      _Node_iterator&
      operator++() noexcept
      {
	this->_M_incr();
	return *this;
      }

      _Node_iterator
      operator++(int) noexcept
      {
	_Node_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };

  /// Node const_iterators, used to iterate through all the hashtable.
  template<typename _Value, bool __constant_iterators, bool __cache>
    struct _Node_const_iterator
    : public _Node_iterator_base<_Value, __cache>
    {
    private:
      using __base_type = _Node_iterator_base<_Value, __cache>;
      using __node_type = typename __base_type::__node_type;

    public:
      typedef _Value					value_type;
      typedef std::ptrdiff_t				difference_type;
      typedef forward_iterator_tag			iterator_category;

      typedef const _Value*				pointer;
      typedef const _Value&				reference;

      _Node_const_iterator() noexcept
      : __base_type(0) { }

      explicit
      _Node_const_iterator(__node_type* __p) noexcept
      : __base_type(__p) { }

      _Node_const_iterator(const _Node_iterator<_Value, __constant_iterators,
			   __cache>& __x) noexcept
      : __base_type(__x._M_cur) { }

      reference
      operator*() const noexcept
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const noexcept
      { return this->_M_cur->_M_valptr(); }

      _Node_const_iterator&
      operator++() noexcept
      {
	this->_M_incr();
	return *this;
      }

      _Node_const_iterator
      operator++(int) noexcept
      {
	_Node_const_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };

  // Many of class template _Hashtable's template parameters are policy
  // classes.  These are defaults for the policies.

  /// Default range hashing function: use division to fold a large number
  /// into the range [0, N).
  struct _Mod_range_hashing
  {
    typedef std::size_t first_argument_type;
    typedef std::size_t second_argument_type;
    typedef std::size_t result_type;

    result_type
    operator()(first_argument_type __num,
	       second_argument_type __den) const noexcept
    { return __num % __den; }
  };

  /// Default ranged hash function H.  In principle it should be a
  /// function object composed from objects of type H1 and H2 such that
  /// h(k, N) = h2(h1(k), N), but that would mean making extra copies of
  /// h1 and h2.  So instead we'll just use a tag to tell class template
  /// hashtable to do that composition.
  struct _Default_ranged_hash { };

  /// Default value for rehash policy.  Bucket size is (usually) the
  /// smallest prime that keeps the load factor small enough.
  struct _Prime_rehash_policy
  {
    _Prime_rehash_policy(float __z = 1.0)
    : _M_max_load_factor(__z), _M_next_resize(0) { }

    float
    max_load_factor() const noexcept
    { return _M_max_load_factor; }

    // Return a bucket size no smaller than n.
    std::size_t
    _M_next_bkt(std::size_t __n) const;

    // Return a bucket count appropriate for n elements
    std::size_t
    _M_bkt_for_elements(std::size_t __n) const
    { return __builtin_ceil(__n / (long double)_M_max_load_factor); }

    // __n_bkt is current bucket count, __n_elt is current element count,
    // and __n_ins is number of elements to be inserted.  Do we need to
    // increase bucket count?  If so, return make_pair(true, n), where n
    // is the new bucket count.  If not, return make_pair(false, 0).
    pair<bool, std::size_t>
    _M_need_rehash(std::size_t __n_bkt, std::size_t __n_elt,
		   std::size_t __n_ins) const;

    typedef std::size_t _State;

    _State
    _M_state() const
    { return _M_next_resize; }

    void
    _M_reset() noexcept
    { _M_next_resize = 0; }

    void
    _M_reset(_State __state)
    { _M_next_resize = __state; }

    enum { _S_n_primes = sizeof(unsigned long) != 8 ? 256 : 256 + 48 };

    static const std::size_t _S_growth_factor = 2;

    float		_M_max_load_factor;
    mutable std::size_t	_M_next_resize;
  };

  // Base classes for std::_Hashtable.  We define these base classes
  // because in some cases we want to do different things depending on
  // the value of a policy class.  In some cases the policy class
  // affects which member functions and nested typedefs are defined;
  // we handle that by specializing base class templates.  Several of
  // the base class templates need to access other members of class
  // template _Hashtable, so we use a variant of the "Curiously
  // Recurring Template Pattern" (CRTP) technique.

  /**
   *  Primary class template _Map_base.
   *
   *  If the hashtable has a value type of the form pair<T1, T2> and a
   *  key extraction policy (_ExtractKey) that returns the first part
   *  of the pair, the hashtable gets a mapped_type typedef.  If it
   *  satisfies those criteria and also has unique keys, then it also
   *  gets an operator[].
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits,
	   bool _Unique_keys = _Traits::__unique_keys::value>
    struct _Map_base { };

  /// Partial specialization, __unique_keys set to false.
  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, false>
    {
      using mapped_type = typename tuple_element<1, _Pair>::type;
    };

  /// Partial specialization, __unique_keys set to true.
  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, true>
    {
    private:
      using __hashtable_base = __detail::_Hashtable_base<_Key, _Pair,
							 _Select1st,
							_Equal, _H1, _H2, _Hash,
							  _Traits>;

      using __hashtable = _Hashtable<_Key, _Pair, _Alloc,
				     _Select1st, _Equal,
				     _H1, _H2, _Hash, _RehashPolicy, _Traits>;

      using __hash_code = typename __hashtable_base::__hash_code;
      using __node_type = typename __hashtable_base::__node_type;

    public:
      using key_type = typename __hashtable_base::key_type;
      using iterator = typename __hashtable_base::iterator;
      using mapped_type = typename tuple_element<1, _Pair>::type;

      mapped_type&
      operator[](const key_type& __k);

      mapped_type&
      operator[](key_type&& __k);

      // _GLIBCXX_RESOLVE_LIB_DEFECTS
      // DR 761. unordered_map needs an at() member function.
      mapped_type&
      at(const key_type& __k);

      const mapped_type&
      at(const key_type& __k) const;
    };

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    typename _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
		       _H1, _H2, _Hash, _RehashPolicy, _Traits, true>
		       ::mapped_type&
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    operator[](const key_type& __k)
    {
      __hashtable* __h = static_cast<__hashtable*>(this);
      __hash_code __code = __h->_M_hash_code(__k);
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p)
	{
	  __p = __h->_M_allocate_node(piecewise_construct,
				      tuple<const key_type&>(__k),
				      tuple<>());
	  return __h->_M_insert_unique_node(__n, __code, __p)->second;
	}

      return __p->_M_v().second;
    }

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    typename _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
		       _H1, _H2, _Hash, _RehashPolicy, _Traits, true>
		       ::mapped_type&
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    operator[](key_type&& __k)
    {
      __hashtable* __h = static_cast<__hashtable*>(this);
      __hash_code __code = __h->_M_hash_code(__k);
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p)
	{
	  __p = __h->_M_allocate_node(piecewise_construct,
				      forward_as_tuple(move(__k)),
				      tuple<>());
	  return __h->_M_insert_unique_node(__n, __code, __p)->second;
	}

      return __p->_M_v().second;
    }

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    typename _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
		       _H1, _H2, _Hash, _RehashPolicy, _Traits, true>
		       ::mapped_type&
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    at(const key_type& __k)
    {
      __hashtable* __h = static_cast<__hashtable*>(this);
      __hash_code __code = __h->_M_hash_code(__k);
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p)
	__throw_out_of_range(__N("_Map_base::at"));
      return __p->_M_v().second;
    }

  template<typename _Key, typename _Pair, typename _Alloc, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    const typename _Map_base<_Key, _Pair, _Alloc, _Select1st,
			     _Equal, _H1, _H2, _Hash, _RehashPolicy,
			     _Traits, true>::mapped_type&
    _Map_base<_Key, _Pair, _Alloc, _Select1st, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    at(const key_type& __k) const
    {
      const __hashtable* __h = static_cast<const __hashtable*>(this);
      __hash_code __code = __h->_M_hash_code(__k);
      std::size_t __n = __h->_M_bucket_index(__k, __code);
      __node_type* __p = __h->_M_find_node(__n, __k, __code);

      if (!__p)
	__throw_out_of_range(__N("_Map_base::at"));
      return __p->_M_v().second;
    }

  /**
   *  Primary class template _Insert_base.
   *
   *  insert member functions appropriate to all _Hashtables.
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Insert_base
    {
    protected:
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey,
				     _Equal, _H1, _H2, _Hash,
				     _RehashPolicy, _Traits>;

      using __hashtable_base = _Hashtable_base<_Key, _Value, _ExtractKey,
					       _Equal, _H1, _H2, _Hash,
					       _Traits>;

      using value_type = typename __hashtable_base::value_type;
      using iterator = typename __hashtable_base::iterator;
      using const_iterator =  typename __hashtable_base::const_iterator;
      using size_type = typename __hashtable_base::size_type;

      using __unique_keys = typename __hashtable_base::__unique_keys;
      using __ireturn_type = typename __hashtable_base::__ireturn_type;
      using __node_type = _Hash_node<_Value, _Traits::__hash_cached::value>;
      using __node_alloc_type =
	typename __alloctr_rebind<_Alloc, __node_type>::__type;
      using __node_gen_type = _AllocNode<__node_alloc_type>;

      __hashtable&
      _M_conjure_hashtable()
      { return *(static_cast<__hashtable*>(this)); }

      template<typename _InputIterator, typename _NodeGetter>
	void
	_M_insert_range(_InputIterator __first, _InputIterator __last,
			const _NodeGetter&);

    public:
      __ireturn_type
      insert(const value_type& __v)
      {
	__hashtable& __h = _M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(__v, __node_gen, __unique_keys());
      }

      iterator
      insert(const_iterator __hint, const value_type& __v)
      {
	__hashtable& __h = _M_conjure_hashtable();
	__node_gen_type __node_gen(__h);	
	return __h._M_insert(__hint, __v, __node_gen, __unique_keys());
      }

      void
      insert(initializer_list<value_type> __l)
      { this->insert(__l.begin(), __l.end()); }

      template<typename _InputIterator>
	void
	insert(_InputIterator __first, _InputIterator __last)
	{
	  __hashtable& __h = _M_conjure_hashtable();
	  __node_gen_type __node_gen(__h);
	  return _M_insert_range(__first, __last, __node_gen);
	}
    };

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    template<typename _InputIterator, typename _NodeGetter>
      void
      _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		    _RehashPolicy, _Traits>::
      _M_insert_range(_InputIterator __first, _InputIterator __last,
		      const _NodeGetter& __node_gen)
      {
	using __rehash_type = typename __hashtable::__rehash_type;
	using __rehash_state = typename __hashtable::__rehash_state;
	using pair_type = pair<bool, std::size_t>;

	size_type __n_elt = __detail::__distance_fw(__first, __last);

	__hashtable& __h = _M_conjure_hashtable();
	__rehash_type& __rehash = __h._M_rehash_policy;
	const __rehash_state& __saved_state = __rehash._M_state();
	pair_type __do_rehash = __rehash._M_need_rehash(__h._M_bucket_count,
							__h._M_element_count,
							__n_elt);

	if (__do_rehash.first)
	  __h._M_rehash(__do_rehash.second, __saved_state);

	for (; __first != __last; ++__first)
	  __h._M_insert(*__first, __node_gen, __unique_keys());
      }

  /**
   *  Primary class template _Insert.
   *
   *  Select insert member functions appropriate to _Hashtable policy choices.
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits,
	   bool _Constant_iterators = _Traits::__constant_iterators::value,
	   bool _Unique_keys = _Traits::__unique_keys::value>
    struct _Insert;

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Insert<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		   _RehashPolicy, _Traits, true, true>
    : public _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
			   _H1, _H2, _Hash, _RehashPolicy, _Traits>
    {
      using __base_type = _Insert_base<_Key, _Value, _Alloc, _ExtractKey,
					_Equal, _H1, _H2, _Hash,
					_RehashPolicy, _Traits>;
      using value_type = typename __base_type::value_type;
      using iterator = typename __base_type::iterator;
      using const_iterator =  typename __base_type::const_iterator;

      using __unique_keys = typename __base_type::__unique_keys;
      using __hashtable = typename __base_type::__hashtable;
      using __node_gen_type = typename __base_type::__node_gen_type;

      using __base_type::insert;

      pair<iterator, bool>
      insert(value_type&& __v)
      {
	__hashtable& __h = this->_M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(move(__v), __node_gen, __unique_keys());
      }

      iterator
      insert(const_iterator __hint, value_type&& __v)
      {
	__hashtable& __h = this->_M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(__hint, move(__v), __node_gen,
			     __unique_keys());
      }
    };

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Insert<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		   _RehashPolicy, _Traits, true, false>
    : public _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
			   _H1, _H2, _Hash, _RehashPolicy, _Traits>
    {
      using __base_type = _Insert_base<_Key, _Value, _Alloc, _ExtractKey,
					_Equal, _H1, _H2, _Hash,
					_RehashPolicy, _Traits>;
      using value_type = typename __base_type::value_type;
      using iterator = typename __base_type::iterator;
      using const_iterator =  typename __base_type::const_iterator;

      using __unique_keys = typename __base_type::__unique_keys;
      using __hashtable = typename __base_type::__hashtable;
      using __node_gen_type = typename __base_type::__node_gen_type;

      using __base_type::insert;

      iterator
      insert(value_type&& __v)
      {
	__hashtable& __h = this->_M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(move(__v), __node_gen, __unique_keys());
      }

      iterator
      insert(const_iterator __hint, value_type&& __v)
      {
	__hashtable& __h = this->_M_conjure_hashtable();
	__node_gen_type __node_gen(__h);
	return __h._M_insert(__hint, move(__v), __node_gen,
			     __unique_keys());
      }
    };

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits, bool _Unique_keys>
    struct _Insert<_Key, _Value, _Alloc, _ExtractKey, _Equal, _H1, _H2, _Hash,
		   _RehashPolicy, _Traits, false, _Unique_keys>
    : public _Insert_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
			   _H1, _H2, _Hash, _RehashPolicy, _Traits>
    {
      using __base_type = _Insert_base<_Key, _Value, _Alloc, _ExtractKey,
				       _Equal, _H1, _H2, _Hash,
				       _RehashPolicy, _Traits>;
      using value_type = typename __base_type::value_type;
      using iterator = typename __base_type::iterator;
      using const_iterator =  typename __base_type::const_iterator;

      using __unique_keys = typename __base_type::__unique_keys;
      using __hashtable = typename __base_type::__hashtable;
      using __ireturn_type = typename __base_type::__ireturn_type;

      using __base_type::insert;

      template<typename _Pair>
	using __is_cons = std::is_constructible<value_type, _Pair&&>;

      template<typename _Pair>
	using _IFcons = std::enable_if<__is_cons<_Pair>::value>;

      template<typename _Pair>
	using _IFconsp = typename _IFcons<_Pair>::type;

      template<typename _Pair, typename = _IFconsp<_Pair>>
	__ireturn_type
	insert(_Pair&& __v)
	{
	  __hashtable& __h = this->_M_conjure_hashtable();
	  return __h._M_emplace(__unique_keys(), std::forward<_Pair>(__v));
	}

      template<typename _Pair, typename = _IFconsp<_Pair>>
	iterator
	insert(const_iterator __hint, _Pair&& __v)
	{
	  __hashtable& __h = this->_M_conjure_hashtable();
	  return __h._M_emplace(__hint, __unique_keys(),
				std::forward<_Pair>(__v));
	}
   };

  /**
   *  Primary class template  _Rehash_base.
   *
   *  Give hashtable the max_load_factor functions and reserve iff the
   *  rehash policy is _Prime_rehash_policy.
  */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Rehash_base;

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash, typename _Traits>
    struct _Rehash_base<_Key, _Value, _Alloc, _ExtractKey, _Equal,
			_H1, _H2, _Hash, _Prime_rehash_policy, _Traits>
    {
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey,
				     _Equal, _H1, _H2, _Hash,
				     _Prime_rehash_policy, _Traits>;

      float
      max_load_factor() const noexcept
      {
	const __hashtable* __this = static_cast<const __hashtable*>(this);
	return __this->__rehash_policy().max_load_factor();
      }

      void
      max_load_factor(float __z)
      {
	__hashtable* __this = static_cast<__hashtable*>(this);
	__this->__rehash_policy(_Prime_rehash_policy(__z));
      }

      void
      reserve(std::size_t __n)
      {
	__hashtable* __this = static_cast<__hashtable*>(this);
	__this->rehash(__builtin_ceil(__n / max_load_factor()));
      }
    };

  /**
   *  Primary class template _Hashtable_ebo_helper.
   *
   *  Helper class using EBO when it is not forbidden (the type is not
   *  final) and when it is worth it (the type is empty.)
   */
  template<int _Nm, typename _Tp,
	   bool __use_ebo = !__is_final(_Tp) && __is_empty(_Tp)>
    struct _Hashtable_ebo_helper;

  /// Specialization using EBO.
  template<int _Nm, typename _Tp>
    struct _Hashtable_ebo_helper<_Nm, _Tp, true>
    : private _Tp
    {
      _Hashtable_ebo_helper() = default;

      template<typename _OtherTp>
	_Hashtable_ebo_helper(_OtherTp&& __tp)
	  : _Tp(std::forward<_OtherTp>(__tp))
	{ }

      static const _Tp&
      _S_cget(const _Hashtable_ebo_helper& __eboh)
      { return static_cast<const _Tp&>(__eboh); }

      static _Tp&
      _S_get(_Hashtable_ebo_helper& __eboh)
      { return static_cast<_Tp&>(__eboh); }
    };

  /// Specialization not using EBO.
  template<int _Nm, typename _Tp>
    struct _Hashtable_ebo_helper<_Nm, _Tp, false>
    {
      _Hashtable_ebo_helper() = default;

      template<typename _OtherTp>
	_Hashtable_ebo_helper(_OtherTp&& __tp)
	  : _M_tp(std::forward<_OtherTp>(__tp))
	{ }

      static const _Tp&
      _S_cget(const _Hashtable_ebo_helper& __eboh)
      { return __eboh._M_tp; }

      static _Tp&
      _S_get(_Hashtable_ebo_helper& __eboh)
      { return __eboh._M_tp; }

    private:
      _Tp _M_tp;
    };

  /**
   *  Primary class template _Local_iterator_base.
   *
   *  Base class for local iterators, used to iterate within a bucket
   *  but not between buckets.
   */
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __cache_hash_code>
    struct _Local_iterator_base;

  /**
   *  Primary class template _Hash_code_base.
   *
   *  Encapsulates two policy issues that aren't quite orthogonal.
   *   (1) the difference between using a ranged hash function and using
   *       the combination of a hash function and a range-hashing function.
   *       In the former case we don't have such things as hash codes, so
   *       we have a dummy type as placeholder.
   *   (2) Whether or not we cache hash codes.  Caching hash codes is
   *       meaningless if we have a ranged hash function.
   *
   *  We also put the key extraction objects here, for convenience.
   *  Each specialization derives from one or more of the template
   *  parameters to benefit from Ebo. This is important as this type
   *  is inherited in some cases by the _Local_iterator_base type used
   *  to implement local_iterator and const_local_iterator. As with
   *  any iterator type we prefer to make it as small as possible.
   *
   *  Primary template is unused except as a hook for specializations.
   */
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __cache_hash_code>
    struct _Hash_code_base;

  /// Specialization: ranged hash function, no caching hash codes.  H1
  /// and H2 are provided but ignored.  We define a dummy hash code type.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2, _Hash, false>
    : private _Hashtable_ebo_helper<0, _ExtractKey>,
      private _Hashtable_ebo_helper<1, _Hash>
    {
    private:
      using __ebo_extract_key = _Hashtable_ebo_helper<0, _ExtractKey>;
      using __ebo_hash = _Hashtable_ebo_helper<1, _Hash>;

    protected:
      typedef void* 					__hash_code;
      typedef _Hash_node<_Value, false>			__node_type;

      // We need the default constructor for the local iterators.
      _Hash_code_base() = default;

      _Hash_code_base(const _ExtractKey& __ex, const _H1&, const _H2&,
		      const _Hash& __h)
      : __ebo_extract_key(__ex), __ebo_hash(__h) { }

      __hash_code
      _M_hash_code(const _Key& __key) const
      { return 0; }

      std::size_t
      _M_bucket_index(const _Key& __k, __hash_code, std::size_t __n) const
      { return _M_ranged_hash()(__k, __n); }

      std::size_t
      _M_bucket_index(const __node_type* __p, std::size_t __n) const
	noexcept( noexcept(std::declval<const _Hash&>()(std::declval<const _Key&>(),
						   (std::size_t)0)) )
      { return _M_ranged_hash()(_M_extract()(__p->_M_v()), __n); }

      void
      _M_store_code(__node_type*, __hash_code) const
      { }

      void
      _M_copy_code(__node_type*, const __node_type*) const
      { }

      void
      _M_swap(_Hash_code_base& __x)
      {
	swap(_M_extract(), __x._M_extract());
	swap(_M_ranged_hash(), __x._M_ranged_hash());
      }

      const _ExtractKey&
      _M_extract() const { return __ebo_extract_key::_S_cget(*this); }

      _ExtractKey&
      _M_extract() { return __ebo_extract_key::_S_get(*this); }

      const _Hash&
      _M_ranged_hash() const { return __ebo_hash::_S_cget(*this); }

      _Hash&
      _M_ranged_hash() { return __ebo_hash::_S_get(*this); }
    };

  // No specialization for ranged hash function while caching hash codes.
  // That combination is meaningless, and trying to do it is an error.

  /// Specialization: ranged hash function, cache hash codes.  This
  /// combination is meaningless, so we provide only a declaration
  /// and no definition.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2, _Hash, true>;

  /// Specialization: hash function and range-hashing function, no
  /// caching of hash codes.
  /// Provides typedef and accessor required by C++ 11.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2,
			   _Default_ranged_hash, false>
    : private _Hashtable_ebo_helper<0, _ExtractKey>,
      private _Hashtable_ebo_helper<1, _H1>,
      private _Hashtable_ebo_helper<2, _H2>
    {
    private:
      using __ebo_extract_key = _Hashtable_ebo_helper<0, _ExtractKey>;
      using __ebo_h1 = _Hashtable_ebo_helper<1, _H1>;
      using __ebo_h2 = _Hashtable_ebo_helper<2, _H2>;

      // Gives the local iterator implementation access to _M_bucket_index().
      friend struct _Local_iterator_base<_Key, _Value, _ExtractKey, _H1, _H2,
					 _Default_ranged_hash, false>;

    public:
      typedef _H1 					hasher;

      hasher
      hash_function() const
      { return _M_h1(); }

    protected:
      typedef std::size_t 				__hash_code;
      typedef _Hash_node<_Value, false>			__node_type;

      // We need the default constructor for the local iterators.
      _Hash_code_base() = default;

      _Hash_code_base(const _ExtractKey& __ex,
		      const _H1& __h1, const _H2& __h2,
		      const _Default_ranged_hash&)
      : __ebo_extract_key(__ex), __ebo_h1(__h1), __ebo_h2(__h2) { }

      __hash_code
      _M_hash_code(const _Key& __k) const
      { return _M_h1()(__k); }

      std::size_t
      _M_bucket_index(const _Key&, __hash_code __c, std::size_t __n) const
      { return _M_h2()(__c, __n); }

      std::size_t
      _M_bucket_index(const __node_type* __p, std::size_t __n) const
	noexcept( noexcept(std::declval<const _H1&>()(std::declval<const _Key&>()))
		  && noexcept(std::declval<const _H2&>()((__hash_code)0,
						    (std::size_t)0)) )
      { return _M_h2()(_M_h1()(_M_extract()(__p->_M_v())), __n); }

      void
      _M_store_code(__node_type*, __hash_code) const
      { }

      void
      _M_copy_code(__node_type*, const __node_type*) const
      { }

      void
      _M_swap(_Hash_code_base& __x)
      {
	swap(_M_extract(), __x._M_extract());
	swap(_M_h1(), __x._M_h1());
	swap(_M_h2(), __x._M_h2());
      }

      const _ExtractKey&
      _M_extract() const { return __ebo_extract_key::_S_cget(*this); }

      _ExtractKey&
      _M_extract() { return __ebo_extract_key::_S_get(*this); }

      const _H1&
      _M_h1() const { return __ebo_h1::_S_cget(*this); }

      _H1&
      _M_h1() { return __ebo_h1::_S_get(*this); }

      const _H2&
      _M_h2() const { return __ebo_h2::_S_cget(*this); }

      _H2&
      _M_h2() { return __ebo_h2::_S_get(*this); }
    };

  /// Specialization: hash function and range-hashing function,
  /// caching hash codes.  H is provided but ignored.  Provides
  /// typedef and accessor required by C++ 11.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2>
    struct _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2,
			   _Default_ranged_hash, true>
    : private _Hashtable_ebo_helper<0, _ExtractKey>,
      private _Hashtable_ebo_helper<1, _H1>,
      private _Hashtable_ebo_helper<2, _H2>
    {
    private:
      // Gives the local iterator implementation access to _M_h2().
      friend struct _Local_iterator_base<_Key, _Value, _ExtractKey, _H1, _H2,
					 _Default_ranged_hash, true>;

      using __ebo_extract_key = _Hashtable_ebo_helper<0, _ExtractKey>;
      using __ebo_h1 = _Hashtable_ebo_helper<1, _H1>;
      using __ebo_h2 = _Hashtable_ebo_helper<2, _H2>;

    public:
      typedef _H1 					hasher;

      hasher
      hash_function() const
      { return _M_h1(); }

    protected:
      typedef std::size_t 				__hash_code;
      typedef _Hash_node<_Value, true>			__node_type;

      _Hash_code_base(const _ExtractKey& __ex,
		      const _H1& __h1, const _H2& __h2,
		      const _Default_ranged_hash&)
      : __ebo_extract_key(__ex), __ebo_h1(__h1), __ebo_h2(__h2) { }

      __hash_code
      _M_hash_code(const _Key& __k) const
      { return _M_h1()(__k); }

      std::size_t
      _M_bucket_index(const _Key&, __hash_code __c,
		      std::size_t __n) const
      { return _M_h2()(__c, __n); }

      std::size_t
      _M_bucket_index(const __node_type* __p, std::size_t __n) const
	noexcept( noexcept(std::declval<const _H2&>()((__hash_code)0,
						 (std::size_t)0)) )
      { return _M_h2()(__p->_M_hash_code, __n); }

      void
      _M_store_code(__node_type* __n, __hash_code __c) const
      { __n->_M_hash_code = __c; }

      void
      _M_copy_code(__node_type* __to, const __node_type* __from) const
      { __to->_M_hash_code = __from->_M_hash_code; }

      void
      _M_swap(_Hash_code_base& __x)
      {
	swap(_M_extract(), __x._M_extract());
	swap(_M_h1(), __x._M_h1());
	swap(_M_h2(), __x._M_h2());
      }

      const _ExtractKey&
      _M_extract() const { return __ebo_extract_key::_S_cget(*this); }

      _ExtractKey&
      _M_extract() { return __ebo_extract_key::_S_get(*this); }

      const _H1&
      _M_h1() const { return __ebo_h1::_S_cget(*this); }

      _H1&
      _M_h1() { return __ebo_h1::_S_get(*this); }

      const _H2&
      _M_h2() const { return __ebo_h2::_S_cget(*this); }

      _H2&
      _M_h2() { return __ebo_h2::_S_get(*this); }
    };

  /**
   *  Primary class template _Equal_helper.
   *
   */
  template <typename _Key, typename _Value, typename _ExtractKey,
	    typename _Equal, typename _HashCodeType,
	    bool __cache_hash_code>
  struct _Equal_helper;

  /// Specialization.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _Equal, typename _HashCodeType>
  struct _Equal_helper<_Key, _Value, _ExtractKey, _Equal, _HashCodeType, true>
  {
    static bool
    _S_equals(const _Equal& __eq, const _ExtractKey& __extract,
	      const _Key& __k, _HashCodeType __c, _Hash_node<_Value, true>* __n)
    { return __c == __n->_M_hash_code && __eq(__k, __extract(__n->_M_v())); }
  };

  /// Specialization.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _Equal, typename _HashCodeType>
  struct _Equal_helper<_Key, _Value, _ExtractKey, _Equal, _HashCodeType, false>
  {
    static bool
    _S_equals(const _Equal& __eq, const _ExtractKey& __extract,
	      const _Key& __k, _HashCodeType, _Hash_node<_Value, false>* __n)
    { return __eq(__k, __extract(__n->_M_v())); }
  };


  /// Partial specialization used when nodes contain a cached hash code.
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Local_iterator_base<_Key, _Value, _ExtractKey,
				_H1, _H2, _Hash, true>
    : private _Hashtable_ebo_helper<0, _H2>
    {
    protected:
      using __base_type = _Hashtable_ebo_helper<0, _H2>;
      using __hash_code_base = _Hash_code_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, true>;

      _Local_iterator_base() = default;
      _Local_iterator_base(const __hash_code_base& __base,
			   _Hash_node<_Value, true>* __p,
			   std::size_t __bkt, std::size_t __bkt_count)
      : __base_type(__base._M_h2()),
	_M_cur(__p), _M_bucket(__bkt), _M_bucket_count(__bkt_count) { }

      void
      _M_incr()
      {
	_M_cur = _M_cur->_M_next();
	if (_M_cur)
	  {
	    std::size_t __bkt
	      = __base_type::_S_get(*this)(_M_cur->_M_hash_code,
					   _M_bucket_count);
	    if (__bkt != _M_bucket)
	      _M_cur = nullptr;
	  }
      }

      _Hash_node<_Value, true>*  _M_cur;
      std::size_t _M_bucket;
      std::size_t _M_bucket_count;

    public:
      const void*
      _M_curr() const { return _M_cur; }  // for equality ops

      std::size_t
      _M_get_bucket() const { return _M_bucket; }  // for debug mode
    };

  // Uninitialized storage for a _Hash_code_base.
  // This type is DefaultConstructible and Assignable even if the
  // _Hash_code_base type isn't, so that _Local_iterator_base<..., false>
  // can be DefaultConstructible and Assignable.
  template<typename _Tp, bool _IsEmpty = std::is_empty<_Tp>::value>
    struct _Hash_code_storage
    {
      __gnu_cxx::__aligned_buffer<_Tp> _M_storage;

      _Tp*
      _M_h() { return _M_storage._M_ptr(); }

      const _Tp*
      _M_h() const { return _M_storage._M_ptr(); }
    };

  // Empty partial specialization for empty _Hash_code_base types.
  template<typename _Tp>
    struct _Hash_code_storage<_Tp, true>
    {
      static_assert( std::is_empty<_Tp>::value, "Type must be empty" );

      // As _Tp is an empty type there will be no bytes written/read through
      // the cast pointer, so no strict-aliasing violation.
      _Tp*
      _M_h() { return reinterpret_cast<_Tp*>(this); }

      const _Tp*
      _M_h() const { return reinterpret_cast<const _Tp*>(this); }
    };

  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    using __hash_code_for_local_iter
      = _Hash_code_storage<_Hash_code_base<_Key, _Value, _ExtractKey,
					   _H1, _H2, _Hash, false>>;

  // Partial specialization used when hash codes are not cached
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash>
    struct _Local_iterator_base<_Key, _Value, _ExtractKey,
				_H1, _H2, _Hash, false>
    : __hash_code_for_local_iter<_Key, _Value, _ExtractKey, _H1, _H2, _Hash>
    {
    protected:
      using __hash_code_base = _Hash_code_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, false>;

      _Local_iterator_base() : _M_bucket_count(-1) { }

      _Local_iterator_base(const __hash_code_base& __base,
			   _Hash_node<_Value, false>* __p,
			   std::size_t __bkt, std::size_t __bkt_count)
      : _M_cur(__p), _M_bucket(__bkt), _M_bucket_count(__bkt_count)
      { _M_init(__base); }

      ~_Local_iterator_base()
      {
	if (_M_bucket_count != -1)
	  _M_destroy();
      }

      _Local_iterator_base(const _Local_iterator_base& __iter)
      : _M_cur(__iter._M_cur), _M_bucket(__iter._M_bucket),
        _M_bucket_count(__iter._M_bucket_count)
      {
	if (_M_bucket_count != -1)
	  _M_init(*__iter._M_h());
      }

      _Local_iterator_base&
      operator=(const _Local_iterator_base& __iter)
      {
	if (_M_bucket_count != -1)
	  _M_destroy();
	_M_cur = __iter._M_cur;
	_M_bucket = __iter._M_bucket;
	_M_bucket_count = __iter._M_bucket_count;
	if (_M_bucket_count != -1)
	  _M_init(*__iter._M_h());
	return *this;
      }

      void
      _M_incr()
      {
	_M_cur = _M_cur->_M_next();
	if (_M_cur)
	  {
	    std::size_t __bkt = this->_M_h()->_M_bucket_index(_M_cur,
							      _M_bucket_count);
	    if (__bkt != _M_bucket)
	      _M_cur = nullptr;
	  }
      }

      _Hash_node<_Value, false>*  _M_cur;
      std::size_t _M_bucket;
      std::size_t _M_bucket_count;

      void
      _M_init(const __hash_code_base& __base)
      { ::new(this->_M_h()) __hash_code_base(__base); }

      void
      _M_destroy() { this->_M_h()->~__hash_code_base(); }

    public:
      const void*
      _M_curr() const { return _M_cur; }  // for equality ops and debug mode

      std::size_t
      _M_get_bucket() const { return _M_bucket; }  // for debug mode
    };

  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash, bool __cache>
    inline bool
    operator==(const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __x,
	       const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __y)
    { return __x._M_curr() == __y._M_curr(); }

  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash, bool __cache>
    inline bool
    operator!=(const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __x,
	       const _Local_iterator_base<_Key, _Value, _ExtractKey,
					  _H1, _H2, _Hash, __cache>& __y)
    { return __x._M_curr() != __y._M_curr(); }

  /// local iterators
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __constant_iterators, bool __cache>
    struct _Local_iterator
    : public _Local_iterator_base<_Key, _Value, _ExtractKey,
				  _H1, _H2, _Hash, __cache>
    {
    private:
      using __base_type = _Local_iterator_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, __cache>;
      using __hash_code_base = typename __base_type::__hash_code_base;
    public:
      typedef _Value					value_type;
      typedef typename conditional<__constant_iterators,
					const _Value*, _Value*>::type
						       pointer;
      typedef typename conditional<__constant_iterators,
					const _Value&, _Value&>::type
						       reference;
      typedef std::ptrdiff_t				difference_type;
      typedef forward_iterator_tag			iterator_category;

      _Local_iterator() = default;

      _Local_iterator(const __hash_code_base& __base,
		      _Hash_node<_Value, __cache>* __p,
		      std::size_t __bkt, std::size_t __bkt_count)
	: __base_type(__base, __p, __bkt, __bkt_count)
      { }

      reference
      operator*() const
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const
      { return this->_M_cur->_M_valptr(); }

      _Local_iterator&
      operator++()
      {
	this->_M_incr();
	return *this;
      }

      _Local_iterator
      operator++(int)
      {
	_Local_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };

  /// local const_iterators
  template<typename _Key, typename _Value, typename _ExtractKey,
	   typename _H1, typename _H2, typename _Hash,
	   bool __constant_iterators, bool __cache>
    struct _Local_const_iterator
    : public _Local_iterator_base<_Key, _Value, _ExtractKey,
				  _H1, _H2, _Hash, __cache>
    {
    private:
      using __base_type = _Local_iterator_base<_Key, _Value, _ExtractKey,
					       _H1, _H2, _Hash, __cache>;
      using __hash_code_base = typename __base_type::__hash_code_base;

    public:
      typedef _Value					value_type;
      typedef const _Value*				pointer;
      typedef const _Value&				reference;
      typedef std::ptrdiff_t				difference_type;
      typedef forward_iterator_tag			iterator_category;

      _Local_const_iterator() = default;

      _Local_const_iterator(const __hash_code_base& __base,
			    _Hash_node<_Value, __cache>* __p,
			    std::size_t __bkt, std::size_t __bkt_count)
	: __base_type(__base, __p, __bkt, __bkt_count)
      { }

      _Local_const_iterator(const _Local_iterator<_Key, _Value, _ExtractKey,
						  _H1, _H2, _Hash,
						  __constant_iterators,
						  __cache>& __x)
	: __base_type(__x)
      { }

      reference
      operator*() const
      { return this->_M_cur->_M_v(); }

      pointer
      operator->() const
      { return this->_M_cur->_M_valptr(); }

      _Local_const_iterator&
      operator++()
      {
	this->_M_incr();
	return *this;
      }

      _Local_const_iterator
      operator++(int)
      {
	_Local_const_iterator __tmp(*this);
	this->_M_incr();
	return __tmp;
      }
    };

  /**
   *  Primary class template _Hashtable_base.
   *
   *  Helper class adding management of _Equal functor to
   *  _Hash_code_base type.
   *
   *  Base class templates are:
   *    - __detail::_Hash_code_base
   *    - __detail::_Hashtable_ebo_helper
   */
  template<typename _Key, typename _Value,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash, typename _Traits>
  struct _Hashtable_base
  : public _Hash_code_base<_Key, _Value, _ExtractKey, _H1, _H2, _Hash,
			   _Traits::__hash_cached::value>,
    private _Hashtable_ebo_helper<0, _Equal>
  {
  public:
    typedef _Key					key_type;
    typedef _Value					value_type;
    typedef _Equal					key_equal;
    typedef std::size_t					size_type;
    typedef std::ptrdiff_t				difference_type;

    using __traits_type = _Traits;
    using __hash_cached = typename __traits_type::__hash_cached;
    using __constant_iterators = typename __traits_type::__constant_iterators;
    using __unique_keys = typename __traits_type::__unique_keys;

    using __hash_code_base = _Hash_code_base<_Key, _Value, _ExtractKey,
					     _H1, _H2, _Hash,
					     __hash_cached::value>;

    using __hash_code = typename __hash_code_base::__hash_code;
    using __node_type = typename __hash_code_base::__node_type;

    using iterator = __detail::_Node_iterator<value_type,
					      __constant_iterators::value,
					      __hash_cached::value>;

    using const_iterator = __detail::_Node_const_iterator<value_type,
						   __constant_iterators::value,
						   __hash_cached::value>;

    using local_iterator = __detail::_Local_iterator<key_type, value_type,
						  _ExtractKey, _H1, _H2, _Hash,
						  __constant_iterators::value,
						     __hash_cached::value>;

    using const_local_iterator = __detail::_Local_const_iterator<key_type,
								 value_type,
					_ExtractKey, _H1, _H2, _Hash,
					__constant_iterators::value,
					__hash_cached::value>;

    using __ireturn_type = typename conditional<__unique_keys::value,
						     pair<iterator, bool>,
						     iterator>::type;
  private:
    using _EqualEBO = _Hashtable_ebo_helper<0, _Equal>;
    using _EqualHelper =  _Equal_helper<_Key, _Value, _ExtractKey, _Equal,
					__hash_code, __hash_cached::value>;

  protected:
    _Hashtable_base(const _ExtractKey& __ex, const _H1& __h1, const _H2& __h2,
		    const _Hash& __hash, const _Equal& __eq)
    : __hash_code_base(__ex, __h1, __h2, __hash), _EqualEBO(__eq)
    { }

    bool
    _M_equals(const _Key& __k, __hash_code __c, __node_type* __n) const
    {
      return _EqualHelper::_S_equals(_M_eq(), this->_M_extract(),
				     __k, __c, __n);
    }

    void
    _M_swap(_Hashtable_base& __x)
    {
      __hash_code_base::_M_swap(__x);
      swap(_M_eq(), __x._M_eq());
    }

    const _Equal&
    _M_eq() const { return _EqualEBO::_S_cget(*this); }

    _Equal&
    _M_eq() { return _EqualEBO::_S_get(*this); }
  };

  /**
   *  struct _Equality_base.
   *
   *  Common types and functions for class _Equality.
   */
  struct _Equality_base
  {
  protected:
    template<typename _Uiterator>
      static bool
      _S_is_permutation(_Uiterator, _Uiterator, _Uiterator);
  };

  // See std::is_permutation in N3068.
  template<typename _Uiterator>
    bool
    _Equality_base::
    _S_is_permutation(_Uiterator __first1, _Uiterator __last1,
		      _Uiterator __first2)
    {
      for (; __first1 != __last1; ++__first1, ++__first2)
	if (!(*__first1 == *__first2))
	  break;

      if (__first1 == __last1)
	return true;

      _Uiterator __last2 = __first2;
      std::advance(__last2, std::distance(__first1, __last1));

      for (_Uiterator __it1 = __first1; __it1 != __last1; ++__it1)
	{
	  _Uiterator __tmp =  __first1;
	  while (__tmp != __it1 && !bool(*__tmp == *__it1))
	    ++__tmp;

	  // We've seen this one before.
	  if (__tmp != __it1)
	    continue;

	  std::ptrdiff_t __n2 = 0;
	  for (__tmp = __first2; __tmp != __last2; ++__tmp)
	    if (*__tmp == *__it1)
	      ++__n2;

	  if (!__n2)
	    return false;

	  std::ptrdiff_t __n1 = 0;
	  for (__tmp = __it1; __tmp != __last1; ++__tmp)
	    if (*__tmp == *__it1)
	      ++__n1;

	  if (__n1 != __n2)
	    return false;
	}
      return true;
    }

  /**
   *  Primary class template  _Equality.
   *
   *  This is for implementing equality comparison for unordered
   *  containers, per N3068, by John Lakos and Pablo Halpern.
   *  Algorithmically, we follow closely the reference implementations
   *  therein.
   */
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits,
	   bool _Unique_keys = _Traits::__unique_keys::value>
    struct _Equality;

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, true>
    {
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey, _Equal,
				     _H1, _H2, _Hash, _RehashPolicy, _Traits>;

      bool
      _M_equal(const __hashtable&) const;
    };

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    bool
    _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, true>::
    _M_equal(const __hashtable& __other) const
    {
      const __hashtable* __this = static_cast<const __hashtable*>(this);

      if (__this->size() != __other.size())
	return false;

      for (auto __itx = __this->begin(); __itx != __this->end(); ++__itx)
	{
	  const auto __ity = __other.find(_ExtractKey()(*__itx));
	  if (__ity == __other.end() || !bool(*__ity == *__itx))
	    return false;
	}
      return true;
    }

  /// Specialization.
  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    struct _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
		     _H1, _H2, _Hash, _RehashPolicy, _Traits, false>
    : public _Equality_base
    {
      using __hashtable = _Hashtable<_Key, _Value, _Alloc, _ExtractKey, _Equal,
				     _H1, _H2, _Hash, _RehashPolicy, _Traits>;

      bool
      _M_equal(const __hashtable&) const;
    };

  template<typename _Key, typename _Value, typename _Alloc,
	   typename _ExtractKey, typename _Equal,
	   typename _H1, typename _H2, typename _Hash,
	   typename _RehashPolicy, typename _Traits>
    bool
    _Equality<_Key, _Value, _Alloc, _ExtractKey, _Equal,
	      _H1, _H2, _Hash, _RehashPolicy, _Traits, false>::
    _M_equal(const __hashtable& __other) const
    {
      const __hashtable* __this = static_cast<const __hashtable*>(this);

      if (__this->size() != __other.size())
	return false;

      for (auto __itx = __this->begin(); __itx != __this->end();)
	{
	  const auto __xrange = __this->equal_range(_ExtractKey()(*__itx));
	  const auto __yrange = __other.equal_range(_ExtractKey()(*__itx));

	  if (std::distance(__xrange.first, __xrange.second)
	      != std::distance(__yrange.first, __yrange.second))
	    return false;

	  if (!_S_is_permutation(__xrange.first, __xrange.second,
				 __yrange.first))
	    return false;

	  __itx = __xrange.second;
	}
      return true;
    }

  /**
   * This type deals with all allocation and keeps an allocator instance through
   * inheritance to benefit from EBO when possible.
   */
  template<typename _NodeAlloc>
    struct _Hashtable_alloc : private _Hashtable_ebo_helper<0, _NodeAlloc>
    {
    private:
      using __ebo_node_alloc = _Hashtable_ebo_helper<0, _NodeAlloc>;
    public:
      using __node_type = typename _NodeAlloc::value_type;
      using __node_alloc_type = _NodeAlloc;
      // Use __gnu_cxx to benefit from _S_always_equal and al.
      using __node_alloc_traits = __alloc_traits<__node_alloc_type>;

      using __value_type = typename __node_type::value_type;
      using __value_alloc_type =
	typename __alloctr_rebind<__node_alloc_type, __value_type>::__type;
      using __value_alloc_traits = allocator_traits<__value_alloc_type>;

      using __node_base = __detail::_Hash_node_base;
      using __bucket_type = __node_base*;      
      using __bucket_alloc_type =
	typename __alloctr_rebind<__node_alloc_type, __bucket_type>::__type;
      using __bucket_alloc_traits = allocator_traits<__bucket_alloc_type>;

      _Hashtable_alloc(const _Hashtable_alloc&) = default;
      _Hashtable_alloc(_Hashtable_alloc&&) = default;

      template<typename _Alloc>
	_Hashtable_alloc(_Alloc&& __a)
	  : __ebo_node_alloc(std::forward<_Alloc>(__a))
	{ }

      __node_alloc_type&
      _M_node_allocator()
      { return __ebo_node_alloc::_S_get(*this); }

      const __node_alloc_type&
      _M_node_allocator() const
      { return __ebo_node_alloc::_S_cget(*this); }

      template<typename... _Args>
	__node_type*
	_M_allocate_node(_Args&&... __args);

      void
      _M_deallocate_node(__node_type* __n);

      // Deallocate the linked list of nodes pointed to by __n
      void
      _M_deallocate_nodes(__node_type* __n);

      __bucket_type*
      _M_allocate_buckets(std::size_t __n);

      void
      _M_deallocate_buckets(__bucket_type*, std::size_t __n);
    };

  // Definitions of class template _Hashtable_alloc's out-of-line member
  // functions.
  template<typename _NodeAlloc>
    template<typename... _Args>
      typename _Hashtable_alloc<_NodeAlloc>::__node_type*
      _Hashtable_alloc<_NodeAlloc>::_M_allocate_node(_Args&&... __args)
      {
	auto __nptr = __node_alloc_traits::allocate(_M_node_allocator(), 1);
	__node_type* __n = std::addressof(*__nptr);
	__try
	  {
	    __value_alloc_type __a(_M_node_allocator());
	    ::new ((void*)__n) __node_type;
	    __value_alloc_traits::construct(__a, __n->_M_valptr(),
					    std::forward<_Args>(__args)...);
	    return __n;
	  }
	__catch(...)
	  {
	    __node_alloc_traits::deallocate(_M_node_allocator(), __nptr, 1);
	    __throw_exception_again;
	  }
      }

  template<typename _NodeAlloc>
    void
    _Hashtable_alloc<_NodeAlloc>::_M_deallocate_node(__node_type* __n)
    {
      typedef typename __node_alloc_traits::pointer _Ptr;
      auto __ptr = std::pointer_traits<_Ptr>::pointer_to(*__n);
      __value_alloc_type __a(_M_node_allocator());
      __value_alloc_traits::destroy(__a, __n->_M_valptr());
      __n->~__node_type();
      __node_alloc_traits::deallocate(_M_node_allocator(), __ptr, 1);
    }

  template<typename _NodeAlloc>
    void
    _Hashtable_alloc<_NodeAlloc>::_M_deallocate_nodes(__node_type* __n)
    {
      while (__n)
	{
	  __node_type* __tmp = __n;
	  __n = __n->_M_next();
	  _M_deallocate_node(__tmp);
	}
    }

  template<typename _NodeAlloc>
    typename _Hashtable_alloc<_NodeAlloc>::__bucket_type*
    _Hashtable_alloc<_NodeAlloc>::_M_allocate_buckets(std::size_t __n)
    {
      __bucket_alloc_type __alloc(_M_node_allocator());

      auto __ptr = __bucket_alloc_traits::allocate(__alloc, __n);
      __bucket_type* __p = std::addressof(*__ptr);
      __builtin_memset(__p, 0, __n * sizeof(__bucket_type));
      return __p;
    }

  template<typename _NodeAlloc>
    void
    _Hashtable_alloc<_NodeAlloc>::_M_deallocate_buckets(__bucket_type* __bkts,
							std::size_t __n)
    {
      typedef typename __bucket_alloc_traits::pointer _Ptr;
      auto __ptr = std::pointer_traits<_Ptr>::pointer_to(*__bkts);
      __bucket_alloc_type __alloc(_M_node_allocator());
      __bucket_alloc_traits::deallocate(__alloc, __ptr, __n);
    }

 //@} hashtable-detail
_GLIBCXX_END_NAMESPACE_VERSION
} // namespace __detail
} // namespace std

// dank: addition

namespace geode::stl::__detail {

  // The sentinel value is kept only for abi backward compatibility.
  inline const unsigned long __prime_list[] = // 256 + 1 or 256 + 48 + 1
  {
    2ul, 3ul, 5ul, 7ul, 11ul, 13ul, 17ul, 19ul, 23ul, 29ul, 31ul,
    37ul, 41ul, 43ul, 47ul, 53ul, 59ul, 61ul, 67ul, 71ul, 73ul, 79ul,
    83ul, 89ul, 97ul, 103ul, 109ul, 113ul, 127ul, 137ul, 139ul, 149ul,
    157ul, 167ul, 179ul, 193ul, 199ul, 211ul, 227ul, 241ul, 257ul,
    277ul, 293ul, 313ul, 337ul, 359ul, 383ul, 409ul, 439ul, 467ul,
    503ul, 541ul, 577ul, 619ul, 661ul, 709ul, 761ul, 823ul, 887ul,
    953ul, 1031ul, 1109ul, 1193ul, 1289ul, 1381ul, 1493ul, 1613ul,
    1741ul, 1879ul, 2029ul, 2179ul, 2357ul, 2549ul, 2753ul, 2971ul,
    3209ul, 3469ul, 3739ul, 4027ul, 4349ul, 4703ul, 5087ul, 5503ul,
    5953ul, 6427ul, 6949ul, 7517ul, 8123ul, 8783ul, 9497ul, 10273ul,
    11113ul, 12011ul, 12983ul, 14033ul, 15173ul, 16411ul, 17749ul,
    19183ul, 20753ul, 22447ul, 24281ul, 26267ul, 28411ul, 30727ul,
    33223ul, 35933ul, 38873ul, 42043ul, 45481ul, 49201ul, 53201ul,
    57557ul, 62233ul, 67307ul, 72817ul, 78779ul, 85229ul, 92203ul,
    99733ul, 107897ul, 116731ul, 126271ul, 136607ul, 147793ul,
    159871ul, 172933ul, 187091ul, 202409ul, 218971ul, 236897ul,
    256279ul, 277261ul, 299951ul, 324503ul, 351061ul, 379787ul,
    410857ul, 444487ul, 480881ul, 520241ul, 562841ul, 608903ul,
    658753ul, 712697ul, 771049ul, 834181ul, 902483ul, 976369ul,
    1056323ul, 1142821ul, 1236397ul, 1337629ul, 1447153ul, 1565659ul,
    1693859ul, 1832561ul, 1982627ul, 2144977ul, 2320627ul, 2510653ul,
    2716249ul, 2938679ul, 3179303ul, 3439651ul, 3721303ul, 4026031ul,
    4355707ul, 4712381ul, 5098259ul, 5515729ul, 5967347ul, 6456007ul,
    6984629ul, 7556579ul, 8175383ul, 8844859ul, 9569143ul, 10352717ul,
    11200489ul, 12117689ul, 13109983ul, 14183539ul, 15345007ul,
    16601593ul, 17961079ul, 19431899ul, 21023161ul, 22744717ul,
    24607243ul, 26622317ul, 28802401ul, 31160981ul, 33712729ul,
    36473443ul, 39460231ul, 42691603ul, 46187573ul, 49969847ul,
    54061849ul, 58488943ul, 63278561ul, 68460391ul, 74066549ul,
    80131819ul, 86693767ul, 93793069ul, 101473717ul, 109783337ul,
    118773397ul, 128499677ul, 139022417ul, 150406843ul, 162723577ul,
    176048909ul, 190465427ul, 206062531ul, 222936881ul, 241193053ul,
    260944219ul, 282312799ul, 305431229ul, 330442829ul, 357502601ul,
    386778277ul, 418451333ul, 452718089ul, 489790921ul, 529899637ul,
    573292817ul, 620239453ul, 671030513ul, 725980837ul, 785430967ul,
    849749479ul, 919334987ul, 994618837ul, 1076067617ul, 1164186217ul,
    1259520799ul, 1362662261ul, 1474249943ul, 1594975441ul, 1725587117ul,
    1866894511ul, 2019773507ul, 2185171673ul, 2364114217ul, 2557710269ul,
    2767159799ul, 2993761039ul, 3238918481ul, 3504151727ul, 3791104843ul,
    4101556399ul, 4294967291ul,
    // Sentinel, so we don't have to test the result of lower_bound,
    // or, on 64-bit machines, rest of the table.
#if __SIZEOF_LONG__ != 8
    4294967291ul
#else
    6442450933ul, 8589934583ul, 12884901857ul, 17179869143ul,
    25769803693ul, 34359738337ul, 51539607367ul, 68719476731ul,
    103079215087ul, 137438953447ul, 206158430123ul, 274877906899ul,
    412316860387ul, 549755813881ul, 824633720731ul, 1099511627689ul,
    1649267441579ul, 2199023255531ul, 3298534883309ul, 4398046511093ul,
    6597069766607ul, 8796093022151ul, 13194139533241ul, 17592186044399ul,
    26388279066581ul, 35184372088777ul, 52776558133177ul, 70368744177643ul,
    105553116266399ul, 140737488355213ul, 211106232532861ul, 281474976710597ul,
    562949953421231ul, 1125899906842597ul, 2251799813685119ul,
    4503599627370449ul, 9007199254740881ul, 18014398509481951ul,
    36028797018963913ul, 72057594037927931ul, 144115188075855859ul,
    288230376151711717ul, 576460752303423433ul,
    1152921504606846883ul, 2305843009213693951ul,
    4611686018427387847ul, 9223372036854775783ul,
    18446744073709551557ul, 18446744073709551557ul
#endif
  };

  // Return a prime no smaller than n.
  inline std::size_t
  _Prime_rehash_policy::_M_next_bkt(std::size_t __n) const
  {
    // Optimize lookups involving the first elements of __prime_list.
    // (useful to speed-up, eg, constructors)
    static const unsigned char __fast_bkt[]
      = { 2, 2, 2, 3, 5, 5, 7, 7, 11, 11, 11, 11, 13, 13 };

    if (__n < sizeof(__fast_bkt))
      {
	if (__n == 0)
	  // Special case on container 1st initialization with 0 bucket count
	  // hint. We keep _M_next_resize to 0 to make sure that next time we
	  // want to add an element allocation will take place.
	  return 1;

	_M_next_resize =
	  __builtin_floor(__fast_bkt[__n] * (double)_M_max_load_factor);
	return __fast_bkt[__n];
      }

    // Number of primes (without sentinel).
    constexpr auto __n_primes
      = sizeof(__prime_list) / sizeof(unsigned long) - 1;

    // Don't include the last prime in the search, so that anything
    // higher than the second-to-last prime returns a past-the-end
    // iterator that can be dereferenced to get the last prime.
    constexpr auto __last_prime = __prime_list + __n_primes - 1;

    const unsigned long* __next_bkt =
      lower_bound(__prime_list + 6, __last_prime, __n);

    if (__next_bkt == __last_prime)
      // Set next resize to the max value so that we never try to rehash again
      // as we already reach the biggest possible bucket number.
      // Note that it might result in max_load_factor not being respected.
      _M_next_resize = size_t(-1);
    else
      _M_next_resize =
	__builtin_floor(*__next_bkt * (double)_M_max_load_factor);

    return *__next_bkt;
  }

  inline pair<bool, std::size_t>
  _Prime_rehash_policy::
  _M_need_rehash(std::size_t __n_bkt, std::size_t __n_elt,
		 std::size_t __n_ins) const
  {
    if (__n_elt + __n_ins > _M_next_resize)
      {
	// If _M_next_resize is 0 it means that we have nothing allocated so
	// far and that we start inserting elements. In this case we start
	// with an initial bucket size of 11.
	double __min_bkts
	  = max<std::size_t>(__n_elt + __n_ins, _M_next_resize ? 0 : 11)
	  / (double)_M_max_load_factor;
	if (__min_bkts >= __n_bkt)
	  return { true,
	    _M_next_bkt(max<std::size_t>(__builtin_floor(__min_bkts) + 1,
					      __n_bkt * _S_growth_factor)) };

	_M_next_resize
	  = __builtin_floor(__n_bkt * (double)_M_max_load_factor);
	return { false, 0 };
      }
    else
      return { false, 0 };
  }
}