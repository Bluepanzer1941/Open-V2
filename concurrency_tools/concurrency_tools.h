#pragma once
#include <stdint.h>
#include <atomic>
#include <string>
#include <type_traits>
#include "common\\common.h"

#undef min
#undef max

//source: Bob Jenkins
class jsf_prng {
private:
	uint32_t a;
	uint32_t b;
	uint32_t c;
	uint32_t d;
public:
	using result_type = uint32_t;

	jsf_prng(uint32_t seed);
	jsf_prng(jsf_prng const& o) noexcept : a(o.a), b(o.b), c(o.c), d(o.d) {}
	jsf_prng(jsf_prng&& o) noexcept : a(o.a), b(o.b), c(o.c), d(o.d) {}
	constexpr static uint32_t max() { return std::numeric_limits<uint32_t>::max(); }
	constexpr static uint32_t min() { return std::numeric_limits<uint32_t>::min(); }
	uint32_t operator()();
};

jsf_prng& get_local_generator();

constexpr uint32_t ct_log2(uint32_t n) {
	return ((n < 2) ? 0 : 1 + ct_log2(n / 2));
}

inline uint32_t rt_log2(uint32_t n) {
	return 31ui32 - uint32_t(__builtin_clz(n | 1ui32));
}

constexpr uint32_t ct_log2_round_up(uint32_t n) {
	return ((1ui32 << ct_log2(n)) >= n) ? ct_log2(n) : ct_log2(n) + 1ui32;
}

inline uint32_t rt_log2_round_up(uint32_t n) {
	return n > 1ui32 ? 32ui32 - uint32_t(__builtin_clz(n - 1ui32)) : 0ui32;
}

template<typename E1, typename E2>
class string_sum_expression;

struct string_expression_common_base {};
template<typename T>
struct string_expression;

template<typename T>
class string_expression_support {
public:
	template<typename O>
	auto operator+(const O& o) const {
		if constexpr(std::is_base_of_v<string_expression_common_base, T> && std::is_base_of_v<string_expression_common_base, O>)
			return string_sum_expression<T, O>(*static_cast<const T*>(this), o);
		else if constexpr(!std::is_base_of_v<string_expression_common_base, T> && std::is_base_of_v<string_expression_common_base, O>)
			return string_sum_expression<string_expression<T>, O>(string_expression<T>(*static_cast<const T*>(this)), o);
		else if constexpr(std::is_base_of_v<string_expression_common_base, T> && !std::is_base_of_v<string_expression_common_base, O>)
			return string_sum_expression<T, string_expression<const O>>(*static_cast<const T*>(this), string_expression<const O>(o));
		else if constexpr(!std::is_base_of_v<string_expression_common_base, T> && !std::is_base_of_v<string_expression_common_base, O>)
			return string_sum_expression<string_expression<T>, string_expression<const O>>(string_expression<T>(*static_cast<const T*>(this)), string_expression<const O>(o));
	}
	operator std::string() const;
};

template<typename T>
struct string_expression : public string_expression_support<string_expression<T>>, public string_expression_common_base {
private:
	const T& base;
public:
	string_expression(const T& b) : base(b) {}
	uint32_t length() const { return static_cast<uint32_t>(base.length()); }
	char operator[](uint32_t i) const { return base[i]; }
};

template<>
struct string_expression<const char*> : public string_expression_support<string_expression<const char*>>, public string_expression_common_base {
private:
	const char* base;
	const uint32_t len = 0;
public:
	string_expression(const char* b) : base(b), len(static_cast<uint32_t>(strlen(b))) {}
	uint32_t length() const { return len; }
	char operator[](uint32_t i) const { return base[i]; }
};

template<>
struct string_expression<char*> : public string_expression<const char*>  {};

template<size_t N>
struct string_expression<const char[N]> : public string_expression_support<string_expression<const char[N]>>, public string_expression_common_base {
private:
	const char* base;
public:
	string_expression(const char(&b)[N]) : base(b) {}
	uint32_t length() const { return N - 1; }
	char operator[](uint32_t i) const { return base[i]; }
};

template<size_t N>
struct string_expression<char[N]>  : public string_expression<const char[N]> {};

