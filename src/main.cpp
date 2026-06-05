#include "MDR32VF0xI_config.h"
#include "MDR32VF0xI_rst_clk.h"
#include "MDR32VF0xI_utils.h"
#include "common.h"
#include "rgb.h"

#define RGB_BRIGHT      15

//#define DEMO_TITLE

#ifdef DEMO_TITLE
#define SCROLL_DURATION 100
#define SCROLL_PAUSE    500

/*
static const uint8_t TITLE[] = {
    0x3E, 0x41, 0x41, 0x22,  0x00, 0x00, 0x00,  0x7F, 0x08, 0x08, 0x7F,  0x00,  0x38, 0x44, 0x44, 0x38,
    0x00,  0x7C, 0x54, 0x54, 0x28,  0x00,  0x7C, 0x50, 0x20, 0x00, 0x7C,  0x00,  0x7C, 0x08, 0x10, 0x08,
    0x7C,  0x00, 0x00, 0x00,  0x7F, 0x01, 0x01, 0x01,  0x00,  0x38, 0x44, 0x44, 0x38,  0x00,  0x30, 0x4A,
    0x4A, 0x3C,  0x00,  0x38, 0x44, 0x44, 0x38,  0x00,  0x7C, 0x08, 0x10, 0x08, 0x7C,  0x00, 0x00,  0x5F,
    0x00, 0x00,  0x5F,  0x00, 0x00,  0x5F
};
*/
static const uint8_t TITLE[] = {
    0x3E, 0x41, 0x41, 0x22,  0x00, 0x00, 0x00,  0x30, 0x4A, 0x4A, 0x3C,  0x00,  0x7C, 0x10, 0x10, 0x7C,
    0x00,  0x39, 0x54, 0x54, 0x19,  0x00,  0x7C, 0x08, 0x10, 0x08, 0x7C,  0x00, 0x00, 0x00,  0xFC, 0x24,
    0x24, 0x18,  0x00,  0x38, 0x44, 0x44, 0x38,  0x00,  0x6C, 0x10, 0x7C, 0x10, 0x6C,  0x00,  0x30, 0x4A,
    0x4A, 0x3C,  0x00,  0x38, 0x54, 0x54, 0x18,  0x00,  0x7C, 0x10, 0x10, 0x7C,  0x00,  0x3C, 0x40, 0x20,
    0x7C,  0x00,  0x48, 0x34, 0x14, 0x7C,  0x00, 0x00,  0x5F,  0x00, 0x00,  0x5F,  0x00, 0x00,  0x5F
};

static void draw_title(uint8_t offset, uint8_t r, uint8_t g, uint8_t b) {
    RGB_Clear();
    for (uint8_t x = 0; x < RGB_WIDTH; ++x) {
        uint8_t row = TITLE[offset];

        for (uint8_t y = 0; y < 8; ++y) {
            if (row & (1 << y))
                RGB_Set(RGB_MatrixXY(x, y), r, g, b);
        }
        if (++offset >= sizeof(TITLE) / sizeof(TITLE[0]))
            break;
    }
}
#endif

static void initClock(void) {
    const RST_CLK_HCLK_InitTypeDef clk_init = {
#ifdef USE_HSE
        .RST_CLK_CPU_C1_Source      = RST_CLK_CPU_C1_CLK_SRC_HSE,
#else
        .RST_CLK_CPU_C1_Source      = RST_CLK_CPU_C1_CLK_SRC_HSI,
#endif
        .RST_CLK_PLLCPU_ClockSource = RST_CLK_PLLCPU_CLK_SRC_CPU_C1,
#if F_CPU == 8000000
        .RST_CLK_PLLCPU_Multiplier  = RST_CLK_PLLCPU_MUL_1,
#elif F_CPU == 24000000
        .RST_CLK_PLLCPU_Multiplier  = RST_CLK_PLLCPU_MUL_3,
#elif F_CPU == 48000000
        .RST_CLK_PLLCPU_Multiplier  = RST_CLK_PLLCPU_MUL_6,
#endif
#if F_CPU == 8000000
        .RST_CLK_CPU_C2_ClockSource = RST_CLK_CPU_C2_CLK_SRC_CPU_C1,
#else
        .RST_CLK_CPU_C2_ClockSource = RST_CLK_CPU_C2_CLK_SRC_PLLCPU,
#endif
        .RST_CLK_CPU_C3_Prescaler   = RST_CLK_CPU_C3_PRESCALER_DIV_1,
        .RST_CLK_HCLK_ClockSource   = RST_CLK_CPU_HCLK_CLK_SRC_CPU_C3,
    };

    RST_CLK_DeInit();
#ifdef USE_HSE
    RST_CLK_HSE_Cmd(ENABLE);
    while (RST_CLK_HSE_GetStatus() == ERROR) {}
#endif
#if F_CPU > 24000000
    FLASH_SetLatency(FLASH_LATENCY_CYCLE_1);
#endif
    RST_CLK_HCLK_Init(&clk_init);

#if F_CPU == 8000000
    RST_CLK_PER1_C2_ClkSelection(RST_CLK_PER1_C2_CLK_SRC_CPU_C1);
#else
    RST_CLK_PER1_C2_ClkSelection(RST_CLK_PER1_C2_CLK_SRC_PLLCPU);
#endif

    SystemCoreClockUpdate();

    MDR_RST_CLK->DIV_SYS_TIM = (F_CPU / 1000000) - 2;
}

int main(void) {
    initClock();

    RGB_Init();

    DELAY_Init(DELAY_MODE_CYCLE_COUNTER);

#ifdef DEMO_TITLE
    uint8_t color = 0, step = 0;

    while (1) {
        if (color == 0)
            draw_title(step, RGB_BRIGHT, 0, 0); // RED
        else if (color == 1)
            draw_title(step, 0, RGB_BRIGHT, 0); // GREEN
        else if (color == 2)
            draw_title(step, 0, 0, RGB_BRIGHT); // BLUE
        else // if (color == 3)
            draw_title(step, RGB_BRIGHT / 3, RGB_BRIGHT / 3, RGB_BRIGHT / 3); // WHITE
        RGB_Update();
        DELAY_WaitMs((step > 0) && (step <= sizeof(TITLE) / sizeof(TITLE[0]) - RGB_WIDTH - 1) ? SCROLL_DURATION : SCROLL_PAUSE);
        if (++step > sizeof(TITLE) / sizeof(TITLE[0]) - RGB_WIDTH) {
            step = 0;
            if (++color > 3)
                color = 0;
        }
    }
#else
    while (1) {
        RGB_Scroll(1);
        RGB_Set(0, RGB_BRIGHT, 0, 0);
        RGB_Update();
        DELAY_WaitMs(100);

        RGB_Scroll(1);
        RGB_Set(0, 0, RGB_BRIGHT, 0);
        RGB_Update();
        DELAY_WaitMs(100);

        RGB_Scroll(1);
        RGB_Set(0, 0, 0, RGB_BRIGHT);
        RGB_Update();
        DELAY_WaitMs(100);

        RGB_Scroll(1);
        RGB_Set(0, RGB_BRIGHT / 3, RGB_BRIGHT / 3, RGB_BRIGHT / 3);
        RGB_Update();
        DELAY_WaitMs(100);
    }
#endif
}
