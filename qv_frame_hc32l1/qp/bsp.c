//============================================================================
// BSP for "real-time" Example
//
// Copyright (C) 2005 Quantum Leaps, LLC. All rights reserved.
//
//                    Q u a n t u m  L e a P s
//                    ------------------------
//                    Modern Embedded Software
//
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-QL-commercial
//
// This software is dual-licensed under the terms of the open-source GNU
// General Public License (GPL) or under the terms of one of the closed-
// source Quantum Leaps commercial licenses.
//
// Redistributions in source code must retain this top-level comment block.
// Plagiarizing this software to sidestep the license obligations is illegal.
//
// NOTE:
// The GPL does NOT permit the incorporation of this code into proprietary
// programs. Please contact Quantum Leaps for commercial licensing options,
// which expressly supersede the GPL and are designed explicitly for
// closed-source distribution.
//
// Quantum Leaps contact information:
// <www.state-machine.com/licensing>
// <info@state-machine.com>
//============================================================================
#include "qpc.h"                 // QP/C real-time event framework
#include "bsp.h"                 // Board Support Package
#include "hc32l19x.h"
#include "qm.h"
#include "main.h"
// add other drivers if necessary...

Q_DEFINE_THIS_FILE  // define the name of this file for assertions
typedef enum
{
	TASK_HIGH,
	TASK_MID,
	TASK_LOW,
}TaskPrio;
#define SYSTEM_CLOCK_HZ         (48000000)
// Local-scope defines -----------------------------------------------------


// Button pins available on the board (just one user Button B1 on PC.13)
// button on GPIO PC (input)

//============================================================================
// Error handler and ISRs...

Q_NORETURN Q_onError(char const * const module, int_t const id) {
    // NOTE: this implementation of the error handler is intended only
    // for debugging and MUST be changed for deployment of the application
    // (assuming that you ship your production code with assertions enabled).
    Q_UNUSED_PAR(module);
    Q_UNUSED_PAR(id);
    QS_ASSERTION(module, id, 10000U); // report assertion to QS

#ifndef NDEBUG
    // light up the user LED

#endif

    NVIC_SystemReset();
}
//............................................................................
// assertion failure handler for the STM32 library, including the startup code
 // prototype
void assert_failed(char const * const module, int const id) {
    Q_onError(module, id);
}

//............................................................................
#ifdef __UVISION_VERSION
// dummy initialization of the ctors (not used in C)
void _init(void);
void _init(void) {
}
#endif // __UVISION_VERSION
static volatile uint32_t BSP_SysTick = 0;
static void BSP_SysTick_increase(void)
{
	++BSP_SysTick;
}

uint32_t BSP_SysTick_get(void)
{
	return BSP_SysTick;
}
void system_delay_ms(__IO uint32_t Delay)
{
  volatile uint32_t tickstart = 0;
  tickstart = BSP_SysTick_get();
  while((BSP_SysTick_get() - tickstart) < Delay)
  {
  }
}

void system_delay_us(__IO uint32_t us)
{
    uint32_t start_val = SysTick->VAL;  // 获取当前值
    uint32_t ticks = us * (SYSTEM_CLOCK_HZ / 1000000UL);  // 计算需要的 ticks
    
    while (1) {
        uint32_t current_val = SysTick->VAL;
        // 检查是否递减足够 ticks（处理计数器溢出）
        if (start_val >= current_val) {
            if ((start_val - current_val) >= ticks) break;
        } else {
            if ((start_val + (0xFFFFFF - current_val)) >= ticks) break;
        }
    }	
}
// 获取两个 tick 值之间的差值，考虑溢出情况
uint32_t BSP_SysTick_diff(uint32_t start_tick, uint32_t end_tick)
{
    if (end_tick >= start_tick) {
        return end_tick - start_tick;  // 没有溢出，正常计算差值
    } else {
        // 发生了溢出，正确计算差值
        return (UINT32_MAX - start_tick) + end_tick + 1;
    }
}
void BSP_SysTick_set(uint32_t tick)
{
	BSP_SysTick = tick;
}
// ISRs used in the application ============================================