struct empty_string_expression : public string_expression_support<empty_string_expression>, public string_expression_common_base {
public:
	empty_string_expression() {}
	empty_string_expression(const string_expression<empty_string_expression>&) {}
	uint32_t length() const { return 0; }
	char operator[](uint32_t) const { return 0; }
};

template<typename E1, typename E2>
class string_sum_expression : public string_expression_support<string_sum_expression<E1, E2>>, public string_expression_common_base {
public:
	const E1 a;
	const E2 b;
	string_sum_expression(const E1& aa, const E2& bb) : a(aa), b(bb) {}
	uint32_t length() const {
		return a.length() + b.length();
	}
	char operator[](uint32_t i) const {
		const auto a_len = a.length();
		if (i < a_len)
			return a[i];
		else
			return b[i - a_len];
	}
};

template<typename E2>
class string_sum_expression<empty_string_expression, E2> :
	public string_expression_support<string_sum_expression<empty_string_expression, E2>>, public string_expression_common_base {
public:
	const E2 b;
	string_sum_expression(const empty_string_expression&, const E2& bb) : b(bb) {}
	uint32_t length() const {
		return b.length();
	}
	char operator[](uint32_t i) const {
		return b[i];
	}
};

class concurrent_string : public string_expression_support<concurrent_string> {
private:
	static constexpr uint32_t internal_concurrent_string_size = 16;

	union {
		char local_data[internal_concurrent_string_size];
		struct {
			char* data;
			uint32_t length;
		} remote_data;
	} _data;
public:
	concurrent_string();
	concurrent_string(const char* source);
	concurrent_string(const char* start, const char* end);
	concurrent_string(const char* start, uint32_t size);
	concurrent_string(const concurrent_string&);
	concurrent_string(concurrent_string&&) noexcept;
	template<typename T>
	concurrent_string(const string_expression<T>&);
	~concurrent_string();

	uint32_t length() const;
	const char* c_str() const;
	char operator[](uint32_t i) const;
	concurrent_string& operator=(const concurrent_string&);
	template<typename T>
	concurrent_string& operator=(const string_expression<T>&);
	concurrent_string& operator=(concurrent_string&&) noexcept;
	concurrent_string& operator+=(const concurrent_string&);
	concurrent_string& operator+=(const char*);
	template<typename T>
	concurrent_string& operator+=(const string_expression<T>&);
	bool operator==(const char*) const;
	bool operator==(const concurrent_string&) const;
	template<typename T>
	bool operator==(const string_expression<T>&) const;
	void clear();
};

template <typename T>
struct concurrent_allocator {
	using value_type = T;
	concurrent_allocator() noexcept {}
	template <typename U>
	concurrent_allocator(const concurrent_allocator<U>&) noexcept {}
	T* allocate(size_t n);
	void deallocate(T* p, size_t n);
};

template <typename T, typename U>
constexpr bool operator== (const concurrent_allocator<T>&, const concurrent_allocator<U>&) noexcept {
	return true;
}

template <class T, class U>
constexpr bool operator!= (const concurrent_allocator<T>&, const concurrent_allocator<U>&) noexcept {
	return false;
}

template <typename T>
struct aligned_allocator_32 {
	using value_type = T;
	aligned_allocator_32() noexcept {}
	template <typename U>
	aligned_allocator_32(const aligned_allocator_32<U>&) noexcept {}
	T* allocate(size_t n);
	void deallocate(T* p, size_t n);
};

template <typename T, typename U, size_t alignment>
constexpr bool operator== (const aligned_allocator_32<T>&, const aligned_allocator_32<U>&) noexcept {
	return true;
}

template <class T, class U, size_t alignment>
constexpr bool operator!= (const aligned_allocator_32<T>&, const aligned_allocator_32<U>&) noexcept {
	return false;
}

template<typename T, uint32_t block, uint32_t index_sz, typename tag_type>
class fixed_sz_deque_iterator;

template<typename T, uint32_t block, uint32_t index_sz, typename tag_type = uint32_t>
class fixed_sz_deque {
private:
	static_assert(1ui64 << ct_log2(block) == block);

