// Keep the real project value headers usable alongside the canonical D3D9 ABI.
#ifdef FOURSTORY_REVERSE_INCLUDE_ORDER
#include <d3dx9.h>
#include <d3d9.h>
#include <afx.h>
#else
#include <afx.h>
#include <d3d9.h>
#include <d3dx9.h>
#endif
static_assert(sizeof(D3DXMATRIX)==sizeof(D3DMATRIX),"D3DX matrix layout");
static_assert(sizeof(D3DXVECTOR3)==sizeof(D3DVECTOR),"D3DX vector layout");
int main(){CString text("header-only");return text.GetLength()==11?0:1;}