void SysTick_IRQHandler(void); // prototype
void SysTick_IRQHandler(void) {

    QTIMEEVT_TICK_X(0U, &l_SysTick_Handler); // time events at rate 0

    // Perform the debouncing of buttons. The algorithm for debouncing
    // adapted from the book "Embedded Systems Dictionary" by Jack Ganssle
    // and Michael Barr, page 71.
	BSP_SysTick_increase();
    QV_ARM_ERRATUM_838869();

}
static PeriodicSpecEvt const periodicSpec[] = {
	{
		QEVT_INITIALIZER(PERIODIC_SPEC_SIG), //high
		.ticks = 1U,
	},
	{
		QEVT_INITIALIZER(PERIODIC_SPEC_SIG),//mid
		.ticks = 1U,
	},	
	{
		QEVT_INITIALIZER(PERIODIC_SPEC_SIG),//low
		.ticks = 1U,
	},	
};
static void BSP_clock_init(void)
{
    stc_sysctrl_pll_cfg_t stcPLLCfg;    

    Sysctrl_SetXTHFreq(SysctrlXthFreq4_8MHz);
    Sysctrl_XTHDriverCfg(SysctrlXtalDriver3);
    Sysctrl_SetXTHStableTime(SysctrlXthStableCycle16384);
    Sysctrl_ClkSourceEnable(SysctrlClkXTH, TRUE);
    stcPLLCfg.enInFreq    = SysctrlPllInFreq6_12MHz;    //XTH 8MHz
    stcPLLCfg.enOutFreq   = SysctrlPllOutFreq36_48MHz;  //PLL ??
    stcPLLCfg.enPllClkSrc = SysctrlPllXthXtal;              //???????RCH
    stcPLLCfg.enPllMul    = SysctrlPllMul6;             //8MHz x 6 = 48MHz
    Sysctrl_SetPLLFreq(&stcPLLCfg); 
    
    ///< ???????HCLK??24M:??FLASH ??????1 cycle(?????1 cycle)
    Flash_WaitCycle(FlashWaitCycle1);    

    ///< ??PLL
    Sysctrl_ClkSourceEnable(SysctrlClkPLL, TRUE);    
    ///< ?????PLL
    Sysctrl_SysClkSwitch(SysctrlClkPLL);  	
}

void App_ClkDivInit(void)
{
    //时钟分频设置
    Sysctrl_SetHCLKDiv(SysctrlHclkDiv1);
    Sysctrl_SetPCLKDiv(SysctrlPclkDiv1);
}
// BSP functions =============================================================
void BSP_init(void) {
    // Configure the MPU to prevent NULL-pointer dereferencing ...
	BSP_clock_init();
	App_ClkDivInit();
	//SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);
	
}
//............................................................................
void BSP_start(void) {
    PeriodHigh_ctor();
    static QEvtPtr periodicHighQSto[10]; // Event queue storage
    QActive_start(
        AO_Period_High,          // AO pointer to start
        1U,                    // QF-prio
        periodicHighQSto,         // storage for the AO's queue
        Q_DIM(periodicHighQSto),  // queue length
        (void *)0, 0U,         // stack storage, size (not used)
        &periodicSpec[TASK_HIGH]); // initialization param	
    PeriodMid_ctor();
    static QEvtPtr periodicMidQSto[10]; // Event queue storage
	QActive_start(
        AO_Periodic_Mid,          // AO pointer to start
        2U,                    // QF-prio
        periodicMidQSto,         // storage for the AO's queue
        Q_DIM(periodicMidQSto),  // queue length
        (void *)0, 0U,         // stack storage, size (not used)
        &periodicSpec[TASK_MID]); // initialization param	
    static QEvtPtr periodicLowQSto[10]; // Event queue storage
	QActive_start(
        AO_Periodic_Low,          // AO pointer to start
        3U,                    // QF-prio
        periodicLowQSto,         // storage for the AO's queue
        Q_DIM(periodicLowQSto),  // queue length
        (void *)0, 0U,         // stack storage, size (not used)
        &periodicSpec[TASK_LOW]); // initialization param			
}
//............................................................................


//............................................................................

// QF callbacks ==============================================================
void QF_onStartup(void) {
    SystemCoreClockUpdate();

    // set up the SysTick timer to fire at BSP_TICKS_PER_SEC rate
    SysTick_Config((SystemCoreClock / BSP_TICKS_PER_SEC) + 1U);

    // set priorities of ISRs used in the system
    // NOTE: all interrupts are "kernel aware" in Cortex-M0+
    //NVIC_SetPriority(SysTick_IRQn, 0U);
    // ...
}
//............................................................................
void QF_onCleanup(void) {
}
//............................................................................
#ifdef QF_ON_CONTEXT_SW
// NOTE: the context-switch callback is called with interrupts DISABLED
void QF_onContextSw(QActive *prev, QActive *next) {
}
#endif // QF_ON_CONTEXT_SW
//............................................................................
void QV_onIdle(void) { // CAUTION: called with interrupts DISABLED, see NOTE0
#ifdef NDEBUG
    // Put the CPU and peripherals to the low-power mode.
    // you might need to customize the clock management for your application,
    // see the datasheet for your particular Cortex-M MCU.
    //
    QV_CPU_SLEEP(); // atomically go to sleep and enable interrupts
#else
    QF_INT_ENABLE(); // just enable interrupts
#endif
}

//============================================================================
// NOTE0:
// The QV_onIdle() callback is called with interrupts disabled, because the
// determination of the idle condition might change by any interrupt posting
// an event. QV_onIdle() must internally enable interrupts, ideally
// atomically with putting the CPU to the power-saving mode.
//
