// Filename - test-json-parser.c
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <tree_sitter/api.h>

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
  TSParser *parser = ts_parser_new();
  TSWasmStore *ts_wasmstore;

  if (argc < 2) return 3;

  TSWasmError werr = { 0 };
  ts_wasmstore = ts_wasm_store_new(NULL, &werr);

  if (werr.kind > 0) {
    fprintf(stderr, "Error creating wasm store: (%s) %s", wasmerr_to_str(werr.kind), werr.message);
    return 1;
  }

  size_t file_size = 0;
  char *data = read_file(argv[1], &file_size);

  if (data == NULL) {
    return 5;
  }

  const TSLanguage *lang = ts_wasm_store_load_language(ts_wasmstore, argv[2], data,
                                                       (uint32_t)file_size, &werr);
  if (lang) {
    fprintf(stderr, "OK");
  } else {
    fprintf(stderr, "FAIL\n");
  }

}
