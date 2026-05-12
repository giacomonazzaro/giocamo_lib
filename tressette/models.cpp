#include "models.h"

namespace tressette {

int strength(int rank) {
  // Tressette ordering: 3 > 2 > 1 > 10 > 9 > 8 > 7 > 6 > 5 > 4.
  switch (rank) {
    case 3: return 9;
    case 2: return 8;
    case 1: return 7;
    case 10: return 6;
    case 9: return 5;
    case 8: return 4;
    case 7: return 3;
    case 6: return 2;
    case 5: return 1;
    case 4: return 0;
  }
  return 0;
}

int card_thirds(int rank) {
  if (rank == 1) return 3;
  if (rank == 2 || rank == 3 || rank == 8 || rank == 9 || rank == 10) return 1;
  return 0;
}

}  // namespace tressette
