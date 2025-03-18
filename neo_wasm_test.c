// Filename - test-json-parser.c
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <tree_sitter/api.h>

// gcc -DBACKHOLD -c neo_wasm_test.c && g++ -o ref neo_wasm_test.o zig-out/lib/libtree-sitter.a ../tree-sitter-html/libtree-sitter-html.git.a -fsanitize=undefine
// zig cc -DBACKHOLD neo_wasm_test.c -o ref zig-out/lib/libtree-sitter.a ../tree-sitter-html/parser.o ../tree-sitter-html/scanner.o -fsanitize=undefined
// zig run -DBACKHOLD neo_wasm_test.c zig-out/lib/libtree-sitter.a ../tree-sitter-html/parser.o ../tree-sitter-html/scanner.o -lc

#ifdef USE_WASMTIME
#include "wasm.h"
#endif

static char *read_file(const char *path, size_t *len)
{
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return NULL;
  }
  fseek(file, 0L, SEEK_END);
  *len = (size_t)ftell(file);
  fseek(file, 0L, SEEK_SET);
  char *data = malloc(*len+1);
  if (fread(data, *len, 1, file) != 1) {
    free(data);
    fclose(file);
    return NULL;
  }
  data[*len] = 0;
  fclose(file);
  return data;
}

static const char *wasmerr_to_str(TSWasmErrorKind werr)
{
  switch (werr) {
  case TSWasmErrorKindParse:
    return "PARSE";
  case TSWasmErrorKindCompile:
    return "COMPILE";
  case TSWasmErrorKindInstantiate:
    return "INSTANTIATE";
  case TSWasmErrorKindAllocate:
    return "ALLOCATE";
  default:
    return "UNKNOWN";
  }
}

int main(int argc, char **argv) {
#ifdef BACKHOLD
  TSLanguage *tree_sitter_html(void), *lang = tree_sitter_html();
#else
  TSWasmStore *ts_wasmstore;

  if (argc < 2) return 3;

  size_t file_size = 0;
  char *data = read_file(argv[1], &file_size);

  if (data == NULL) {
    return 5;
  }

  clock_t ladda = clock();
  TSWasmEngine *engine = NULL;
#ifdef USE_WASMTIME
    engine = wasm_engine_new();
#endif

  TSWasmError werr = { 0 };
  ts_wasmstore = ts_wasm_store_new(engine, &werr);
  if (werr.kind > 0) {
    fprintf(stderr, "Error creating wasm store: (%s) %s", wasmerr_to_str(werr.kind), werr.message);
    return 1;
  }

  const TSLanguage *lang = ts_wasm_store_load_language(ts_wasmstore, argv[2], data,
                                                       (uint32_t)file_size, &werr);
  clock_t endtime = clock() - ladda;
  fprintf(stderr, "load TIME: %ld\n", endtime);
#endif

  if (lang) {
    fprintf(stderr, "OK %d\n", ts_language_abi_version(lang));

  } else {
    fprintf(stderr, "FAIL\n");
    return 5;
  }

  TSParser *parser = ts_parser_new();
#ifdef USE_WASMTIME
  ts_parser_set_wasm_store(parser, ts_wasmstore);
#else
  ts_parser_set_wasm_store(parser, (TSWasmStore *)lang);
#endif
  ts_parser_set_language(parser, lang);

  //fprintf(stderr, "is set! \n");

  const char *source_code = "<html><body>halloj</body></html>";
  if (argc >= 4) {
    size_t lenni;
    source_code = read_file(argv[3], &lenni);
    if (!source_code) return 8;
  }

  clock_t klocka = clock();
  TSTree *tree = ts_parser_parse_string( parser, NULL, source_code, strlen(source_code));
  clock_t time = clock() - klocka;

  fprintf(stderr, "is tree: %d\n", !!tree);
  fprintf(stderr, "is TIME: %ld\n", time);

#if !defined(BACKHOLD) && !defined(USE_WASMTIME)
  void print_counter(void);
  print_counter();
#endif

  ts_tree_print_dot_graph(tree, 1);
  return 0;
}