	std::atomic<T*> index_array[index_sz] = { nullptr };
	std::atomic<uint64_t> first_free = 0;
	std::atomic<uint32_t> first_free_index = 1;

	void create_new_block();
public:
	fixed_sz_deque();
	~fixed_sz_deque();

	T& at(tag_type index) const;
	T* safe_at(tag_type index) const;
	void free(tag_type index);
	template<typename U>
	void free(tag_type index, U&);
	uint32_t past_end() const;

	template<typename ...P>
	tagged_object<T, tag_type> emplace(P&& ... params);
	template<typename ...P>
	T& emplace_at(tag_type location, P&& ... params); // not thread safe
	T& ensure_reserved(tag_type location);
	template<typename F>
	void visit(tag_type location, const F& f) const;

	fixed_sz_deque_iterator<T, block, index_sz, tag_type> begin() const;
	fixed_sz_deque_iterator<T, block, index_sz, tag_type> end() const;
};

template<typename T, uint32_t block, uint32_t index_sz>
class fixed_sz_list {
private:
	static_assert(1ui64 << ct_log2(block) == block);

	std::atomic<T*> index_array[index_sz] = { nullptr };
	std::atomic<uint64_t> first_free = 0;
	std::atomic<uint64_t> first_in_list = 0;
	std::atomic<uint32_t> first_free_index = 1;

	void create_new_block();
public:
	fixed_sz_list();
	~fixed_sz_list();

	template<typename ...P>
	void emplace(P&& ... params);
	template<typename F>
	void try_pop(const F& f);
	template<typename F>
	void flush(const F& f);
};

template<typename T>
constexpr value_base_of<T> high_bit_mask = value_base_of<T>(1ui64 << (sizeof(value_base_of<T>) * 8ui64 - 1ui64));

template<typename object_type, typename index_type, uint32_t block_size, uint32_t index_size>
class stable_vector {
public:
	static_assert(1ui64 << ct_log2(block_size) == block_size);

	object_type* index_array[index_size] = { nullptr };
	uint32_t indices_in_use = 0ui32;
	index_type first_free = index_type(static_cast<value_base_of<index_type>>(to_index(index_type()) | high_bit_mask<index_type>));

	stable_vector();
	~stable_vector();

	static index_type get_id(object_type const& o) {
		if((to_index(o.id) & high_bit_mask<index_type>) != 0)
			return null_value_of<index_type>;
		return o.id;
	}
	static index_type get_id(index_type i) {
		if((to_index(i) & high_bit_mask<index_type>) != 0)
			return null_value_of<index_type>;
		return i;
	}

	object_type& get(index_type i) const; // safe from any thread
	object_type& operator[](index_type i) const; // safe from any thread
	bool is_valid_index(index_type i) const; //safe (but potentially inaccurate) from any thread, but if true, can use get without possible memory error

	object_type& get_new(); // single thread only
	void remove(index_type i); // single thread only
	object_type* get_location(index_type i); // single thread only, forces storage to expand

	template<typename T>
	void for_each(T&& f);
};

template<typename object_type, typename outer_index_type, typename inner_index_type, uint32_t block_size, uint32_t index_size>
class stable_2d_vector {
public:
	static_assert(1ui64 << ct_log2(block_size) == block_size);

	object_type* index_array[index_size] = { nullptr };
	uint32_t inner_size = 0ui32;
	uint32_t indices_in_use = 0ui32;

	stable_2d_vector();
	~stable_2d_vector();

	void reset(uint32_t inner_size); // single thread only
	void ensure_capacity(uint32_t outer_size); // single thread only
	void clear_row(outer_index_type i); // single thread only

	object_type* get_row(outer_index_type i) const; // safe from any thread
	object_type* safe_get_row(outer_index_type i); // single thread only
	object_type& get(outer_index_type i, inner_index_type j) const; // safe from any thread
	object_type& safe_get(outer_index_type i, inner_index_type j); // single thread only
	bool is_valid_index(outer_index_type i) const; //safe from any thread; if true, can use get without possible memory error
};

using stable_mk_2_tag = uint32_t;

