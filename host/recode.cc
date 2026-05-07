// Extracted from fate's js8.cc.
// Inserts the three Costas sync arrays into the 174-bit codeword
// to produce the 79-tone sequence for JS8 Normal.

#include <vector>
#include <cassert>

// JS8 Normal Costas array (same as FT8 "original" set).
static int costas[] = { 4, 2, 5, 6, 1, 3, 0 };

std::vector<int>
recode(int a174[])
{
  int i174 = 0;
  std::vector<int> out79;
  for(int i79 = 0; i79 < 79; i79++){
    if(i79 < 7){
      out79.push_back(costas[i79]);
    } else if(i79 >= 36 && i79 < 36+7){
      out79.push_back(costas[i79-36]);
    } else if(i79 >= 72){
      out79.push_back(costas[i79-72]);
    } else {
      int sym = (a174[i174+0] << 2) | (a174[i174+1] << 1) | (a174[i174+2] << 0);
      i174 += 3;
      out79.push_back(sym);
    }
  }
  assert((int)out79.size() == 79);
  assert(i174 == 174);
  return out79;
}
