#ifndef VOX_CALLBACK_H
#define VOX_CALLBACK_H

struct VoxCallback {
  /// Called when voxel xyz intersects triangle
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          unsigned long long trigIdx) {}
};

#endif