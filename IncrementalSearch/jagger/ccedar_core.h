// ccedar -- C++ implementation of Character-wise, Efficiently-updatable Double ARray trie (minimal version for Jagger)
//  $Id: ccedar_core.h 2152 2026-05-24 07:45:08Z ynaga $
// Copyright (c) 2022 Naoki Yoshinaga <ynaga@iis.u-tokyo.ac.jp>
#ifndef CCEDAR_CORE_H
#define CCEDAR_CORE_H
#include <cstdlib>
#include <cstdint>
#include <err.h>

#define ERR_IF(condition, format, ...) \
	if (condition) errx(1, __FILE__ " [%d]: " format, __LINE__, ##__VA_ARGS__)

namespace ccedar {
// typedefs
template <typename T>
struct to_unsigned;
template <>
struct to_unsigned<char> {
	typedef unsigned char type;
};
template <>
struct to_unsigned<int> {
	typedef unsigned int type;
};
enum error_code { NO_VALUE = -1,
	NO_PATH = -2 };
struct da_base { // double array trie with int value
	struct node {
		union {
			uint64_t r;
			struct {
				union {
					int base;
					int value;
				};
				int check;
			};
		};
		node(int base_ = 0, int check_ = 0)
			: base(base_)
			, check(check_)
		{
		}
	};
	da_base()
		: _array(0)
		, _size(0)
	{
	}
	size_t size() const { return _size; }
	node *array() const { return _array; }
	void set_array(void *p, size_t size_ = 0)
	{ // ad-hoc
		this->_array = static_cast<node *>(p);
		_size = static_cast<int>(size_);
	}

protected:
	node *_array;
	size_t _size;
};
// updatable double array trie
template <typename key_type, int MAX_KEY_BITS = sizeof(key_type) * 8>
struct da : da_base {
	typedef typename to_unsigned<key_type>::type ukey_type;
	enum { MAX_KEY_CODE = 1 << MAX_KEY_BITS,
		MAX_ALLOC_SIZE = MAX_KEY_CODE << 4,
		MAX_TRIAL = 1 };
	struct ninfo { // x1.5 update speed; x2 memory (8n -> 16n); can be 12n
		ukey_type sibling; // right sibling (= 0 if not exist)
		ukey_type child; // first child
	};
	struct block { // a block w/ sizeof (key_type) << 8 elements
		int prev; // prev block; 3 bytes
		int next; // next block; 3 bytes
		int num; // # empty elements; 0 - sizeof (key_type) << 8
		int trial; // # trial
		int ehead; // first empty item
		block()
			: prev(0)
			, next(0)
			, num(MAX_KEY_CODE)
			, trial(0)
			, ehead(0)
		{
		}
	};
	da()
		: _ninfo(0)
		, _block(0)
		, _bheadF(-1)
		, _bheadC(-1)
		, _bheadO(-1)
		, _capacity(0)
		, _child()
	{
		_add_block();
	}
	~da() { clear(); }
	// interfance (minimized)
	int traverse(const key_type *key, size_t &from, size_t &pos, size_t len) const
	{
		for (int to = 0; pos < len; from = to, ++pos) { // follow link
			to = _array[from].base ^ static_cast<ukey_type>(key[pos]);
			if (_array[to].check != static_cast<int>(from)) return NO_PATH;
		}
		const node n = _array[_array[from].base ^ 0];
		return n.check == static_cast<int>(from) ? n.base : NO_VALUE;
	}
	int &update(const key_type *key, size_t len, size_t from = 0, size_t pos = 0)
	{
		for (; pos < len; ++pos)
			from = _follow(from, static_cast<ukey_type>(key[pos]));
		const int to = _follow(from, 0);
		return _array[to].value;
	}
	void clear()
	{
		std::free(_array);
		std::free(_ninfo);
		std::free(_block);
		_array = NULL;
		_ninfo = NULL;
		_block = NULL;
		_bheadF = _bheadC = _bheadO = -1;
		_capacity = _size = 0;
	}

private:
	// currently disabled; implement these if you need
	da(const da &);
	da &operator=(const da &);
	ninfo *_ninfo;
	block *_block;
	int _bheadF; // first block of Full;   -1 if none
	int _bheadC; // first block of Closed; -1 if none
	int _bheadO; // first block of Open;   -1 if none
	size_t _capacity;
	ukey_type _child[MAX_KEY_CODE];
	//
	template <typename T>
	static void _realloc_array(T *&p, const int size_n, const int size_p = 0)
	{
		void *tmp = std::realloc(p, sizeof(T) * static_cast<size_t>(size_n));
		ERR_IF(!tmp, "memory reallocation failed\n");
		p = static_cast<T *>(tmp);
		static const T T0 = T();
		for (T *q(p + size_p), *const r(p + size_n); q != r; ++q)
			*q = T0;
	}
	int _follow(size_t &from, const ukey_type &label)
	{ // follow / create edge
		int to = 0;
		const int base = _array[from].base;
		if (base < 0 || _array[to = base ^ label].check < 0) {
			to = _pop_enode(base, label, from);
			_push_sibling(from, to ^ label, label, base >= 0);
		} else if (!to || _array[to].check != static_cast<int>(from))
			to = _resolve(from, base, label);
		return to;
	}
	void _pop_block(const int bi, int &head_in, const bool last)
	{
		if (last) // last one poped; Closed or Open
			head_in = -1;
		else {
			const block &b = _block[bi];
			_block[b.prev].next = b.next;
			_block[b.next].prev = b.prev;
			if (bi == head_in) head_in = b.next;
		}
	}
	void _push_block(const int bi, int &head_out, const bool empty)
	{
		block &b = _block[bi];
		if (empty) // the destination is empty
			head_out = b.prev = b.next = bi;
		else { // use most recently pushed
			int &tail_out = _block[head_out].prev;
			b.prev = tail_out;
			b.next = head_out;
			head_out = tail_out = _block[tail_out].next = bi;
		}
	}
	int _add_block()
	{
		if (_size == _capacity) { // allocate memory if needed
			_capacity += MAX_ALLOC_SIZE;
			_realloc_array(_array, _capacity, _capacity);
			_realloc_array(_ninfo, _capacity, _size);
			_realloc_array(_block, _capacity >> MAX_KEY_BITS, _size >> MAX_KEY_BITS);
		}
		_array[_size] = node(-(_size + MAX_KEY_CODE - 1), -(_size + 1));
		for (size_t i = _size + 1; i < _size + MAX_KEY_CODE - 1; ++i)
			_array[i] = node(-(i - 1), -(i + 1));
		_array[_size + MAX_KEY_CODE - 1] = node(-(_size + MAX_KEY_CODE - 2), -_size);
		_block[_size >> MAX_KEY_BITS].ehead = _size;
		_push_block(_size >> MAX_KEY_BITS, _bheadO, _bheadO == -1); // add to Open
		if (_size == 0) _pop_enode(-1, -1, 0); // reserve 0 for root
		_size += MAX_KEY_CODE;
		return (_size >> MAX_KEY_BITS) - 1;
	}
	// transfer block from one start w/ head_in to one start w/ head_out
	void _transfer_block(const int bi, int &head_in, int &head_out)
	{
		_pop_block(bi, head_in, bi == _block[bi].next);
		_push_block(bi, head_out, head_out == -1);
	}
	// pop empty node from block; never transfer the special block (bi = 0)
	int _pop_enode(const int base, const ukey_type label, const int from)
	{
		const int e = base < 0 ? _find_place() : base ^ label;
		const int bi = e >> MAX_KEY_BITS;
		node &n = _array[e];
		block &b = _block[bi];
		if (--b.num == 0) {
			_transfer_block(bi, _bheadC, _bheadF); // Closed to Full
		} else { // release empty node from empty ring
			_array[-n.base].check = n.check;
			_array[-n.check].base = n.base;
			if (e == b.ehead) b.ehead = -n.check; // set ehead
			if (b.num == 1 && b.trial != MAX_TRIAL) // Open to Closed
				_transfer_block(bi, _bheadO, _bheadC);
		}
		// initialize the released node
		n = node(label ? -1 : 0, from);
		if (base < 0) _array[from].base = e ^ label;
		return e;
	}
	// push empty node into empty ring
	void _push_enode(const int e)
	{
		const int bi = e >> MAX_KEY_BITS;
		block &b = _block[bi];
		if (++b.num == 1) { // Full to Closed
			b.ehead = e;
			_array[e] = node(-e, -e);
			_transfer_block(bi, _bheadF, _bheadC); // Full to Closed
		} else {
			const int prev = b.ehead;
			const int next = -_array[prev].check;
			_array[e] = node(-prev, -next);
			_array[prev].check = _array[next].base = -e;
			if (b.num == 2 || b.trial == MAX_TRIAL) // Closed to Open
				_transfer_block(bi, _bheadC, _bheadO);
			b.trial = 0;
		}
		_ninfo[e] = ninfo(); // reset ninfo; no child, no sibling
	}
	// push label to from's child
	void _push_sibling(const size_t from, const int base, const ukey_type label, const bool flag = true)
	{
		ukey_type *c = &_ninfo[from].child;
		if (flag && !*c) c = &_ninfo[base ^ *c].sibling;
		_ninfo[base ^ label].sibling = *c;
		*c = label;
	}
	// pop label from from's child
	void _pop_sibling(const size_t from, const int base, const ukey_type label)
	{
		ukey_type *c = &_ninfo[from].child;
		while (*c != label)
			c = &_ninfo[base ^ *c].sibling;
		*c = _ninfo[base ^ label].sibling;
	}
	// check whether to replace branching w/ the newly added node
	bool _consult(const int base_n, const int base_p, ukey_type c_n, ukey_type c_p) const
	{
		do
			if (!(c_p = _ninfo[base_p ^ c_p].sibling)) return false;
		while ((c_n = _ninfo[base_n ^ c_n].sibling));
		return true;
	}
	// enumerate (equal to or more than one) child nodes
	ukey_type *_set_child(ukey_type *p, const int base, ukey_type c, const int label)
	{
		if (!c) *p++ = c, c = _ninfo[base ^ c].sibling; // 0: terminal
		if (label != -1) *p++ = static_cast<ukey_type>(label);
		while (c)
			*p++ = c, c = _ninfo[base ^ c].sibling;
		return --p;
	}
	// explore new block to settle down
	int _find_place()
	{
		if (_bheadC != -1) return _block[_bheadC].ehead;
		if (_bheadO != -1) return _block[_bheadO].ehead;
		return _add_block() << MAX_KEY_BITS;
	}
	int _find_place(const ukey_type *const first, const ukey_type *const last)
	{
		if (_bheadO != -1)
			for (int bi(_bheadO), bz(_block[_bheadO].prev);;) { // examine blocks
				block &b = _block[bi];
				const int bj = b.next;
				if (b.num >= last - first + 1)
					for (int e = b.ehead;;) { // explore configuration
						const int base = e ^ *first;
						for (const ukey_type *p = first; _array[base ^ *++p].check < 0 && (base ^ *p);)
							if (p == last) return b.ehead = e, e; // no conflict
						if ((e = -_array[e].check) == b.ehead) break;
					}
				if (++b.trial == MAX_TRIAL) _transfer_block(bi, _bheadO, _bheadC);
				if (bi == bz) break;
				bi = bj;
			}
		return _add_block() << MAX_KEY_BITS;
	}
	// resolve conflict on base_n ^ label_n = base_p ^ label_p
	int _resolve(size_t &from_n, const int base_n, const ukey_type label_n)
	{
		// examine siblings of conflicted nodes
		const int to_pn = base_n ^ label_n;
		const int from_p = _array[to_pn].check; // could be root with dummy check
		const int base_p = _array[from_p].base; // never move root
		const bool move_n = !to_pn || _consult(base_n, base_p, _ninfo[from_n].child, _ninfo[from_p].child); // whether to replace siblings of newly added
		const int from = move_n ? static_cast<int>(from_n) : from_p;
		const int base_ = move_n ? base_n : base_p; // base to be replaced
		ukey_type *const first = &_child[0];
		ukey_type *const last = _set_child(first, base_, _ninfo[from].child, move_n ? label_n : -1);
		const int base = // new base; base_ -> base
			(first == last ? _find_place() : _find_place(first, last)) ^ *first;
		// replace & modify empty list
		for (const ukey_type *p = first; p <= last; ++p) { // move to_ => to
			const int to_ = base_ ^ *p; // old address
			const int to = _pop_enode(base, *p, from); // new address
			_ninfo[to].sibling = (p == last ? 0 : *(p + 1));
			if (move_n && to_ == to_pn) continue; // skip newcomer (no child)
			node &n_ = _array[to_];
			node &n = _array[to];
			if ((n.base = n_.base) > 0 && *p) { // copy base; bug fix
				ukey_type c = _ninfo[to].child = _ninfo[to_].child; // copy child
				do
					_array[n.base ^ c].check = to; // adjust grandson's check
				while ((c = _ninfo[n.base ^ c].sibling));
			}
			if (!move_n && to_ == static_cast<int>(from_n)) // parent node moved
				from_n = static_cast<size_t>(to); // bug fix
			if (!move_n && to_ == to_pn) { // the address is immediately used
				_push_sibling(from_n, to_pn ^ label_n, label_n);
				_ninfo[to_].child = 0; // remember to reset child
				n_ = node(label_n ? -1 : 0, static_cast<int>(from_n));
			} else
				_push_enode(to_);
		}
		_array[from].base = base; // new base
		if (move_n && *first == label_n) _ninfo[from].child = label_n; // new child
		return move_n ? base ^ label_n : to_pn;
	}
};
}
#endif
