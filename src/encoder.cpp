/*
 * @Author: ilikara 3435193369@qq.com
 * @Date: 2024-10-11 06:19:57
 * @LastEditors: ilikara 3435193369@qq.com
 * @LastEditTime: 2024-12-01 03:54:06
 * @FilePath: /smartcar/src/encoder.cpp
 * @Description:
 *
 * Copyright (c) 2024 by ilikara 3435193369@qq.com, All Rights Reserved.
 */
#include "encoder.h"

ENCODER::ENCODER(int pwmNum, int gpioNum) : base_addr(PWM_BASE_ADDR + pwmNum * PWM_OFFSET), directionGPIO(gpioNum)
{
    directionGPIO.setDirection("in");

    control_buffer = map_register(base_addr + CONTROL_REG_OFFSET, PAGE_SIZE);
    low_buffer = map_register(base_addr + LOW_BUFFER_OFFSET, PAGE_SIZE);
    full_buffer = map_register(base_addr + FULL_BUFFER_OFFSET, PAGE_SIZE);

    printf("Registers mapped successfully\n");

    PWM_Init();
}

ENCODER::~ENCODER()
{
    munmap(control_buffer, PAGE_SIZE);
    munmap(low_buffer, PAGE_SIZE);
    munmap(full_buffer, PAGE_SIZE);
}

void *ENCODER::map_register(uint32_t physical_address, size_t size)
{
    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd == -1)
    {
        perror("Failed to open /dev/mem");
        exit(EXIT_FAILURE);
    }

    void *mapped_addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, physical_address & ~(PAGE_SIZE - 1));
    if (mapped_addr == MAP_FAILED)
    {
        perror("Failed to map memory");
        close(mem_fd);
        exit(EXIT_FAILURE);
    }

    close(mem_fd);

    return (void *)((uintptr_t)mapped_addr + (physical_address & (PAGE_SIZE - 1)));
}

void ENCODER::PWM_Init(void)
{
    uint32_t control_reg = 0;

    control_reg |= CNTR_ENABLE_BIT;
    control_reg |= MEASURE_PULSE_BIT;
    // control_reg |= INT_ENABLE_BIT;  // 注释掉中断使能，避免中断超时错误

    REG_WRITE(control_buffer, control_reg);

    printf("PWM initialized with control register: 0x%08X\n", control_reg);
}

void ENCODER::reset_counter(void)
{
    uint32_t control_reg = REG_READ(control_buffer);
    control_reg |= COUNTER_RESET_BIT;
    REG_WRITE(control_buffer, control_reg);
}

double ENCODER::pulse_counter_update(void)
{
    uint32_t full_count = REG_READ(full_buffer);
    
    // 防止除以零
    if (full_count == 0)
    {
        return 0.0;
    }
    
    double value = 100000000.0 / full_count / 1024.0 * (directionGPIO.readValue() * 2 - 1);
    return value;
}