template<typename object_type, uint32_t minimum_size, size_t memory_size>
class stable_variable_vector_storage_mk_2 {
public:
	uint64_t* backing_storage = nullptr;
	uint32_t first_free = 0ui32;

	stable_mk_2_tag free_lists[17] = { null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, 
		null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>,
		null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, 
		null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag>, null_value_of<stable_mk_2_tag> };

	stable_variable_vector_storage_mk_2();
	~stable_variable_vector_storage_mk_2();

	void reset();
	stable_mk_2_tag make_new(uint32_t capacity);
	void increase_capacity(stable_mk_2_tag& i, uint32_t new_capacity);
	void shrink_capacity(stable_mk_2_tag& i);
	void release(stable_mk_2_tag& i);
};

//general interface, safe from any thread
template<typename object_type, uint32_t minimum_size, size_t memory_size>
std::pair<object_type*, object_type*> get_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, stable_mk_2_tag i);

template<typename object_type, uint32_t minimum_size, size_t memory_size>
object_type& get(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, stable_mk_2_tag i, uint32_t inner_index); // safe to read from any thread

template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_capacity(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, stable_mk_2_tag i);

template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_size(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, stable_mk_2_tag i);

//unsorted interface
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void push_back(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag& i, object_type obj);

template<typename object_type, uint32_t minimum_size, size_t memory_size>
void pop_back(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag i);

template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_unordered_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag& i, object_type const* first, object_type const* last);

template<typename object_type, uint32_t minimum_size, size_t memory_size>
void remove_unsorted_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag i, object_type obj);

//sorted interface
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag& i, object_type obj);

template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_unique_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag& i, object_type obj);

//range passed must already be sorted
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_ordered_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag& i, object_type const* first, object_type const* last);

//range passed must already be sorted and contain no duplicates
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_unique_ordered_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag& i, object_type const* first, object_type const* last);

template<typename object_type, uint32_t minimum_size, size_t memory_size>
void remove_sorted_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, stable_mk_2_tag i, object_type obj);

//sorted interface safe from any thread
template<typename object_type, uint32_t minimum_size, size_t memory_size>
bool contains_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, stable_mk_2_tag i, object_type obj);


