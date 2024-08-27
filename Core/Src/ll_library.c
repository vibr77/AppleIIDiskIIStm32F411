
#include "ll_library.h"

void LL_DMA_SPI_Init(){

}

void LL_DMA_SPI_Start(){

}
void LL_DMA_SPI_Stop(){

}

void LL_GPIO_Init(void){


    SET_BIT(RCC->AHB1ENR,RCC_APB1ENR_TIM3EN);
    
    //RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;                // GPIOA clock enable
    
    GPIOA->MODER    &= ~GPIO_MODER_MODER8;              // Configure PA08 as Alternative Function.
    GPIOA->MODER    |= GPIO_MODER_MODER8_1;
    GPIOA->AFR[1]   &= ~GPIO_AFRH_AFRH1;                // PA8 configured as AF1: TIM1_CH4
    GPIOA->AFR[1]   |= 0x01000000;
}

void LL_TIMER1_Init(void){
    
    LL_GPIO_Init();
   
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;                 // TIM1 timer clock enable
    TIM1->PSC   = 0;                                    // No Prescaler
    TIM1->ARR   = 100;                                  // Period                                   
    TIM1->RCR   = 0;                                      // RepetitionCounter
    
    TIM1->CR1 &= ~TIM_CR1_CKD;
    TIM1->CR1 |= TIM_CLOCKDIVISION_DIV1;

    MODIFY_REG(TIM1->CR1, TIM_CR1_ARPE, TIM_AUTORELOAD_PRELOAD_ENABLE);
    
    
    TIM1-> CCR4 = 50;
   
    TIM1->EGR   |= TIM_EGR_UG;                          // Before starting the counter, you have to initialize all the registers
    TIM1->BDTR  |= TIM_BDTR_MOE;
    
    TIM1->CR1   |= TIM_CR1_CEN;                         // Start Timer

    TIM1->CCER &= ~TIM_CCER_CC1E;                       // Disable the Channel 1: Reset the CC1E Bit */

    TIM1->CCMR1 &= ~TIM_CCMR1_OC1M;                     // Reset the Output Compare Mode Bits */
    TIM1->CCMR1 &= ~TIM_CCMR1_CC1S;
    
    TIM1->CCMR1 |= TIM_OCMODE_PWM1;                     // Select the Output Compare Mode */
  
    TIM1->CCER &= ~TIM_CCER_CC1P;
    TIM1->CCER |= TIM_OCPOLARITY_HIGH;                  // Set the Output Compare Polarity */

    TIM1->CCER &= ~TIM_CCER_CC1NP;
    TIM1->CCER |= TIM_OCNPOLARITY_HIGH;                 // Set the Output N Polarity */
    TIM1->CCER &= ~TIM_CCER_CC1NE;                      // Reset the Output N State */

    TIM1->CR2 &= ~TIM_CR2_OIS1;
    TIM1->CR2 &= ~TIM_CR2_OIS1N;
   
    TIM1->CR2 |= TIM_OCIDLESTATE_RESET;                 // Set the Output Idle state */
    TIM1->CR2 |= TIM_OCNIDLESTATE_RESET;                // Set the Output N Idle state */
 
    TIM1->CCR1 = 25;                                    // Pulse Duty Cycle

}


void LL_PWM_TIMER_Start(){

}

void LL_PWM_TIMER_Stop(){

}