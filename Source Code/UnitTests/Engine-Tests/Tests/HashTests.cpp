#include "stdafx.h"

#include "Headers/Defines.h"
#include "Platform/Headers/PlatformDefines.h"
#include "Core/Math/Headers/MathHelper.h"
#include "Core/Resources/Headers/ResourceDescriptor.h"

namespace Divide {

TEST(HashCombineStr)
{
    const stringImpl inputA = "bla1";
    const stringImpl inputB = "bla1";

    size_t seed1 = 0, seed2 = 0;
    Util::Hash_combine(seed1, inputA);
    Util::Hash_combine(seed2, inputB);

    CHECK_EQUAL(seed1, seed2);
}


TEST(HashCombineResourceDescriptors)
{
    ResourceDescriptor inputA("testDescriptor");
    inputA.flag(true);

    ResourceDescriptor inputB("testDescriptor");
    inputB.flag(true);

    size_t result1 = inputA.getHash();
    size_t result2 = inputB.getHash();

    CHECK_EQUAL(result1, result2);

    P32 testMask;
    testMask.i = 0;
    testMask.b[2] = true;
    inputB.mask(testMask);
    result2 = inputB.getHash();

    CHECK_NOT_EQUAL(result1, result2);
}

}; //namespace Divide