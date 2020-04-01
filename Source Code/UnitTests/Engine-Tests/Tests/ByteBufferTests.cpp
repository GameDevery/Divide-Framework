#include "stdafx.h"

#include "Headers/Defines.h"
#include "Core/Headers/ByteBuffer.h"
#include "Platform/Headers/PlatformDefines.h"

namespace Divide{

template<typename T>
bool compareVectors(const vectorSTD<T>& a, const vectorSTD<T>& b) {
    if (a.size() == b.size()) {
        for (size_t i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }

    return false;
}

template<typename T, size_t N>
bool compareArrays(const std::array<T, N>& a, const std::array<T, N>& b) {
    for (size_t i = 0; i < N; ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}

TEST(ByteBufferRWBool)
{
    const bool input = false;

    ByteBuffer test;
    test << input;

    bool output = true;
    test >> output;
    CHECK_EQUAL(output, input);
}

TEST(ByteBufferRWPOD)
{
    const U8  inputU8 = 2;
    const U16 inputU16 = 4;
    const U32 inputU32 = 6;
    const I8  inputI8 = -2;
    const I16 inputI16 = 40;
    const I32 inputI32 = -6;
    const F32 inputF32 = 3.45632f;
    const D64 inputD64 = 1.14159;

    ByteBuffer test;
    test << inputU8;
    test << inputU16;
    test << inputU32;
    test << inputI8;
    test << inputI16;
    test << inputI32;
    test << inputF32;
    test << inputD64;

    U8  outputU8 = 0;
    U16 outputU16 = 0;
    U32 outputU32 = 0;
    I8  outputI8 = 0;
    I16 outputI16 = 0;
    I32 outputI32 = 0;
    F32 outputF32 = 0.0f;
    D64 outputD64 = 0.0;

    test >> outputU8;
    test >> outputU16;
    test >> outputU32;
    test >> outputI8;
    test >> outputI16;
    test >> outputI32;
    test >> outputF32;
    test >> outputD64;

    CHECK_EQUAL(outputU8,  inputU8);
    CHECK_EQUAL(outputU16, inputU16);
    CHECK_EQUAL(outputU32, inputU32);
    CHECK_EQUAL(outputI8,  inputI8);
    CHECK_EQUAL(outputI16, inputI16);
    CHECK_EQUAL(outputI32, inputI32);
    CHECK_TRUE(COMPARE(outputF32, inputF32));
    CHECK_TRUE(COMPARE(outputD64, inputD64));
}


TEST(ByteBufferRWString)
{
    const stringImpl input = "StringTest Whatever";

    ByteBuffer test;
    test << input;

    stringImpl output = "Output";

    test >> output;

    CHECK_EQUAL(input, output);
}


TEST(ByteBufferRWVectorInt)
{
    const vectorSTD<I32> input = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };

    ByteBuffer test;
    test << input;

    vectorSTD<I32> output;

    test >> output;

    CHECK_TRUE(compareVectors(input, output));
}


TEST(ByteBufferRWVectorString)
{
    const vectorSTD<stringImpl> input = { "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5" };

    ByteBuffer test;
    test << input;

    vectorSTD<stringImpl> output;

    test >> output;

    CHECK_TRUE(compareVectors(input, output));
}

TEST(ByteBufferRWArrayInt)
{
    const std::array<I32, 11> input = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };

    ByteBuffer test;
    test << input;

    std::array<I32, 11> output;

    test >> output;

    CHECK_TRUE(compareArrays(input, output));
}


TEST(ByteBufferRWArrayString)
{
    const std::array<stringImpl, 11> input = { "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5" };

    ByteBuffer test;
    test << input;

    std::array<stringImpl, 11> output;

    test >> output;

    CHECK_TRUE(compareArrays(input, output));
}


TEST(ByteBufferRWMixedData)
{
    const bool inputBool = false;
    const vectorSTD<I32> inputVectorInt = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
    const vectorSTD<stringImpl> inputVectorStr = { "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5" };
    const std::array<I32, 11> inputArrayInt = { -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5 };
    const std::array<stringImpl, 11> inputArrayStr = { "-5", "-4", "-3", "-2", "-1", "0", "1", "2", "3", "4", "5" };
    const U8  inputU8 = 2;
    const U16 inputU16 = 4;
    const U32 inputU32 = 6;
    const I8  inputI8 = -2;
    const I16 inputI16 = 40;
    const I32 inputI32 = -6;
    const F32 inputF32 = 3.45632f;
    const stringImpl inputStr = "StringTest Whatever";
    const D64 inputD64 = 1.14159;

    bool outputBool = true;
    vectorSTD<I32> outputVectorInt;
    vectorSTD<stringImpl> outputVectorStr;
    std::array<I32, 11> outputArrayInt;
    std::array<stringImpl, 11> outputArrayStr;
    U8  outputU8 = 0;
    U16 outputU16 = 0;
    U32 outputU32 = 0;
    I8  outputI8 = 0;
    I16 outputI16 = 0;
    I32 outputI32 = 0;
    F32 outputF32 = 0.0f;
    stringImpl outputStr = "Output";
    D64 outputD64 = 0.0;

    ByteBuffer test;
    test << inputBool;
    test << inputVectorInt;
    test << inputVectorStr;
    test << inputArrayInt;
    test << inputArrayStr;
    test << inputU8;
    test << inputU16;
    test << inputU32;
    test << inputI8;
    test << inputI16;
    test << inputI32;
    test << inputF32;
    test << inputStr;
    test << inputD64;


    test >> outputBool;
    test >> outputVectorInt;
    test >> outputVectorStr;
    test >> outputArrayInt;
    test >> outputArrayStr;
    test >> outputU8;
    test >> outputU16;
    test >> outputU32;
    test >> outputI8;
    test >> outputI16;
    test >> outputI32;
    test >> outputF32;
    test >> outputStr;
    test >> outputD64;

    CHECK_EQUAL(inputBool, outputBool);
    CHECK_TRUE(compareVectors(inputVectorInt, outputVectorInt));
    CHECK_TRUE(compareVectors(inputVectorStr, outputVectorStr));
    CHECK_TRUE(compareArrays(inputArrayInt, outputArrayInt));
    CHECK_TRUE(compareArrays(inputArrayStr, outputArrayStr));
    CHECK_EQUAL(outputU8, inputU8);
    CHECK_EQUAL(outputU16, inputU16);
    CHECK_EQUAL(outputU32, inputU32);
    CHECK_EQUAL(outputI8, inputI8);
    CHECK_EQUAL(outputI16, inputI16);
    CHECK_EQUAL(outputI32, inputI32);
    CHECK_TRUE(COMPARE(outputF32, inputF32));
    CHECK_EQUAL(inputStr, outputStr);
    CHECK_TRUE(COMPARE(outputD64, inputD64));

}

};//namespace Divide