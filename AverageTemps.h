#include "Sensor.h"
#include <algorithm>

template<typename InputIt, typename MemberType>
float averageGreaterThanZero(InputIt&& begin, InputIt&& end, MemberType Sensor::*var) {
  using value_type = typename iterator_traits<InputIt>::value_type;

  unsigned int count = 0;
  float total = 0;

  for_each(forward<InputIt>(begin), forward<InputIt>(end), [var, &count, &total](const value_type& x) {
    if(x.*var > 0) {
      total += x.*var;
      ++count;
    }
  });

  return count == 0 ? 0 : total / count;
}

template<typename InputIt>
float averageGreaterThanZero(InputIt&& begin, InputIt&& end) {
  using value_type = typename iterator_traits<InputIt>::value_type;

  unsigned int count = 0;
  float total = 0;

  for_each(forward<InputIt>(begin), forward<InputIt>(end), [&count, &total](const value_type& x) {
    if(x > 0) {
      total += x;
      ++count;
    }
  });

  return count == 0 ? 0 : total / count;
}

