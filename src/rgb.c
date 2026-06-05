#include <string.h>
#include "system_MDR32VF0xI_it.h"
#include "MDR32VF0xI_config.h"
#include "MDR32VF0xI_rst_clk.h"
#include "MDR32VF0xI_port.h"
#include "MDR32VF0xI_ssp.h"
#include "MDR32VF0xI_dma.h"
#include "common.h"
#include "rgb.h"

#define USE_FIFO

#ifndef USE_FIFO
#define USE_DMA
#endif

#define RGB_SSP         2 // 1..3 (A15, D3, C5)

#define RGB_PREFIX      40 // > 50 us LOW

#ifdef USE_FIFO
static uint8_t _rgb[RGB_WIDTH * RGB_HEIGHT * 3];
static volatile uint16_t _rgb_pos = 0;
#else
static uint32_t _rgb[RGB_PREFIX / 8 + (RGB_WIDTH * RGB_HEIGHT) * 24 / 8];
#endif

#ifdef USE_FIFO
static uint32_t rgbToSSP(void) {
    const uint8_t ZERO = 0x08; // 0b1000
    const uint8_t ONE = 0x0E;  // 0b1110

    uint32_t result = 0;

    if (_rgb_pos < sizeof(_rgb)) {
        uint8_t b = _rgb[_rgb_pos++];

        for (uint8_t bit = 0; bit < 8; ++bit) {
            result <<= 4;
            if (b & 0x80)
                result |= ONE;
            else
                result |= ZERO;
            b <<= 1;
        }
    }
    return result;
}
#else

static void setRGB(uint16_t index, uint32_t rgb) {
    const uint8_t ZERO = 0x08; // 0b1000
    const uint8_t ONE = 0x0E;  // 0b1110

    if (index < RGB_WIDTH * RGB_HEIGHT) {
        uint32_t bits = 0;

        index = RGB_PREFIX / 8 + index * 24 / 8;
        for (uint8_t bit = 0; bit < 24; ++bit) {
            if (rgb & 0x800000)
                bits |= ONE;
            else
                bits |= ZERO;
            if ((bit & 7) == 7) {
                _rgb[index++] = bits;
                bits = 0;
            } else { // (bits & 7) != 7
                bits <<= 4;
            }
            rgb <<= 1;
        }
    }
}

static void fillRGB(uint16_t from, uint16_t count, uint32_t rgb) {
    while ((from < RGB_WIDTH * RGB_HEIGHT) && count--) {
        setRGB(from++, rgb);
    }
}
#endif

#ifdef USE_FIFO
void RGB_Set(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < RGB_WIDTH * RGB_HEIGHT) {
        index *= 3;
#ifdef RGB_GRB
        _rgb[index++] = g;
        _rgb[index++] = r;
#else
        _rgb[index++] = r;
        _rgb[index++] = g;
#endif
        _rgb[index] = b;
    }
}

void RGB_Fill(uint16_t from, uint16_t count, uint8_t r, uint8_t g, uint8_t b) {
    while ((from < RGB_WIDTH * RGB_HEIGHT) && count--) {
        RGB_Set(from++, r, g, b);
    }
}

inline __attribute__((always_inline)) void RGB_Clear(void) {
//    RGB_Fill(0, RGB_WIDTH * RGB_HEIGHT, 0, 0, 0);
    memset(_rgb, 0, sizeof(_rgb));
}
#else

inline __attribute__((always_inline)) void RGB_Set(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_GRB
    setRGB(index, (g << 16) | (r << 8) | b);
#else
    setRGB(index, (r << 16) | (g << 8) | b);
#endif
}

inline __attribute__((always_inline)) void RGB_Fill(uint16_t from, uint16_t count, uint8_t r, uint8_t g, uint8_t b) {
#ifdef RGB_GRB
    fillRGB(from, count, (g << 16) | (r << 8) | b);
#else
    fillRGB(from, count, (r << 16) | (g << 8) | b);
#endif
}

inline __attribute__((always_inline)) void RGB_Clear(void) {
    fillRGB(0, RGB_WIDTH * RGB_HEIGHT, 0);
}
#endif

