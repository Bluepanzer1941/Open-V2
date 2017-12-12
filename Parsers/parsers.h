#pragma once
#include <vector>
#include "common\\common.h"

enum class association_type : unsigned short { none, eq, lt, le, gt, ge, ne, eq_default, list};

enum class token_type {
	identifier,
	quoted_string,
	special_identifier,
	brace,
	unknown
};

struct token_and_type {
	const char* start = nullptr;
	const char* end = nullptr;
	token_type type = token_type::unknown;
};

class token_group {
public:
	token_and_type token;
	association_type association = association_type::none;
	unsigned short group_size = 0;
};

template<typename T>
T token_to(const token_and_type& in);

class end_brace_finder;
class vector_of_strings_operation;
class tokenize_char_stream_operation;

float parse_float(const char* start, const char* end);
bool parse_bool(const char* start, const char* end);
double parse_double(const char* start, const char* end);
int32_t parse_int(const char* start, const char* end);
uint32_t parse_uint(const char* start, const char* end);

void parse_pdx_file(std::vector<token_group>& results, const char* file_start, const char* file_end);
void parse_lua_file(std::vector<token_group>& results, const char* file_start, const char* file_end);

template<size_t count_values, typename T>
char* CALL parse_fixed_amount_csv_values(char* start, char* end, char seperator, const T& function);
template<typename T>
char* CALL parse_first_and_nth_csv_values(uint32_t nth, char* start, char* end, char seperator, const T& function);
association_type parse_association_type_b(const char* start, const char* end);

char16_t win1250toUTF16(char in);

template<size_t N>
bool is_fixed_token(const token_and_type& g, const char(&t)[N]);
template<size_t N>
bool is_fixed_token_ci(const token_and_type& g, const char(&t)[N]);
template<typename T>
bool any_token(const token_group* start, const token_group* end, const T& function);
template<typename T>
bool any_token_recursive(const token_group* start, const token_group* end, const T& function);
template<typename T>
bool all_tokens(const token_group* start, const token_group* end, const T& function);
template<typename T>
bool all_tokens_recursive(const token_group* start, const token_group* end, const T& function);
template<typename T>
token_group* find_token(token_group* start, const token_group* end, const T& function);
template<typename T>
token_group* find_token_recursive(token_group* start, const token_group* end, const T& function);
template<typename T>
void forall_tokens(token_group* start, const token_group* end, const T& function);
template<typename T>
void forall_tokens_recursive(token_group* start, const token_group* end, const T& function);
template<typename T>
void forall_tokens(const token_group* start, const token_group* end, const T& function);
template<typename T>
void forall_tokens_recursive(const token_group* start, const token_group* end, const T& function);