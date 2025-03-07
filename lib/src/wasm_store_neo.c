#include "tree_sitter/api.h"
#include "./parser.h"
#include "./array.h"
#include "./language.h"
#include <string.h>

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

typedef struct WASMLanguage WASMLanguage;
typedef struct {
  // TODO: c side struct + zig side struct should be one alloc
  WASMLanguage *wasm_lang;
  TSLexer *current_lexer;
  int32_t external_states_address;
  int32_t lex_main_fn_index;
  int32_t lex_keyword_fn_index;
  int32_t scanner_create_fn_index;
  int32_t scanner_destroy_fn_index;
  int32_t scanner_serialize_fn_index;
  int32_t scanner_deserialize_fn_index;
  int32_t scanner_scan_fn_index;
  uint32_t lexer_address;
} LanguageWasmModule;

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

static bool ts_wasm_store__sentinel_lex_fn(TSLexer *_lexer, TSStateId state) {
  return false;
}

// ZIG BINDING
WASMLanguage *ts_wasm_load(const char *data, size_t len, const char* lang_name, size_t lexer_size);
char *ts_wasm_get_mem(WASMLanguage *lang);

typedef struct {
    uint32_t lang_in_mem;
    uint32_t lexer_in_mem;
} SharedMemInfo;

void ts_wasm_get_mem_info(WASMLanguage* lang, SharedMemInfo *info);
char *ts_wasm_reset_heap(WASMLanguage *lang, size_t serialize_buffer_size);
uint32_t ts_wasm_serialize_buffer(WASMLanguage *lang);
uint32_t ts_wasm_call_tbl_func(WASMLanguage *lang, uint32_t table_idx, int n_res, int n_arg, uint32_t arg1, uint32_t arg2, uint32_t arg3);

// END ZIG

TSWasmStore *ts_wasm_store_new(TSWasmEngine *engine, TSWasmError *wasm_error) {
  return NULL;
}

TSLanguage *big_thing_copy(WASMLanguage *lang, LanguageWasmModule* language_module);
const TSLanguage *ts_wasm_store_load_language(
  TSWasmStore *self,
  const char *language_name,
  const char *wasm,
  uint32_t wasm_len,
  TSWasmError *wasm_error
) {
  WASMLanguage *lang = ts_wasm_load(wasm, wasm_len, language_name, sizeof(LexerInWasmMemory));

  LanguageWasmModule *language_module = ts_malloc(sizeof(LanguageWasmModule));
  TSLanguage *language = big_thing_copy(lang, language_module);
  language->lex_fn = ts_wasm_store__sentinel_lex_fn;
  language->keyword_lex_fn = (bool (*)(TSLexer *, TSStateId))language_module;

  return language;
}

void ts_wasm_store_delete(TSWasmStore *self) {
  (void)self;
}

static LanguageWasmModule *unself(TSWasmStore *self) {
  return (LanguageWasmModule *)(((TSLanguage *)self)->keyword_lex_fn);
}

