#include "tree_sitter/api.h"
#include "./parser.h"
#include <stdint.h>

#include "./alloc.h"
#include "./array.h"
#include "./atomic.h"
#include "./language.h"
#include "./lexer.h"
#include "./wasm/wasm-stdlib.h"
#include "./wasm_store.h"
#include "wasm_c_api.h"

typedef struct {
  wasm_store_t *store;
  wasm_module_t *mod;
  wasm_instance_t *in;
  // TODO: c side struct + zig side struct should be one alloc
  char* static_mem_copy;
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

struct TSWasmStore {
  wasm_engine_t *engine;
  // this is bullshit, just because the internal api abuses WASMStore as a
  // pointer to a language ("current_lexer"/"current_instance").
  LanguageWasmModule *current;
};

TSWasmStore *ts_wasm_store_new(TSWasmEngine *engine, TSWasmError *wasm_error) {
  TSWasmStore *store = ts_malloc(sizeof(TSWasmStore));
  store->engine = wasm_engine_new();
  return store;
}

const TSLanguage *ts_wasm_store_load_language(
  TSWasmStore *self,
  const char *language_name,
  const char *wasm,
  uint32_t wasm_len,
  TSWasmError *wasm_error
) {
  LanguageWasmModule *mod = ts_malloc(sizeof(LanguageWasmModule));

  wasm_store_t *store = wasm_store_new(self->engine);
  wasm_byte_vec_t text;
  wasm_byte_vec_new(&text, wasm_len, wasm);
  mod->mod = wasm_module_new(store, &text);

  wasm_importtype_vec_t import_types;
  wasm_module_imports(mod->mod, &import_types);

  wasm_extern_vec_t imports;
  wasm_extern_vec_new_uninitialized(&imports, import_types.size);

  for (size_t i = 0; i < import_types.size; i++) {
    wasm_importtype_t *type = import_types.data[i];
    const wasm_name_t *name = wasm_importtype_name(type);
    switch(wasm_externtype_kind(wasm_importtype_type(type))) {
      case WASM_EXTERN_FUNC:
        fprintf(stderr, "func: "); break;
      case WASM_EXTERN_GLOBAL:
        fprintf(stderr, "global: "); break;
      case WASM_EXTERN_TABLE:
        fprintf(stderr, "table: "); break;
      case WASM_EXTERN_MEMORY:
        fprintf(stderr, "mem: "); break;
    }
    fprintf(stderr, "%.*s\n", (int)name->size, name->data);
  }

  abort();
}

// If the WASM feature is not enabled, define dummy versions of all of the
// wasm-related functions.

void ts_wasm_store_delete(TSWasmStore *self) {
  (void)self;
}

bool ts_wasm_store_start(
  TSWasmStore *self,
  TSLexer *lexer,
  const TSLanguage *language
) {
  (void)self;
  (void)lexer;
  (void)language;
  return false;
}

void ts_wasm_store_reset(TSWasmStore *self) {
  (void)self;
}

bool ts_wasm_store_call_lex_main(TSWasmStore *self, TSStateId state) {
  (void)self;
  (void)state;
  return false;
}

bool ts_wasm_store_call_lex_keyword(TSWasmStore *self, TSStateId state) {
  (void)self;
  (void)state;
  return false;
}

uint32_t ts_wasm_store_call_scanner_create(TSWasmStore *self) {
  (void)self;
  return 0;
}

void ts_wasm_store_call_scanner_destroy(
  TSWasmStore *self,
  uint32_t scanner_address
) {
  (void)self;
  (void)scanner_address;
}

bool ts_wasm_store_call_scanner_scan(
  TSWasmStore *self,
  uint32_t scanner_address,
  uint32_t valid_tokens_ix
) {
  (void)self;
  (void)scanner_address;
  (void)valid_tokens_ix;
  return false;
}

uint32_t ts_wasm_store_call_scanner_serialize(
  TSWasmStore *self,
  uint32_t scanner_address,
  char *buffer
) {
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
  (void)self;
  (void)scanner_address;
  (void)buffer;
  (void)length;
}

bool ts_wasm_store_has_error(const TSWasmStore *self) {
  (void)self;
  return false;
}

bool ts_language_is_wasm(const TSLanguage *self) {
  (void)self;
  return false;
}

void ts_wasm_language_retain(const TSLanguage *self) {
  (void)self;
}

void ts_wasm_language_release(const TSLanguage *self) {
  (void)self;
}
