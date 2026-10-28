// Q-C12: parse the original native loader umbrella, not a replacement struct set.
#include <T3D.h>
static_assert(sizeof(WMESHVERTEX)==48,"original weighted vertex layout");
static_assert(sizeof(MESHVERTEX)==40,"original mesh vertex layout");
static_assert(sizeof(TVERTEX)==24,"original UI vertex layout");
int main(){return 0;}
