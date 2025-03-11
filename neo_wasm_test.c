// Filename - test-json-parser.c
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <tree_sitter/api.h>

// gcc -DBACKHOLD -c neo_wasm_test.c && g++ -o ref neo_wasm_test.o zig-out/lib/libtree-sitter.a ../tree-sitter-html/libtree-sitter-html.git.a -fsanitize=undefine
// zig cc -DBACKHOLD neo_wasm_test.c -o ref zig-out/lib/libtree-sitter.a ../tree-sitter-html/parser.o ../tree-sitter-html/scanner.o -fsanitize=undefined
// zig run -DBACKHOLD neo_wasm_test.c zig-out/lib/libtree-sitter.a ../tree-sitter-html/parser.o ../tree-sitter-html/scanner.o -lc

static char *read_file(const char *path, size_t *len)
{
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return NULL;
  }
  fseek(file, 0L, SEEK_END);
  *len = (size_t)ftell(file);
  fseek(file, 0L, SEEK_SET);
  char *data = malloc(*len);
  if (fread(data, *len, 1, file) != 1) {
    free(data);
    fclose(file);
    return NULL;
  }
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

  TSWasmError werr = { 0 };
  ts_wasmstore = ts_wasm_store_new(NULL, &werr);
  if (werr.kind > 0) {
    fprintf(stderr, "Error creating wasm store: (%s) %s", wasmerr_to_str(werr.kind), werr.message);
    return 1;
  }

  const TSLanguage *lang = ts_wasm_store_load_language(ts_wasmstore, argv[2], data,
                                                       (uint32_t)file_size, &werr);
#endif

  if (lang) {
    fprintf(stderr, "OK %d\n", ts_language_abi_version(lang));

  } else {
    fprintf(stderr, "FAIL\n");
    return 5;
  }

  TSParser *parser = ts_parser_new();
  ts_parser_set_wasm_store(parser, (TSWasmStore *)lang);
  ts_parser_set_language(parser, lang);

  //fprintf(stderr, "is set! \n");

  const char *source_code = "<html><body>halloj</body></html>";
  TSTree *tree = ts_parser_parse_string( parser, NULL, source_code, strlen(source_code));

  fprintf(stderr, "is tree: %d\n", !!tree);

  ts_tree_print_dot_graph(tree, 1);
  return 0;
}
