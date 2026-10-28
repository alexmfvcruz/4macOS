// Reference vectors: the exact call sequence of EncryptBuffer (CryptographyExt.cpp)
// with Windows CryptoAPI. Output: one line per case "key-len input-len output-hex".
#include <windows.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>
static BOOL enc(BYTE* buf, DWORD& len, const BYTE* key, DWORD keylen) {
  BOOL ok = FALSE; DWORD n = len; HCRYPTPROV p = 0; HCRYPTHASH h = 0; HCRYPTKEY k = 0;
  if (CryptAcquireContext(&p, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
    if (CryptCreateHash(p, CALG_MD5, 0, 0, &h)) {
      if (CryptHashData(h, key, keylen, 0) && CryptDeriveKey(p, CALG_RC4, h, CRYPT_EXPORTABLE, &k)) {
        if (CryptEncrypt(k, 0, TRUE, 0, buf, &n, len)) { len = n; ok = TRUE; }
        CryptDestroyKey(k);
      }
      CryptDestroyHash(h);
    }
    CryptReleaseContext(p, 0);
  }
  return ok;
}
int main() {
  static const BYTE game[] = {0x41,0x35,0x24,0x24,0x38,0x41,0x46,0x53,0x31,0x33,0x41,0x31,0x3a,0x3a,0x2d,0x31,0x31,0x23,0x21,0x2e,0x2e,0x27,0xa7,0x31,0x39,0x37,0x31,0x36,0x41,0x43,0x26,0xa7,0x2f,0x44,0x31,0x3b,0x3b,0x31,0x23,0x00};
  struct { const BYTE* k; DWORD kl; } keys[] = { {game, 40}, {(const BYTE*)"abc", 4}, {(const BYTE*)"", 1} };
  DWORD lens[] = {1, 16, 17, 100, 1000};
  char provname[256] = {0}; DWORD pl = sizeof(provname); HCRYPTPROV p;
  if (CryptAcquireContext(&p, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) { CryptGetProvParam(p, PP_NAME, (BYTE*)provname, &pl, 0); CryptReleaseContext(p, 0); }
  printf("# provider: %s\n", provname);
  for (auto& key : keys) for (DWORD L : lens) {
    BYTE buf[1000]; for (DWORD i = 0; i < L; i++) buf[i] = (BYTE)(i * 7 + 3);
    DWORD len = L;
    if (!enc(buf, len, key.k, key.kl)) { printf("%lu %lu FAIL %lu\n", key.kl, L, GetLastError()); continue; }
    printf("%lu %lu ", key.kl, len); for (DWORD i = 0; i < len; i++) printf("%02x", buf[i]); printf("\n");
  }
  // Key stream across calls: two calls with final = FALSE, then two with final = TRUE.
  for (int final = 0; final < 2; final++) {
    HCRYPTPROV p2 = 0; HCRYPTHASH h2 = 0; HCRYPTKEY k2 = 0;
    CryptAcquireContext(&p2, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(p2, CALG_MD5, 0, 0, &h2); CryptHashData(h2, (const BYTE*)"abc", 4, 0);
    CryptDeriveKey(p2, CALG_RC4, h2, CRYPT_EXPORTABLE, &k2);
    printf("stream final=%d", final);
    for (int call = 0; call < 2; call++) {
      BYTE b[8] = {0}; DWORD n = 8;
      CryptEncrypt(k2, 0, final, 0, b, &n, 8);
      printf(" "); for (DWORD i = 0; i < n; i++) printf("%02x", b[i]);
    }
    printf("\n");
    CryptDestroyKey(k2); CryptDestroyHash(h2); CryptReleaseContext(p2, 0);
  }
  printf("RC4-DONE\n");
}
