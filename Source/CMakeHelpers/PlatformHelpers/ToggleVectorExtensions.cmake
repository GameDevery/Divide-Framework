set(AVX512_OPT OFF)
set(AVX2_OPT OFF)
set(AVX_OPT OFF)
set(SSE41_OPT OFF)
set(SSE42_OPT OFF)
set(FMA4_OPT OFF)
set(BMI2_OPT OFF)
set(F16C_OPT OFF)
set(EXTENSIONS "")

function (Toggle_Available_Vector_Extensions)

    AutodetectHostArchitecture()
    OptimizeForArchitecture()

    foreach(flag ${_enable_vector_unit_list})
        string(TOUPPER "${flag}" flag)
        string(REPLACE "\." "_" flag "${flag}")
        string(REPLACE "_" "" flag "${flag}")

        if("${flag}" MATCHES "AVX512F")
        set(AVX512_OPT ON)
        endif()

        if("${flag}" MATCHES "AVX2")
        set(AVX2_OPT ON)
        endif()

        if("${flag}" MATCHES "AVX")
            set(AVX_OPT ON)
        endif()

        if("${flag}" MATCHES "SSE42")
            set(SSE42_OPT ON)
        endif()

        if("${flag}" MATCHES "SSE41")
            set(SSE41_OPT ON)
        endif()

        if("${flag}" MATCHES "fma4")
        set(FMA4_OPT ON)
        endif()

        if("${flag}" MATCHES "bmi2")
        set(BMI2_OPT ON)
        endif()

        if("${flag}" MATCHES "f16c")
        set(F16C_OPT ON)
        endif()

    endforeach(flag)

    if (AVX512_OPT)
        set(EXTENSIONS ${EXTENSIONS} " AVX512 ")
        add_compile_definitions(HAS_AVX512)
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-AVX512 ")
    endif()

    if (AVX2_OPT)
        set(EXTENSIONS ${EXTENSIONS} " AVX2 ")
        add_compile_definitions(HAS_AVX2)
        if (MSVC_COMPILER)
            add_compile_options(/arch:AVX2)
        else()
            add_compile_options(-mavx2)
        endif()
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-AVX2 ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mno-avx2)
        endif()
    endif()

    if (AVX_OPT)
        set(EXTENSIONS ${EXTENSIONS} " AVX ")
        add_compile_definitions(HAS_AVX)
        if (MSVC_COMPILER)
            add_compile_options(/arch:AVX)
        else()
            add_compile_options(-mavx)
        endif()
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-AVX ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mno-avx)
        endif()
    endif()

    if (SSE42_OPT)
        set(EXTENSIONS ${EXTENSIONS} " SSE4.2 ")
        add_compile_definitions(HAS_SSE42)
        if (MSVC_COMPILER)
            add_compile_options(/d2archSSE42)
        else()
            add_compile_options(-msse4.2)
        endif()
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-SSE4.2 ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mno-sse4.2)
        endif()
    endif()

    if (SSE41_OPT)
        set(EXTENSIONS ${EXTENSIONS} " SSE4.1 ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-msse)
        endif()
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-SSE4.1 ")
        message(FATAL_ERROR "SSE4.1 was not detected. SSE4.1 is a minimum requirment in order for the build to proceed!")
    endif()

    if(FMA4_OPT)
        set(EXTENSIONS ${EXTENSIONS} " FMA4 ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mfma)
        endif()
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-FMA4 ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mno-fma)
        endif()
    endif()

    if(BMI2_OPT)
        set(EXTENSIONS ${EXTENSIONS} " BMI2 ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mbmi -mlzcnt)
        endif()
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-BMI2 ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mno-bmi -mno-lzcnt)
        endif()
    endif()

    if(F16C_OPT)
        set(EXTENSIONS ${EXTENSIONS} " F16C ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mf16c)
        endif()
    else()
        set(EXTENSIONS ${EXTENSIONS} " NO-F16C ")
        if (NOT MSVC_COMPILER)
            add_compile_options(-mno-f16c)
        endif()
    endif()

    message("Available extensions: " ${EXTENSIONS})

endfunction()
