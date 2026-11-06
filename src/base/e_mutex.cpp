// Port of base/e_mutex.cpp.
#include "simmy/types_gen.hpp"

// 802CB914
EMutex::EMutex() { m_sem.Create(1, -1); }
// 802CB96C
EMutex::~EMutex() {}
// 802CB9BC
void EMutex::Acquire(char*, int, unsigned int timeout) { Acquire(timeout); }
// 802CB9F4
int EMutex::Acquire(unsigned int timeout) { return m_sem.Acquire(timeout); }
// 802CBA18
int EMutex::Release() { return m_sem.Release(); }
// 802CBA3C
void EMutex::Release(char*, int) { Release(); }
// 802CBBD4
void EMutex::iAcquire() { m_sem.iAcquire(); }
// 802CBBF8
void EMutex::iRelease() { m_sem.iRelease(); }
// 802CBC1C
EMutex& EMutex::operator++() { Acquire(0xFFFFFFFFu); return *this; }
// 802CBC64
EMutex& EMutex::operator++(int) { Acquire(0xFFFFFFFFu); return *this; }
// 802CBCF0
EMutex& EMutex::operator--() { Release(); return *this; }
// 802CBCAC
EMutex& EMutex::operator--(int) { Release(); return *this; }