void RGB_Init(void) {
    const SSP_InitTypeDef ssp_init = {
        .SSP_CPSDVSR         = (F_CPU / 3000000) & 0xFE, // 3 MHz
        .SSP_SCR             = 0,
        .SSP_Mode            = SSP_MODE_MASTER,
        .SSP_WordLength      = SSP_WORD_LENGTH_32BIT,
        .SSP_ClockPhase      = SSP_CLOCK_PHASE_1EDGE,
        .SSP_ClockPolarity   = SSP_CLOCK_POLARITY_LOW,
        .SSP_FrameFormat     = SSP_FRAME_FORMAT_SPI,
        .SSP_SlaveModeOutput = DISABLE,
        .SSP_FastSPISlave    = DISABLE,
        .SSP_LoopbackMode    = DISABLE,
    };
    const PORT_InitTypeDef port_init = {
#if RGB_SSP == 1
        .PORT_Pin       = PORT_PIN_15,
#elif RGB_SSP == 2
        .PORT_Pin       = PORT_PIN_3,
#elif RGB_SSP == 3
        .PORT_Pin       = PORT_PIN_5,
#endif
        .PORT_Mode      = PORT_MODE_DIGITAL,
        .PORT_Direction = PORT_DIRECTION_OUTPUT,
#if RGB_SSP == 1
        .PORT_Function  = PORT_FUNCTION_MAIN,
#elif RGB_SSP == 2
        .PORT_Function  = PORT_FUNCTION_MAIN,
#elif RGB_SSP == 3
        .PORT_Function  = PORT_FUNCTION_ALTERNATIVE,
#endif
        .PORT_Power     = PORT_POWER_NOMINAL_UPTO_2mA,
        .PORT_PullUp    = PORT_PULL_UP_OFF,
        .PORT_PullDown  = PORT_PULL_DOWN_OFF,
    };
#ifdef USE_FIFO
    const CLIC_IRQ_InitTypeDef irq_init = {
        .CLIC_EnableIRQ        = ENABLE,
        .CLIC_VectorModeIRQ    = ENABLE,
        .CLIC_TriggerIRQ       = CLIC_TRIGGER_IRQ_EDGE_RISING,
        .CLIC_PrivilegeModeIRQ = CLIC_PRIVILEGE_MODE_IRQ_M,
        .CLIC_LevelIRQ         = 1,
        .CLIC_PriorityIRQ      = 1,
    };
#endif

#ifdef USE_DMA
    RST_CLK_PCLKCmd(RST_CLK_PCLK_DMA, ENABLE);
    DMA_Cmd(ENABLE);
#endif

#if RGB_SSP == 1
    RST_CLK_PER1_C2_SetPrescaler(RST_CLK_PER1_C2_SSP1, RST_CLK_PER1_PRESCALER_DIV_1);
    RST_CLK_PER1_C2_Cmd(RST_CLK_PER1_C2_SSP1, ENABLE);

    RST_CLK_PCLKCmd(RST_CLK_PCLK_SSP1, ENABLE);
    SSP_DeInit(MDR_SSP1);
    SSP_Init(MDR_SSP1, &ssp_init);
#ifdef USE_DMA
    SSP_DMACmd(MDR_SSP1, SSP_DMA_REQ_TX, ENABLE);
#endif

    RST_CLK_PCLKCmd(RST_CLK_PCLK_PORTA, ENABLE);
    PORT_Init(MDR_PORTA, &port_init);

#ifdef USE_DMA
    SSP_Cmd(MDR_SSP1, ENABLE);
#endif
#elif RGB_SSP == 2
    RST_CLK_PER1_C2_SetPrescaler(RST_CLK_PER1_C2_SSP2, RST_CLK_PER1_PRESCALER_DIV_1);
    RST_CLK_PER1_C2_Cmd(RST_CLK_PER1_C2_SSP2, ENABLE);

    RST_CLK_PCLKCmd(RST_CLK_PCLK_SSP2, ENABLE);
    SSP_DeInit(MDR_SSP2);
    SSP_Init(MDR_SSP2, &ssp_init);
#ifdef USE_DMA
    SSP_DMACmd(MDR_SSP2, SSP_DMA_REQ_TX, ENABLE);
#endif

    RST_CLK_PCLKCmd(RST_CLK_PCLK_PORTD, ENABLE);
    PORT_Init(MDR_PORTD, &port_init);

#ifdef USE_DMA
    SSP_Cmd(MDR_SSP2, ENABLE);
#endif
#elif RGB_SSP == 3
    RST_CLK_PER1_C2_SetPrescaler(RST_CLK_PER1_C2_SSP3, RST_CLK_PER1_PRESCALER_DIV_1);
    RST_CLK_PER1_C2_Cmd(RST_CLK_PER1_C2_SSP3, ENABLE);

    RST_CLK_PCLKCmd(RST_CLK_PCLK_SSP3, ENABLE);
    SSP_DeInit(MDR_SSP3);
    SSP_Init(MDR_SSP3, &ssp_init);
#ifdef USE_DMA
    SSP_DMACmd(MDR_SSP3, SSP_DMA_REQ_TX, ENABLE);
#endif

    RST_CLK_PCLKCmd(RST_CLK_PCLK_PORTC, ENABLE);
    PORT_Init(MDR_PORTC, &port_init);

#ifdef USE_DMA
    SSP_Cmd(MDR_SSP3, ENABLE);
#endif
#endif

#ifdef USE_FIFO
#if RGB_SSP == 1
    CLIC_InitIRQ(SSP1_IRQn, &irq_init);

    SSP_Cmd(MDR_SSP1, ENABLE);
#elif RGB_SSP == 2
    CLIC_InitIRQ(SSP2_IRQn, &irq_init);

    SSP_Cmd(MDR_SSP2, ENABLE);
#elif RGB_SSP == 3
    CLIC_InitIRQ(SSP3_IRQn, &irq_init);

    SSP_Cmd(MDR_SSP3, ENABLE);
#endif

    RGB_Clear();
#else
    memset(_rgb, 0, RGB_PREFIX / 2);
    fillRGB(0, RGB_WIDTH * RGB_HEIGHT, 0);
#endif
}

