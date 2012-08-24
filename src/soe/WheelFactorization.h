//
// Copyright (c) 2012 Kim Walisch, <kim.walisch@gmail.com>.
// All rights reserved.
//
// This file is part of primesieve.
// Homepage: http://primesieve.googlecode.com
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//   * Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above
//     copyright notice, this list of conditions and the following
//     disclaimer in the documentation and/or other materials provided
//     with the distribution.
//   * Neither the name of the author nor the names of its
//     contributors may be used to endorse or promote products derived
//     from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.


/// @file WheelFactorization.h
/// @brief Wheel factorization is used to skip multiples of small
/// primes to speed up the sieve of Eratosthenes.
/// http://en.wikipedia.org/wiki/Wheel_factorization

#ifndef WHEELFACTORIZATION_H
#define WHEELFACTORIZATION_H

#include "PrimeSieve.h"
#include "toString.h"
#include "imath.h"
#include "config.h"

#include <stdint.h>
#include <limits>
#include <cassert>

namespace soe {

struct WheelInit {
  uint8_t nextMultipleFactor;
  uint8_t wheelIndex;
};

/// The WheelElement data structure holds the information needed to
/// unset the bit within the SieveOfEratosthenes array corresponding to
/// the current multiple of a sieving prime.
/// @see WheelFactorization.cpp
///
struct WheelElement {
  WheelElement(uint8_t unsetBit_,
               uint8_t nextMultipleFactor_,
               uint8_t correct_,
                int8_t next_) :
    unsetBit(unsetBit_),
    nextMultipleFactor(nextMultipleFactor_),
    correct(correct_),
    next(next_)
  { }
  /// Bitmask used with the bitwise & operator to unset the bit
  /// corresponding to the current multiple of a WheelPrime object.
  uint8_t unsetBit;
  /// Factor used to calculate the next multiple of a sieving prime
  /// that is not divisible by any of the wheel factors.
  uint8_t nextMultipleFactor;
  /// Overflow needed to correct the next multiple index,
  /// (due to sievingPrime = prime / 30).
  uint8_t correct;
  /// Used to calculate the next wheel index:
  /// wheelIndex += next;
   int8_t next;
};

/// @see WheelFactorization.cpp
extern const WheelInit wheel30Init[30];
extern const WheelInit wheel210Init[210];
extern const WheelElement wheel30[8*8];
extern const WheelElement wheel210[48*8];


/// WheelPrime objects are sieving primes <= sqrt(n) that are used to
/// cross-off multiples and that skip multiples of small primes e.g.
/// <= 7 using wheel factorization. Each WheelPrime contains a sieving
/// prime, the position of the next multiple within the sieve of
/// Eratosthenes array (multipleIndex) and a wheelIndex.
///
class WheelPrime {
public:
  static uint_t getMaxSieveSize()
  {
    // max(multipleIndex) = 2^23-1 (see indexes_),
    // max(multipleIndex) % sieveSize <= 2^23-1
    return (1u << 23);
  }
  /// multipleIndex and wheelIndex are compressed into the
  /// same 32-bit indexes_ variable.
  void set(uint_t multipleIndex, uint_t wheelIndex)
  {
    assert(multipleIndex < (1u << 23));
    assert(wheelIndex    < (1u << 9));
    indexes_ = static_cast<uint32_t>(multipleIndex | (wheelIndex << 23));
  }
  void set(uint_t sievingPrime,
           uint_t multipleIndex,
           uint_t wheelIndex)
  {
    set(multipleIndex, wheelIndex);
    sievingPrime_ = static_cast<uint32_t>(sievingPrime);
  }
  void setMultipleIndex(uint_t multipleIndex)
  {
    assert(multipleIndex < (1u << 23));
    indexes_ = static_cast<uint32_t>(indexes_ | multipleIndex);
  }
  void setWheelIndex(uint_t wheelIndex)
  {
    assert(wheelIndex < (1u << 9));
    indexes_ = static_cast<uint32_t>(wheelIndex << 23);
  }
  uint_t getSievingPrime() const  { return sievingPrime_; }
  uint_t getMultipleIndex() const { return indexes_ & ((1 << 23) - 1); }
  uint_t getWheelIndex() const    { return indexes_ >> 23; }
private:
  /// multipleIndex = 23 least significant bits of indexes_.
  /// wheelIndex    =  9 most  significant bits of indexes_.
  uint32_t indexes_;
  /// sievingPrime_ = prime / 30;
  /// '/ 30' is used as SieveOfEratosthenes objects use a bit array
  /// with 30 numbers per byte for sieving.
  uint32_t sievingPrime_;
};


/// The Bucket data structure is used to store sieving primes. It is
/// designed as a singly linked list, once there is no more space in
/// the current bucket a new bucket node is allocated.
/// @see http://www.ieeta.pt/~tos/software/prime_sieve.html
///
class Bucket {
public:
  /// list.push_back( Bucket() ) adds an empty bucket without copying
  Bucket(const Bucket&) : current_(wheelPrimes_) { }
  Bucket() :              current_(wheelPrimes_) { }
  WheelPrime* begin() { return &wheelPrimes_[0]; }
  WheelPrime* last()  { return &wheelPrimes_[config::BUCKETSIZE - 1]; }
  WheelPrime* end()   { return current_;}
  Bucket* next()      { return next_; }
  bool hasNext()      { return next_ != NULL; }
  bool empty()        { return begin() == end(); }
  void reset()        { current_ = begin(); }
  void setNext(Bucket* next)
  {
    next_ = next;
  }
  /// Store a WheelPrime in the bucket.
  /// @return true  if the bucket is not full else false.
  bool store(uint_t sievingPrime,
             uint_t multipleIndex,
             uint_t wheelIndex)
  {
    WheelPrime* wPrime = current_;
    current_++;
    wPrime->set(sievingPrime, multipleIndex, wheelIndex);
    return wPrime != last();
  }
private:
  WheelPrime* current_;
  Bucket* next_;
  WheelPrime wheelPrimes_[config::BUCKETSIZE];
};


/// The abstract WheelFactorization is used skip multiples of small
/// primes in the sieve of Eratosthenes. The EratSmall, EratMedium and
/// EratBig classes are derived from WheelFactorization.
///
template <uint_t MODULO, uint_t SIZE, const WheelInit* INIT, const WheelElement* WHEEL>
class WheelFactorization {
public:
  /// stop_ must be <= getMaxStop() in order to prevent
  /// 64-bit integer overflows in add().
  ///
  static uint64_t getMaxStop()
  {
    uint64_t maxPrime = std::numeric_limits<uint32_t>::max();
    return              std::numeric_limits<uint64_t>::max() - maxPrime * getMaxFactor();
  }
  /// Get the maximum wheel factor.
  static uint_t getMaxFactor()
  {
    return WHEEL[0].nextMultipleFactor;
  }
  /// Calculate the first multiple > segmentLow of prime that is not
  /// divisible by any of the wheel's factors (e.g. not a multiple of
  /// 2, 3 and 5 for a modulo 30 wheel) and the position within the
  /// SieveOfEratosthenes array of that multiple (multipleIndex) and
  /// its wheel index. When done store the sieving prime.
  /// @see sieve() in SieveOfEratosthenes-inline.h
  ///
  void add(uint_t prime, uint64_t segmentLow)
  {
    segmentLow += 6;
    // calculate the first multiple > segmentLow
    uint64_t quotient = segmentLow / prime + 1;
    uint64_t multiple = prime * quotient;
    // prime is not needed for sieving
    if (multiple > stop_)
      return;
    uint64_t square = isquare<uint64_t>(prime);
    // prime^2 is the first multiple that must be crossed-off
    if (multiple < square) {
      multiple = square;
      quotient = prime;
    }
    // calculate the next multiple of prime that is not
    // divisible by any of the wheel's factors
    multiple += static_cast<uint64_t>(prime) * INIT[quotient % MODULO].nextMultipleFactor;
    if (multiple > stop_)
      return;
    uint_t multipleIndex = static_cast<uint_t>((multiple - segmentLow) / 30);
    uint_t wheelIndex = wheelOffsets_[prime % 30] + INIT[quotient % MODULO].wheelIndex;
    // @see Erat(Small|Medium|Big).cpp
    store(prime, multipleIndex, wheelIndex);
  }
protected:
  /// @param stop       Upper bound for sieving.
  /// @param sieveSize  Sieve size in bytes.
  ///
  WheelFactorization(uint64_t stop, uint_t sieveSize) :
    stop_(stop)
  {
    uint_t maxSieveSize = WheelPrime::getMaxSieveSize();
    if (sieveSize > maxSieveSize)
      throw primesieve_error("WheelFactorization: sieveSize must be <= " + toString( maxSieveSize ));
    if (stop > getMaxStop())
      throw primesieve_error("WheelFactorization: stop must be <= 2^64 - 2^32 * " + toString( getMaxFactor() ));
  }
  virtual ~WheelFactorization() { }
  virtual void store(uint_t, uint_t, uint_t) = 0;
  /// Cross-off the current multiple (unset bit) of sievingPrime
  /// and calculate its next multiple.
  ///
  static void unsetBit(uint8_t* sieve, uint_t sievingPrime, uint_t* multipleIndex, uint_t* wheelIndex)
  {
    sieve[*multipleIndex] &= WHEEL[*wheelIndex].unsetBit;
    *multipleIndex        += WHEEL[*wheelIndex].nextMultipleFactor * sievingPrime;
    *multipleIndex        += WHEEL[*wheelIndex].correct;
    *wheelIndex           += WHEEL[*wheelIndex].next;
  }
private:
  static const uint_t wheelOffsets_[30];
  const uint64_t stop_;
  WheelFactorization(const WheelFactorization&);
  WheelFactorization& operator=(const WheelFactorization&);
};

/// The wheelOffsets_ array is used to calculate the WHEEL index
/// corresponding to the first multiple >= segmentLow.
///
template <uint_t MODULO, uint_t SIZE, const WheelInit* INIT, const WheelElement* WHEEL>
const uint_t
WheelFactorization<MODULO, SIZE, INIT, WHEEL>::wheelOffsets_[30] =
{
  0xFF, 7 * SIZE, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0 * SIZE, 0xFF, 0xFF, 0xFF, 1 * SIZE,
  0xFF, 2 * SIZE, 0xFF, 0xFF, 0xFF, 3 * SIZE,
  0xFF, 4 * SIZE, 0xFF, 0xFF, 0xFF, 5 * SIZE,
  0xFF, 0xFF,     0xFF, 0xFF, 0xFF, 6 * SIZE
};

/// 3rd wheel, skips multiples of 2, 3 and 5
typedef WheelFactorization<30, 8, wheel30Init, wheel30> Modulo30Wheel_t;
/// 4th wheel, skips multiples of 2, 3, 5 and 7
typedef WheelFactorization<210, 48, wheel210Init, wheel210> Modulo210Wheel_t;

} // namespace soe

#endif