template<typename object_type, uint32_t minimum_size, size_t memory_size>
std::pair<object_type*, object_type*> get_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, set_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
std::pair<object_type*, object_type*> get_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, multiset_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
std::pair<object_type*, object_type*> get_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, array_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
object_type& get(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, set_tag<object_type> i, uint32_t inner_index);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_capacity(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, set_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_size(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, set_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
object_type& get(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, multiset_tag<object_type> i, uint32_t inner_index);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_capacity(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, multiset_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_size(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, multiset_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
object_type& get(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, array_tag<object_type> i, uint32_t inner_index);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_capacity(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, array_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
uint32_t get_size(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, array_tag<object_type> i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, array_tag<object_type>& i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, set_tag<object_type>& i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, multiset_tag<object_type>& i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void remove_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, array_tag<object_type>& i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void remove_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, set_tag<object_type>& i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void remove_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, multiset_tag<object_type>& i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size, typename FUNC>
void remove_item_if(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, array_tag<object_type>& i, FUNC const& f);
template<typename object_type, uint32_t minimum_size, size_t memory_size, typename FUNC>
void remove_item_if(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, set_tag<object_type>& i, FUNC const& f);
template<typename object_type, uint32_t minimum_size, size_t memory_size, typename FUNC>
void remove_item_if(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, multiset_tag<object_type>& i, FUNC const& f);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
bool contains_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, array_tag<object_type> i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
bool contains_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, set_tag<object_type> i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
bool contains_item(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, multiset_tag<object_type> i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, array_tag<object_type>& i, object_type const* first, object_type const* last);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, set_tag<object_type>& i, object_type const* first, object_type const* last);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void add_range(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, multiset_tag<object_type>& i, object_type const* first, object_type const* last);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void clear(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, array_tag<object_type>& i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void clear(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, set_tag<object_type>& i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void clear(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, multiset_tag<object_type>& i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void resize(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, array_tag<object_type>& i, uint32_t new_size);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void resize(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, set_tag<object_type>& i, uint32_t new_size);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void resize(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, multiset_tag<object_type>& i, uint32_t new_size);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void shrink(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, array_tag<object_type>& i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void shrink(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, set_tag<object_type>& i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
void shrink(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, multiset_tag<object_type>& i);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
object_type* find(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, array_tag<object_type> i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
object_type* find(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, set_tag<object_type> i, object_type obj);
template<typename object_type, uint32_t minimum_size, size_t memory_size>
object_type* find(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, multiset_tag<object_type> i, object_type obj);

namespace serialization {
	template<typename object_type, uint32_t minimum_size, size_t memory_size, typename type_tag>
	void serialize_stable_array(std::byte* &output, stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, type_tag i);
	template<typename object_type, uint32_t minimum_size, size_t memory_size, typename type_tag>
	void deserialize_stable_array(std::byte const* &input, stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size>& storage, type_tag& i);
	template<typename object_type, uint32_t minimum_size, size_t memory_size, typename type_tag>
	size_t serialize_stable_array_size(stable_variable_vector_storage_mk_2<object_type, minimum_size, memory_size> const& storage, type_tag i);
}

template<typename T, uint32_t block, uint32_t index_sz, typename tag_type>
class fixed_sz_deque_iterator {
private:
	const fixed_sz_deque<T, block, index_sz, tag_type>* parent;
	int32_t position;
public:
	fixed_sz_deque_iterator() : parent(nullptr), position(0) {}
	fixed_sz_deque_iterator(const fixed_sz_deque<T, block, index_sz, tag_type>& p) : parent(&p), position(0) {}
	fixed_sz_deque_iterator(const fixed_sz_deque<T, block, index_sz, tag_type>& p, int32_t o) : parent(&p), position(o) {}

	bool operator==(const fixed_sz_deque_iterator& o) const {
		return position == o.position;
	}
	bool operator!=(const fixed_sz_deque_iterator& o) const {
		return position != o.position;
	}
	T* operator*() const {
		return parent->safe_at(tag_type( static_cast<value_base_of<tag_type>>(position)));
	}
	T* operator[](int32_t offset) const {
		return parent->safe_at(tag_type(static_cast<value_base_of<tag_type>>(position + offset)));
	}
	T* operator->() const {
		return parent->safe_at(tag_type(static_cast<value_base_of<tag_type>>(position)));
	}
	fixed_sz_deque_iterator& operator++() {
		++position;
		return *this;
	}
	fixed_sz_deque_iterator& operator--() {
		++position;
		return *this;
	}
	fixed_sz_deque_iterator operator++(int) {
		++position;
		return fixed_sz_deque_iterator(parent, position - 1);
	}
	fixed_sz_deque_iterator operator--(int) {
		++position;
		return fixed_sz_deque_iterator(parent, position + 1);
	}
	fixed_sz_deque_iterator& operator+=(int32_t v) {
		position += v;
		return *this;
	}
	fixed_sz_deque_iterator& operator-=(int32_t v) {
		position -= v;
		return *this;
	}
	fixed_sz_deque_iterator operator+(int32_t v) {
		return fixed_sz_deque_iterator(parent, position + v);
	}
	fixed_sz_deque_iterator operator-(int32_t v) {
		return fixed_sz_deque_iterator(parent, position - v);
	}
	int32_t operator-(const fixed_sz_deque_iterator& o) {
		return position - o.position;
	}
	bool operator<(const fixed_sz_deque_iterator& o) const {
		return position < o.position;
	}
	bool operator<=(const fixed_sz_deque_iterator& o) const {
		return position <= o.position;
	}
	bool operator>(const fixed_sz_deque_iterator& o) const {
		return position > o.position;
	}
	bool operator>=(const fixed_sz_deque_iterator& o) const {
		return position >= o.position;
	}
};

template<typename T, uint32_t block, uint32_t index_sz, typename tag_type>
class std::iterator_traits<fixed_sz_deque_iterator<T, block, index_sz, tag_type>> {
public:
	using difference_type = int32_t;
	using value_type = T;
	using pointer = T*;
	using reference = T*&;
	using iterator_category = std::random_access_iterator_tag;
};
