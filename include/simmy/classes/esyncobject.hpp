// @class ESyncObject
#pragma once

struct ESyncObject {
  ESyncObject();
  virtual ~ESyncObject();
  virtual int Acquire(unsigned int timeout) = 0;
  virtual int Release() = 0;
  virtual void Release(unsigned int count, unsigned int* previous);
};
