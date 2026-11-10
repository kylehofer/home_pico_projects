#include "DependencyExecutor.h"
#include <memory.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

// Assumes little endian
void printBits(size_t const size, void const *const ptr)
{
    unsigned char *b = (unsigned char *)ptr;
    unsigned char byte;
    int i, j;

    for (i = size - 1; i >= 0; i--)
    {
        for (j = 0; j < 8; j++)
        {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

inline size_t resetMask(uint8_t *&mask, size_t size, bool initialValue = true)
{
    size_t bytes = (size / 8);
    size_t bits = (size % 8);

    memset(mask, 0, bytes + 1);
    if (initialValue)
    {
        memset(mask, 0xFF, bytes);
        memset(&mask[bytes], 0xFF << (8 - bits), 1);
    }

    return bytes + 1;
}

/**
 * @brief Create a Bit Mask object
 *
 * @param mask
 * @param size
 * @return size_t
 */
inline size_t createBitMask(uint8_t *&mask, size_t size)
{
    size_t bytes = (size / 8);
    size_t bits = (size % 8);

    size_t total = bytes + (bits > 0 ? 1 : 1);

    mask = (uint8_t *)malloc(total);

    memset(mask, 0, total);

    return total;
}

/**
 * @brief Get the Bit object
 *
 * @param mask
 * @param input
 * @return true
 * @return false
 */
inline bool getBit(uint8_t *mask, size_t input)
{
    size_t byte = (input / 8);
    uint8_t bit = (input % 8);

    bit = 1 << (7 - bit);

    return mask[byte] & bit;
}

/**
 * @brief
 *
 * @param mask
 * @param input
 */
inline void flagBit(uint8_t *mask, size_t input)
{
    size_t byte = (input / 8);
    uint8_t bit = (input % 8);

    bit = 1 << (7 - bit);

    uint8_t value = mask[byte];

    mask[byte] = value | bit;
}

inline void clearBit(uint8_t *mask, size_t input)
{
    size_t byte = (input / 8);
    uint8_t bit = (input % 8);

    bit = 1 << (7 - bit);

    uint8_t value = mask[byte];

    mask[byte] = value & ~bit;
}

inline void clearBits(uint8_t *mask, size_t start, size_t length)
{
    for (size_t i = start; i <= (length + start); i++)
    {
        flagBit(mask, i);
    }
}

inline void flagBits(uint8_t *mask, size_t start, size_t length)
{
    for (size_t i = start; i <= (length + start); i++)
    {
        flagBit(mask, i);
    }
}

DependencyExecutor::DependencyExecutor()
{
}

DependencyExecutor::~DependencyExecutor()
{
}

DependencyToken DependencyExecutor::add(DependencyFunction function)
{
    return this->add(0, function);
}

DependencyToken DependencyExecutor::add(DependencyToken dependent, DependencyFunction function)
{
    if (dependent != 0 && executors.count(dependent) <= 0)
    {
        return -1;
    }

    executors[++next] = std::move(DependencyEntry{
        .function = function,
        .dependent = dependent});

    return next;
}

DependencyToken DependencyExecutor::addChain(const std::vector<DependencyFunction> &functions)
{
    return addChain(0, functions);
}

DependencyToken DependencyExecutor::addChain(DependencyToken dependant, const std::vector<DependencyFunction> &functions)
{
    for (auto &function : functions)
    {
        dependant = add(dependant, function);
    }
    return dependant;
}

bool DependencyExecutor::execute()
{
    size_t size = executors.size();

    size_t mask = 0;
    uint8_t *mask_ptr = (uint8_t *)&mask;
    // createBitMask(mask, size);

    flagBit(mask_ptr, 0);

    bool success = true;

    for (const auto &[key, value] : executors)
    {
        if (getBit(mask_ptr, value.dependent) && value.function())
        {
            flagBit(mask_ptr, key);
        }
        else
        {
            success = false;
        }
    }

    return success;
}