void RGB_Update(void) {
#ifdef USE_FIFO
#if RGB_SSP == 1
    SSP_ITConfig(MDR_SSP1, SSP_IT_TX, DISABLE);
    SSP_TransmitterFIFOClear(MDR_SSP1);

    _rgb_pos = 0;
    for (uint8_t i = 0; i < RGB_PREFIX / 8; ++i) {
        while (SSP_GetFlagStatus(MDR_SSP1, SSP_FLAG_TNF) == RESET) {}
        SSP_SendData(MDR_SSP1, 0);
    }
    while (SSP_GetFlagStatus(MDR_SSP1, SSP_FLAG_TNF) == SET) {
        uint32_t data = rgbToSSP();

        if (! data)
            break;
        SSP_SendData(MDR_SSP1, data);
    }
    if (_rgb_pos < sizeof(_rgb))
        SSP_ITConfig(MDR_SSP1, SSP_IT_TX, ENABLE);
#elif RGB_SSP == 2
    SSP_ITConfig(MDR_SSP2, SSP_IT_TX, DISABLE);
    SSP_TransmitterFIFOClear(MDR_SSP2);

    _rgb_pos = 0;
    for (uint8_t i = 0; i < RGB_PREFIX / 8; ++i) {
        while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_TNF) == RESET) {}
        SSP_SendData(MDR_SSP2, 0);
    }
    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_TNF) == SET) {
        uint32_t data = rgbToSSP();

        if (! data)
            break;
        SSP_SendData(MDR_SSP2, data);
    }
    if (_rgb_pos < sizeof(_rgb))
        SSP_ITConfig(MDR_SSP2, SSP_IT_TX, ENABLE);