bool ts_wasm_store_start(
  TSWasmStore *self,
  TSLexer *lexer,
  const TSLanguage *language
) {
  LanguageWasmModule *language_module = (void *)language->keyword_lex_fn;
  language_module->current_lexer = lexer;
  ts_wasm_reset_heap(language_module->wasm_lang, TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
  fprintf(stderr, "IS START\n");
  return true;
}

void ts_wasm_store_reset(TSWasmStore *self) {
  (void)self;
}

bool ts_wasm_store_call_lex_main(TSWasmStore *self, TSStateId state) {
  fprintf(stderr, "lex_main\n");
  abort();
  (void)self;
  (void)state;
  return false;
}

bool ts_wasm_store_call_lex_keyword(TSWasmStore *self, TSStateId state) {
  fprintf(stderr, "lex_keyword\n");
  abort();
  (void)self;
  (void)state;
  return false;
}

uint32_t ts_wasm_store_call_scanner_create(TSWasmStore *self) {
  LanguageWasmModule *mod = unself(self);
  fprintf(stderr, "scanner_create\n");
  uint32_t addr = ts_wasm_call_tbl_func(mod->wasm_lang, mod->scanner_create_fn_index, 1, 0, 0, 0, 0);
  fprintf(stderr, "creat at %d\n", addr);
  return addr;
}

void ts_wasm_store_call_scanner_destroy(
  TSWasmStore *self,
  uint32_t scanner_address
) {
  (void)self;
  (void)scanner_address;
}

typedef struct {
  int32_t lookahead;
  TSSymbol result_symbol;
} TSLexerDataPrefix;
bool ts_wasm_store_call_scanner_scan(
  TSWasmStore *self,
  uint32_t scanner_address,
  uint32_t valid_tokens_ix
) {
  fprintf(stderr, "scanner_scann\n");
  LanguageWasmModule *mod = unself(self);
  char *memory = ts_wasm_get_mem(mod->wasm_lang);

  memcpy(
    &memory[mod->lexer_address],
    mod->current_lexer,
    sizeof(TSLexerDataPrefix)
  );

  uint32_t valid_tokens_address =
    mod->external_states_address +
    (valid_tokens_ix * sizeof(bool));
  uint32_t retval = ts_wasm_call_tbl_func(mod->wasm_lang, mod->scanner_deserialize_fn_index, 0, 3, scanner_address, mod->lexer_address, valid_tokens_address);
  // TODO BLUFF: if (mod->has_error) return false;

  memcpy(
    mod->current_lexer,
    &memory[mod->lexer_address],
    sizeof(TSLexerDataPrefix)
  );

  return retval;
}

uint32_t ts_wasm_store_call_scanner_serialize(
  TSWasmStore *self,
  uint32_t scanner_address,
  char *buffer
) {
  fprintf(stderr, "scanner_serialize\n");
  abort();
  (void)self;
  (void)scanner_address;
  (void)buffer;
  return 0;
}

void ts_wasm_store_call_scanner_deserialize(
  TSWasmStore *self,
  uint32_t scanner_address,
  const char *buffer,
  unsigned length
) {
  LanguageWasmModule *mod = unself(self);
  uint32_t serialization_buffer_address = ts_wasm_serialize_buffer(mod->wasm_lang);
  char *memory = ts_wasm_get_mem(mod->wasm_lang);
  if (length > 0) {
    memcpy(memory+serialization_buffer_address, buffer, length);
  }

  fprintf(stderr, "scanner_deserialize %d %d %d\n", scanner_address, serialization_buffer_address, length);
  ts_wasm_call_tbl_func(mod->wasm_lang, mod->scanner_deserialize_fn_index, 0, 3, scanner_address, serialization_buffer_address, length);
}

bool ts_wasm_store_has_error(const TSWasmStore *self) {
  (void)self;
  return false;
}

bool ts_language_is_wasm(const TSLanguage *self) {
  fprintf(stderr, "it is? %d\n", self->lex_fn == ts_wasm_store__sentinel_lex_fn);
  return self->lex_fn == ts_wasm_store__sentinel_lex_fn;
}

void ts_wasm_language_retain(const TSLanguage *self) {
  (void)self;
}

void ts_wasm_language_release(const TSLanguage *self) {
  (void)self;
}

#define array_len(a) (sizeof(a) / sizeof(a[0]))
typedef Array(char) StringData;
static void *copy(const void *data, size_t size) {
  void *result = ts_malloc(size);
  memcpy(result, data, size);
  return result;
}

static void *copy_unsized_static_array(
  const uint8_t *data,
  int32_t start_address,
  const int32_t all_addresses[],
  size_t address_count
) {
  int32_t end_address = 0;
  for (unsigned i = 0; i < address_count; i++) {
    if (all_addresses[i] > start_address) {
      if (!end_address || all_addresses[i] < end_address) {
        end_address = all_addresses[i];
      }
    }
  }

  if (!end_address) return NULL;
  size_t size = end_address - start_address;
  void *result = ts_malloc(size);
  memcpy(result, &data[start_address], size);
  return result;
}

static void *copy_strings(
  const uint8_t *data,
  int32_t array_address,
  size_t count,
  StringData *string_data
) {
  const char **result = ts_malloc(count * sizeof(char *));
  for (unsigned i = 0; i < count; i++) {
    int32_t address;
    memcpy(&address, &data[array_address + i * sizeof(address)], sizeof(address));
    if (address == 0) {
      result[i] = (const char *)-1;
    } else {
      const uint8_t *string = &data[address];
      uint32_t len = strlen((const char *)string);
      result[i] = (const char *)(uintptr_t)string_data->size;
      array_extend(string_data, len + 1, string);
    }
  }
  for (unsigned i = 0; i < count; i++) {
    if (result[i] == (const char *)-1) {
      result[i] = NULL;
    } else {
      result[i] = string_data->contents + (uintptr_t)result[i];
    }
  }
  return result;
}

static void *copy_string(
  const uint8_t *data,
  int32_t address
) {
  const char *string = (const char *)&data[address];
  size_t len = strlen(string);
  char *result = ts_malloc(len + 1);
  memcpy(result, string, len + 1);
  return result;
}

TSLanguage *big_thing_copy(WASMLanguage *lang, LanguageWasmModule* language_module) {
  LanguageInWasmMemory wasm_language;
  SharedMemInfo sh;
  char *memory = ts_wasm_get_mem(lang);
  ts_wasm_get_mem_info(lang, &sh);
  memcpy(&wasm_language, &memory[sh.lang_in_mem], sizeof(LanguageInWasmMemory));
  language_module->lexer_address = sh.lexer_in_mem;

  bool has_supertypes =
    wasm_language.abi_version > LANGUAGE_VERSION_WITH_RESERVED_WORDS &&
    wasm_language.supertype_count > 0;

  int32_t addresses[] = {
    wasm_language.parse_table,
    wasm_language.small_parse_table,
    wasm_language.small_parse_table_map,
    wasm_language.parse_actions,
    wasm_language.symbol_names,
    wasm_language.field_names,
    wasm_language.field_map_slices,
    wasm_language.field_map_entries,
    wasm_language.symbol_metadata,
    wasm_language.public_symbol_map,
    wasm_language.alias_map,
    wasm_language.alias_sequences,
    wasm_language.lex_modes,
    wasm_language.lex_fn,
    wasm_language.keyword_lex_fn,
    wasm_language.primary_state_ids,
    wasm_language.name,
    wasm_language.reserved_words,
    has_supertypes ? wasm_language.supertype_symbols : 0,
    has_supertypes ? wasm_language.supertype_map_entries : 0,
    has_supertypes ? wasm_language.supertype_map_slices : 0,
    wasm_language.external_token_count > 0 ? wasm_language.external_scanner.states : 0,
    wasm_language.external_token_count > 0 ? wasm_language.external_scanner.symbol_map : 0,
    wasm_language.external_token_count > 0 ? wasm_language.external_scanner.create : 0,
    wasm_language.external_token_count > 0 ? wasm_language.external_scanner.destroy : 0,
    wasm_language.external_token_count > 0 ? wasm_language.external_scanner.scan : 0,
    wasm_language.external_token_count > 0 ? wasm_language.external_scanner.serialize : 0,
    wasm_language.external_token_count > 0 ? wasm_language.external_scanner.deserialize : 0,
    sh.lang_in_mem,
    0,  // TODO: very bluff
  };
  uint32_t address_count = array_len(addresses);

  TSLanguage *language = ts_calloc(1, sizeof(TSLanguage));
  StringData symbol_name_buffer = array_new();
  StringData field_name_buffer = array_new();

  *language = (TSLanguage) {
    .abi_version = wasm_language.abi_version,
    .symbol_count = wasm_language.symbol_count,
    .alias_count = wasm_language.alias_count,
    .token_count = wasm_language.token_count,
    .external_token_count = wasm_language.external_token_count,
    .state_count = wasm_language.state_count,
    .large_state_count = wasm_language.large_state_count,
    .production_id_count = wasm_language.production_id_count,
    .field_count = wasm_language.field_count,
    .supertype_count = wasm_language.supertype_count,
    .max_alias_sequence_length = wasm_language.max_alias_sequence_length,
    .keyword_capture_token = wasm_language.keyword_capture_token,
    .metadata = wasm_language.metadata,
    .parse_table = copy(
      &memory[wasm_language.parse_table],
      wasm_language.large_state_count * wasm_language.symbol_count * sizeof(uint16_t)
    ),
    .parse_actions = copy_unsized_static_array(
      memory,
      wasm_language.parse_actions,
      addresses,
      address_count
    ),
    .symbol_names = copy_strings(
      memory,
      wasm_language.symbol_names,
      wasm_language.symbol_count + wasm_language.alias_count,
      &symbol_name_buffer
    ),
    .symbol_metadata = copy(
      &memory[wasm_language.symbol_metadata],
      (wasm_language.symbol_count + wasm_language.alias_count) * sizeof(TSSymbolMetadata)
    ),
    .public_symbol_map = copy(
      &memory[wasm_language.public_symbol_map],
      (wasm_language.symbol_count + wasm_language.alias_count) * sizeof(TSSymbol)
    ),
    .lex_modes = copy(
      &memory[wasm_language.lex_modes],
      wasm_language.state_count * sizeof(TSLexerMode)
    ),
  };

  if (language->field_count > 0 && language->production_id_count > 0) {
    language->field_map_slices = copy(
      &memory[wasm_language.field_map_slices],
      wasm_language.production_id_count * sizeof(TSMapSlice)
    );

    // Determine the number of field map entries by finding the greatest index
    // in any of the slices.
    uint32_t field_map_entry_count = 0;
    for (uint32_t i = 0; i < wasm_language.production_id_count; i++) {
      TSMapSlice slice = language->field_map_slices[i];
      uint32_t slice_end = slice.index + slice.length;
      if (slice_end > field_map_entry_count) {
        field_map_entry_count = slice_end;
      }
    }

    language->field_map_entries = copy(
      &memory[wasm_language.field_map_entries],
      field_map_entry_count * sizeof(TSFieldMapEntry)
    );
    language->field_names = copy_strings(
      memory,
      wasm_language.field_names,
      wasm_language.field_count + 1,
      &field_name_buffer
    );
  }

  if (has_supertypes) {
    language->supertype_symbols = copy(
      &memory[wasm_language.supertype_symbols],
      wasm_language.supertype_count * sizeof(TSSymbol)
    );

    // Determine the number of supertype map slices by finding the greatest
    // supertype ID.
    int largest_supertype = 0;
    for (unsigned i = 0; i < language->supertype_count; i++) {
      TSSymbol supertype = language->supertype_symbols[i];
      if (supertype > largest_supertype) {
        largest_supertype = supertype;
      }
    }

    language->supertype_map_slices = copy(
      &memory[wasm_language.supertype_map_slices],
      (largest_supertype + 1) * sizeof(TSMapSlice)
    );

    TSSymbol last_supertype = language->supertype_symbols[language->supertype_count - 1];
    TSMapSlice last_slice = language->supertype_map_slices[last_supertype];
    uint32_t supertype_map_entry_count = last_slice.index + last_slice.length;

    language->supertype_map_entries = copy(
      &memory[wasm_language.supertype_map_entries],
      supertype_map_entry_count * sizeof(char *)
    );
  }

  if (language->max_alias_sequence_length > 0 && language->production_id_count > 0) {
    // The alias map contains symbols, alias counts, and aliases, terminated by a null symbol.
    int32_t alias_map_size = 0;
    for (;;) {
      TSSymbol symbol;
      memcpy(&symbol, &memory[wasm_language.alias_map + alias_map_size], sizeof(symbol));
      alias_map_size += sizeof(TSSymbol);
      if (symbol == 0) break;
      uint16_t value_count;
      memcpy(&value_count, &memory[wasm_language.alias_map + alias_map_size], sizeof(value_count));
      alias_map_size += value_count * sizeof(TSSymbol);
    }
    language->alias_map = copy(
      &memory[wasm_language.alias_map],
      alias_map_size * sizeof(TSSymbol)
    );
    language->alias_sequences = copy(
      &memory[wasm_language.alias_sequences],
      wasm_language.production_id_count * wasm_language.max_alias_sequence_length * sizeof(TSSymbol)
    );
  }

  if (language->state_count > language->large_state_count) {
    uint32_t small_state_count = wasm_language.state_count - wasm_language.large_state_count;
    language->small_parse_table_map = copy(
      &memory[wasm_language.small_parse_table_map],
      small_state_count * sizeof(uint32_t)
    );
    language->small_parse_table = copy_unsized_static_array(
      memory,
      wasm_language.small_parse_table,
      addresses,
      address_count
    );
  }

  if (language->abi_version >= LANGUAGE_VERSION_WITH_PRIMARY_STATES) {
    language->primary_state_ids = copy(
      &memory[wasm_language.primary_state_ids],
      wasm_language.state_count * sizeof(TSStateId)
    );
  }

  if (language->abi_version >= LANGUAGE_VERSION_WITH_RESERVED_WORDS) {
    language->name = copy_string(memory, wasm_language.name);
    language->reserved_words = copy(
        &memory[wasm_language.reserved_words],
        wasm_language.max_reserved_word_set_size * sizeof(TSSymbol)
    );
    language->max_reserved_word_set_size = wasm_language.max_reserved_word_set_size;
  }

  if (language->external_token_count > 0) {
    language->external_scanner.symbol_map = copy(
      &memory[wasm_language.external_scanner.symbol_map],
      wasm_language.external_token_count * sizeof(TSSymbol)
    );
    language->external_scanner.states = (void *)(uintptr_t)wasm_language.external_scanner.states;
  }

  *language_module = (LanguageWasmModule) {
    .wasm_lang = lang,
    .external_states_address = wasm_language.external_scanner.states,
    .lex_main_fn_index = wasm_language.lex_fn,
    .lex_keyword_fn_index = wasm_language.keyword_lex_fn,
    .scanner_create_fn_index = wasm_language.external_scanner.create,
    .scanner_destroy_fn_index = wasm_language.external_scanner.destroy,
    .scanner_serialize_fn_index = wasm_language.external_scanner.serialize,
    .scanner_deserialize_fn_index = wasm_language.external_scanner.deserialize,
    .scanner_scan_fn_index = wasm_language.external_scanner.scan,
  };
  return language;
}
