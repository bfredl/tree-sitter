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

const uint8_t *
wasm_runtime_get_custom_section(const wasm_module_t module,
                                const char *name, uint32_t *len);

typedef struct {
  uint32_t memory_size;
  uint32_t memory_align;
  uint32_t table_size;
  uint32_t table_align;
} WasmDylinkInfo;

static uint8_t read_u8(const uint8_t **p) {
  return *(*p)++;
}

static inline uint64_t read_uleb128(const uint8_t **p, const uint8_t *end) {
  uint64_t value = 0;
  unsigned shift = 0;
  do {
    if (*p == end)  return UINT64_MAX;
    value += (uint64_t)(**p & 0x7f) << shift;
    shift += 7;
  } while (*((*p)++) >= 128);
  return value;
}

static bool parse_dylink(wasm_module_t *mod, WasmDylinkInfo *info) {
  uint32_t section_size;
  const uint8_t * dylink_section = wasm_runtime_get_custom_section(*mod, "dylink.0", &section_size);
  if (!dylink_section) return false;
  const uint8_t * section_end = dylink_section + section_size;
  const uint8_t WASM_DYLINK_MEM_INFO = 0x1;

  const uint8_t *p = dylink_section;
  while (p < section_end) {
    uint8_t subsection_type = read_u8(&p);
    uint32_t subsection_size = read_uleb128(&p, section_end);
    const uint8_t *subsection_end = p + subsection_size;
    if (subsection_end > section_end) return false;
    if (subsection_type == WASM_DYLINK_MEM_INFO) {
      info->memory_size = read_uleb128(&p, subsection_end);
      info->memory_align = read_uleb128(&p, subsection_end);
      info->table_size = read_uleb128(&p, subsection_end);
      info->table_align = read_uleb128(&p, subsection_end);
      return true;
    }
    p = subsection_end;
  }
  return false;

}

static bool name_eq(const wasm_name_t *name, const char *string) {
  return strncmp(string, name->data, name->size) == 0;
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

  WasmDylinkInfo dylink_info;
  if (!parse_dylink(mod->mod, &dylink_info)) {
    fprintf(stderr, "fail:<\n");
    abort();
  }
  fprintf(stderr, "DYNAMIC: %d, %d\n", dylink_info.memory_size, dylink_info.table_size);

  uint32_t lexer_in_mem = 2048; // this is arbitrary, but likely should not be zero
  uint32_t memory_base = lexer_in_mem + sizeof(LexerInWasmMemory);
  uint32_t stack_pointer = lexer_in_mem - 16;

  wasm_table_t *functable;

  wasm_valtype_t *i32 = wasm_valtype_new_i32();
  wasm_globaltype_t *glob_mut = wasm_globaltype_new(i32, true);
  wasm_globaltype_t *glob_const = wasm_globaltype_new(i32, false);

  int table_base = 5;

  for (size_t i = 0; i < import_types.size; i++) {
    wasm_importtype_t *type = import_types.data[i];
    const wasm_name_t *name = wasm_importtype_name(type);
    wasm_extern_t *resolved = NULL;
    switch(wasm_externtype_kind(wasm_importtype_type(type))) {
      case WASM_EXTERN_FUNC:
        fprintf(stderr, "func: "); break;
      case WASM_EXTERN_GLOBAL:;
        uint32_t value = 0;
        bool mut = false;
        if (name_eq(name, "__stack_pointer")) {
          value = stack_pointer; mut = true;
        } else if (name_eq(name, "__memory_base")) {
          value = memory_base;
        } else if (name_eq(name, "__table_base")) {
          value = table_base;
        } else {
          goto importfail;
        }
        wasm_global_t *glob = wasm_global_new(mod->store, mut ? glob_mut : glob_const, &(wasm_val_t)WASM_I32_VAL(value));
        resolved = wasm_global_as_extern(glob);
        fprintf(stderr, "global: "); break;
      case WASM_EXTERN_TABLE:
        if (name_eq(name, "__indirect_function_table")) {
        }
        fprintf(stderr, "table: "); break;
      case WASM_EXTERN_MEMORY:
        fprintf(stderr, "mem: "); break;
    }
    imports.data[i] = resolved;
    fprintf(stderr, "%.*s\n", (int)name->size, name->data);
  }

importfail:
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