#elif RGB_SSP == 3
    SSP_ITConfig(MDR_SSP3, SSP_IT_TX, DISABLE);
    SSP_TransmitterFIFOClear(MDR_SSP3);

    _rgb_pos = 0;
    for (uint8_t i = 0; i < RGB_PREFIX / 8; ++i) {
        while (SSP_GetFlagStatus(MDR_SSP3, SSP_FLAG_TNF) == RESET) {}
        SSP_SendData(MDR_SSP3, 0);
    }
    while (SSP_GetFlagStatus(MDR_SSP3, SSP_FLAG_TNF) == SET) {
        uint32_t data = rgbToSSP();

        if (! data)
            break;
        SSP_SendData(MDR_SSP3, data);
    }
    if (_rgb_pos < sizeof(_rgb))
        SSP_ITConfig(MDR_SSP3, SSP_IT_TX, ENABLE);
#endif
#else // #ifdef USE_FIFO
#ifdef USE_DMA
    const DMA_CtrlData_InitTypeDef dma_ctrl = {
        .DMA_SourceBaseAddr    = (uint32_t)&_rgb,
#if RGB_SSP == 1
        .DMA_DestBaseAddr      = (uint32_t)&MDR_SSP1->DR,
#elif RGB_SSP == 2
        .DMA_DestBaseAddr      = (uint32_t)&MDR_SSP2->DR,
#elif RGB_SSP == 3
        .DMA_DestBaseAddr      = (uint32_t)&MDR_SSP3->DR,
#endif
        .DMA_SourceIncSize     = DMA_DATA_INC_32BIT,
        .DMA_DestIncSize       = DMA_DATA_INC_NO,
        .DMA_MemoryDataSize    = DMA_MEMORY_DATA_SIZE_32BIT,
        .DMA_Mode              = DMA_MODE_BASIC,
        .DMA_CycleSize         = sizeof(_rgb) / sizeof(_rgb[0]),
        .DMA_ArbitrationPeriod = DMA_ARB_AFTER_4,
    };
    const DMA_Channel_InitTypeDef dma_init = {
        .DMA_PriCtrlData         = (DMA_CtrlData_InitTypeDef*)&dma_ctrl,
        .DMA_AltCtrlData         = (DMA_CtrlData_InitTypeDef*)0,
        .DMA_UseHighPriority     = DISABLE,
        .DMA_UseBurst            = DISABLE,
        .DMA_SelectDataStructure = DMA_CTRL_DATA_PRIMARY,
    };
#endif

//    memset(_rgb, 0, RGB_PREFIX / 2);

#if RGB_SSP == 1
#ifdef USE_DMA
    DMA_ChannelCmd(DMA_CHANNEL_SSP1_TX, DISABLE);
    DMA_ChannelInit(DMA_CHANNEL_SSP1_TX, &dma_init);
    DMA_ChannelCmd(DMA_CHANNEL_SSP1_TX, ENABLE);
#else
    SSP_Cmd(MDR_SSP1, ENABLE);

    for (uint32_t i = 0; i < sizeof(_rgb) / sizeof(_rgb[0]); ++i) {
        while (SSP_GetFlagStatus(MDR_SSP1, SSP_FLAG_TNF) == RESET) {}
        SSP_SendData(MDR_SSP1, _rgb[i]);
    }
    while (SSP_GetFlagStatus(MDR_SSP1, SSP_FLAG_TFE) == RESET) {}
    while (SSP_GetFlagStatus(MDR_SSP1, SSP_FLAG_BSY) == SET) {}

    SSP_Cmd(MDR_SSP1, DISABLE);
#endif
#elif RGB_SSP == 2
#ifdef USE_DMA
    DMA_ChannelCmd(DMA_CHANNEL_SSP2_TX, DISABLE);
    DMA_ChannelInit(DMA_CHANNEL_SSP2_TX, &dma_init);
    DMA_ChannelCmd(DMA_CHANNEL_SSP2_TX, ENABLE);
#else
    SSP_Cmd(MDR_SSP2, ENABLE);

    for (uint32_t i = 0; i < sizeof(_rgb) / sizeof(_rgb[0]); ++i) {
        while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_TNF) == RESET) {}
        SSP_SendData(MDR_SSP2, _rgb[i]);
    }
    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_TFE) == RESET) {}
    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_BSY) == SET) {}

    SSP_Cmd(MDR_SSP2, DISABLE);
