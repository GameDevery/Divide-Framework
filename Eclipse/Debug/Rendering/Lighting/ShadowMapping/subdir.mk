################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
/home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/CascadedShadowMaps.cpp \
/home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/CubeShadowMap.cpp \
/home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/ShadowMap.cpp \
/home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/SingleShadowMap.cpp 

OBJS += \
./Rendering/Lighting/ShadowMapping/CascadedShadowMaps.o \
./Rendering/Lighting/ShadowMapping/CubeShadowMap.o \
./Rendering/Lighting/ShadowMapping/ShadowMap.o \
./Rendering/Lighting/ShadowMapping/SingleShadowMap.o 

CPP_DEPS += \
./Rendering/Lighting/ShadowMapping/CascadedShadowMaps.d \
./Rendering/Lighting/ShadowMapping/CubeShadowMap.d \
./Rendering/Lighting/ShadowMapping/ShadowMap.d \
./Rendering/Lighting/ShadowMapping/SingleShadowMap.d 


# Each subdirectory must supply rules for building sources it contributes
Rendering/Lighting/ShadowMapping/CascadedShadowMaps.o: /home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/CascadedShadowMaps.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D_DEBUG -I"/home/ionut/Divide/Source Code" -I/home/ionut/DivideLibraries/physx/Include -I/home/ionut/DivideLibraries/OIS/include -I/home/ionut/DivideLibraries/Threadpool/include -I/home/ionut/DivideLibraries/SimpleINI/include -I/home/ionut/DivideLibraries/glbinding/include -I/home/ionut/DivideLibraries/cegui-deps/include -I/home/ionut/DivideLibraries/boost -O0 -g3 -Wall -c -fmessage-length=0 -std=c++1y -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

Rendering/Lighting/ShadowMapping/CubeShadowMap.o: /home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/CubeShadowMap.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D_DEBUG -I"/home/ionut/Divide/Source Code" -I/home/ionut/DivideLibraries/physx/Include -I/home/ionut/DivideLibraries/OIS/include -I/home/ionut/DivideLibraries/Threadpool/include -I/home/ionut/DivideLibraries/SimpleINI/include -I/home/ionut/DivideLibraries/glbinding/include -I/home/ionut/DivideLibraries/cegui-deps/include -I/home/ionut/DivideLibraries/boost -O0 -g3 -Wall -c -fmessage-length=0 -std=c++1y -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

Rendering/Lighting/ShadowMapping/ShadowMap.o: /home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/ShadowMap.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D_DEBUG -I"/home/ionut/Divide/Source Code" -I/home/ionut/DivideLibraries/physx/Include -I/home/ionut/DivideLibraries/OIS/include -I/home/ionut/DivideLibraries/Threadpool/include -I/home/ionut/DivideLibraries/SimpleINI/include -I/home/ionut/DivideLibraries/glbinding/include -I/home/ionut/DivideLibraries/cegui-deps/include -I/home/ionut/DivideLibraries/boost -O0 -g3 -Wall -c -fmessage-length=0 -std=c++1y -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

Rendering/Lighting/ShadowMapping/SingleShadowMap.o: /home/ionut/Divide/Source\ Code/Rendering/Lighting/ShadowMapping/SingleShadowMap.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D_DEBUG -I"/home/ionut/Divide/Source Code" -I/home/ionut/DivideLibraries/physx/Include -I/home/ionut/DivideLibraries/OIS/include -I/home/ionut/DivideLibraries/Threadpool/include -I/home/ionut/DivideLibraries/SimpleINI/include -I/home/ionut/DivideLibraries/glbinding/include -I/home/ionut/DivideLibraries/cegui-deps/include -I/home/ionut/DivideLibraries/boost -O0 -g3 -Wall -c -fmessage-length=0 -std=c++1y -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


