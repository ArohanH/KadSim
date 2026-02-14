#ifndef ASSERT_H
#define ASSERT_H

void custom_assert(bool, char const*, char const*, int, char const*);

#ifndef assert
    #define assert(X) custom_assert(X, #X, __func__, __LINE__, __FILE__)
#endif

#endif // ASSERT_H