#endif
#elif RGB_SSP == 3
#ifdef USE_DMA
    DMA_ChannelCmd(DMA_CHANNEL_SSP3_TX, DISABLE);
    DMA_ChannelInit(DMA_CHANNEL_SSP3_TX, &dma_init);
    DMA_ChannelCmd(DMA_CHANNEL_SSP3_TX, ENABLE);
#else
    SSP_Cmd(MDR_SSP3, ENABLE);

    for (uint32_t i = 0; i < sizeof(_rgb) / sizeof(_rgb[0]); ++i) {
        while (SSP_GetFlagStatus(MDR_SSP3, SSP_FLAG_TNF) == RESET) {}
        SSP_SendData(MDR_SSP3, _rgb[i]);
    }
    while (SSP_GetFlagStatus(MDR_SSP3, SSP_FLAG_TFE) == RESET) {}
    while (SSP_GetFlagStatus(MDR_SSP3, SSP_FLAG_BSY) == SET) {}

    SSP_Cmd(MDR_SSP3, DISABLE);
#endif
#endif
#endif
}

void RGB_Scroll(int16_t delta) {
    if (((delta > 0) && (delta < RGB_WIDTH * RGB_HEIGHT)) || ((delta < 0) && (delta > -(RGB_WIDTH * RGB_HEIGHT)))) {
#ifdef USE_FIFO
        if (delta > 0)
            memmove(&_rgb[delta * 3], _rgb, (RGB_WIDTH * RGB_HEIGHT - delta) * 3);
        else
            memmove(_rgb, &_rgb[-delta * 3], (RGB_WIDTH * RGB_HEIGHT + delta) * 3);
#else
        if (delta > 0)
            memmove(&_rgb[RGB_PREFIX / 8 + delta * 24 / 8], &_rgb[RGB_PREFIX / 8], (RGB_WIDTH * RGB_HEIGHT - delta) * 24 / 2);
        else
            memmove(&_rgb[RGB_PREFIX / 8], &_rgb[RGB_PREFIX / 8 - delta * 24 / 8], (RGB_WIDTH * RGB_HEIGHT + delta) * 24 / 2);
#endif
    }
}

#if RGB_HEIGHT > 1
uint16_t RGB_MatrixXY(uint8_t x, uint8_t y) {
    uint16_t result;

    result = x * RGB_HEIGHT;
    if (x & 1)
        result += ((RGB_HEIGHT - 1) - y);
    else
        result += y;
    return result;
}
#endif

#ifdef USE_FIFO
#if RGB_SSP == 1
__INTERRUPT_MACHINE void SSP1_IRQHandler() {
    while (SSP_GetFlagStatus(MDR_SSP1, SSP_FLAG_TNF) == SET) {
        uint32_t data = rgbToSSP();

        if (! data) {
            SSP_ITConfig(MDR_SSP1, SSP_IT_TX, DISABLE);
            break;
        }
        SSP_SendData(MDR_SSP1, data);
    }
#elif RGB_SSP == 2
__INTERRUPT_MACHINE void SSP2_IRQHandler() {
    while (SSP_GetFlagStatus(MDR_SSP2, SSP_FLAG_TNF) == SET) {
        uint32_t data = rgbToSSP();

        if (! data) {
            SSP_ITConfig(MDR_SSP2, SSP_IT_TX, DISABLE);
            break;
        }
        SSP_SendData(MDR_SSP2, data);
    }
#elif RGB_SSP == 3
__INTERRUPT_MACHINE void SSP3_IRQHandler() {
    while (SSP_GetFlagStatus(MDR_SSP3, SSP_FLAG_TNF) == SET) {
        uint32_t data = rgbToSSP();

        if (! data) {
            SSP_ITConfig(MDR_SSP3, SSP_IT_TX, DISABLE);
            break;
        }
        SSP_SendData(MDR_SSP3, data);
    }
#endif
}
#endif
