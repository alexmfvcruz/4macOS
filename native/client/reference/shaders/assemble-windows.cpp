// Assemble each original client shader exactly as CD3DDevice does
// (Engine Lib/D3DDevice.cpp:883-889 and :918): resource bytes and size,
// no defines, no include handler, flags 0. Writes <name>.bin per shader.
#include <windows.h>
#include <d3dx9.h>
#include <cstdio>
#include <vector>
int main(int argc, char** argv) {
  int failed = 0;
  for (int i = 2; i < argc; i++) {
    char in[MAX_PATH], out[MAX_PATH];
    sprintf_s(in, "%s\\%s", argv[1], argv[i]);
    FILE* f = nullptr; fopen_s(&f, in, "rb");
    if (!f) { printf("%s\tOPEN_FAILED\n", argv[i]); failed++; continue; }
    std::vector<char> text; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) text.insert(text.end(), b, b + n);
    fclose(f);
    LPD3DXBUFFER code = nullptr, errors = nullptr;
    HRESULT hr = D3DXAssembleShader(text.data(), (UINT)text.size(), NULL, NULL, 0, &code, &errors);
    printf("%s\tsource_bytes=%u\thr=0x%08lX", argv[i], (unsigned)text.size(), (unsigned long)hr);
    if (SUCCEEDED(hr) && code) {
      sprintf_s(out, "%s.bin", argv[i]);
      FILE* o = nullptr; fopen_s(&o, out, "wb");
      fwrite(code->GetBufferPointer(), 1, code->GetBufferSize(), o); fclose(o);
      printf("\tcode_bytes=%lu\n", (unsigned long)code->GetBufferSize());
      code->Release();
    } else { printf("\t%s\n", errors ? (const char*)errors->GetBufferPointer() : ""); failed++; }
    if (errors) errors->Release();
  }
  printf("ASM-DONE failed=%d\n", failed);
  return failed ? 1 : 0;
}
