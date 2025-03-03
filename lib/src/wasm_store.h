#ifndef TREE_SITTER_WASM_H_
#define TREE_SITTER_WASM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "tree_sitter/api.h"
#include "./parser.h"

// LanguageInWasmMemory - The memory layout of a `TSLanguage` when compiled to
// wasm32. This is used to copy static language data out of the wasm memory.
typedef struct {
  uint32_t abi_version;
  uint32_t symbol_count;
  uint32_t alias_count;
  uint32_t token_count;
  uint32_t external_token_count;
  uint32_t state_count;
  uint32_t large_state_count;
  uint32_t production_id_count;
  uint32_t field_count;
  uint16_t max_alias_sequence_length;
  int32_t parse_table;
  int32_t small_parse_table;
  int32_t small_parse_table_map;
  int32_t parse_actions;
  int32_t symbol_names;
  int32_t field_names;
  int32_t field_map_slices;
  int32_t field_map_entries;
  int32_t symbol_metadata;
  int32_t public_symbol_map;
  int32_t alias_map;
  int32_t alias_sequences;
  int32_t lex_modes;
  int32_t lex_fn;
  int32_t keyword_lex_fn;
  TSSymbol keyword_capture_token;
  struct {
    int32_t states;
    int32_t symbol_map;
    int32_t create;
    int32_t destroy;
    int32_t scan;
    int32_t serialize;
    int32_t deserialize;
  } external_scanner;
  int32_t primary_state_ids;
  int32_t name;
  int32_t reserved_words;
  uint16_t max_reserved_word_set_size;
  uint32_t supertype_count;
  int32_t supertype_symbols;
  int32_t supertype_map_slices;
  int32_t supertype_map_entries;
  TSLanguageMetadata metadata;
} LanguageInWasmMemory;

// LexerInWasmMemory - The memory layout of a `TSLexer` when compiled to wasm32.
// This is used to copy mutable lexing state in and out of the wasm memory.
typedef struct {
  int32_t lookahead;
  TSSymbol result_symbol;
  int32_t advance;
  int32_t mark_end;
  int32_t get_column;
  int32_t is_at_included_range_start;
  int32_t eof;
} LexerInWasmMemory;

// The data fields of TSLexer, without the function pointers.
//
// This portion of the struct needs to be copied in and out
// of wasm memory before and after calling a scan function.
typedef struct {
  int32_t lookahead;
  TSSymbol result_symbol;
} TSLexerDataPrefix;

bool ts_wasm_store_start(TSWasmStore *self, TSLexer *lexer, const TSLanguage *language);
void ts_wasm_store_reset(TSWasmStore *self);
bool ts_wasm_store_has_error(const TSWasmStore *self);

bool ts_wasm_store_call_lex_main(TSWasmStore *self, TSStateId state);
bool ts_wasm_store_call_lex_keyword(TSWasmStore *self, TSStateId state);

uint32_t ts_wasm_store_call_scanner_create(TSWasmStore *self);
void ts_wasm_store_call_scanner_destroy(TSWasmStore *self, uint32_t scanner_address);
bool ts_wasm_store_call_scanner_scan(TSWasmStore *self, uint32_t scanner_address, uint32_t valid_tokens_ix);
uint32_t ts_wasm_store_call_scanner_serialize(TSWasmStore *self, uint32_t scanner_address, char *buffer);
void ts_wasm_store_call_scanner_deserialize(TSWasmStore *self, uint32_t scanner, const char *buffer, unsigned length);

void ts_wasm_language_retain(const TSLanguage *self);
void ts_wasm_language_release(const TSLanguage *self);

#ifdef __cplusplus
}
#endif

#endif  // TREE_SITTER_WASM_H_
