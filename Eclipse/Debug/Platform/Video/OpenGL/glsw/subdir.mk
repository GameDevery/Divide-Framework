################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
/home/ionut/Divide/Source\ Code/Platform/Video/OpenGL/glsw/bstrlib.c \
/home/ionut/Divide/Source\ Code/Platform/Video/OpenGL/glsw/glsw.c 

OBJS += \
./Platform/Video/OpenGL/glsw/bstrlib.o \
./Platform/Video/OpenGL/glsw/glsw.o 

C_DEPS += \
./Platform/Video/OpenGL/glsw/bstrlib.d \
./Platform/Video/OpenGL/glsw/glsw.d 


# Each subdirectory must supply rules for building sources it contributes
Platform/Video/OpenGL/glsw/bstrlib.o: /home/ionut/Divide/Source\ Code/Platform/Video/OpenGL/glsw/bstrlib.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

Platform/Video/OpenGL/glsw/glsw.o: /home/ionut/Divide/Source\ Code/Platform/Video/OpenGL/glsw/glsw.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


