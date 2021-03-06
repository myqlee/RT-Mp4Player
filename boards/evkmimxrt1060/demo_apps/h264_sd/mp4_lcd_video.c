/*
 * Copyright 2019 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "board.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "fsl_common.h"
#include "fsl_debug_console.h"
#include "fsl_pxp.h"
#include "fsl_elcdif.h"
#include "fsl_cache.h"
#include "fsl_gpio.h"
#include "mp4.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define APP_ELCDIF LCDIF

#if VIDEO_LCD_RESOLUTION_HVGA272 == 1
#define APP_HSW 41
#define APP_HFP 4
#define APP_HBP 8
#define APP_VSW 10
#define APP_VFP 4
#define APP_VBP 2
#elif VIDEO_LCD_RESOLUTION_SVGA600 == 1
#define APP_HSW 48
#define APP_HFP 112
#define APP_HBP 88
#define APP_VSW 3
#define APP_VFP 21
#define APP_VBP 39
#elif VIDEO_LCD_RESOLUTION_WXGA800 == 1
#define APP_HSW 10
#define APP_HFP 70
#define APP_HBP 80
#define APP_VSW 3
#define APP_VFP 10
#define APP_VBP 10
#endif
#if (VIDEO_LCD_RESOLUTION_HVGA272 == 1) || (VIDEO_LCD_RESOLUTION_WXGA800 == 1)
#define APP_POL_FLAGS \
    (kELCDIF_DataEnableActiveHigh | kELCDIF_VsyncActiveLow | kELCDIF_HsyncActiveLow | kELCDIF_DriveDataOnRisingClkEdge)
#elif (VIDEO_LCD_RESOLUTION_SVGA600 == 1)
#define APP_POL_FLAGS \
    (kELCDIF_DataEnableActiveHigh | kELCDIF_VsyncActiveHigh | kELCDIF_HsyncActiveHigh | kELCDIF_DriveDataOnFallingClkEdge)
#endif

#define APP_LCDIF_DATA_BUS kELCDIF_DataBus16Bit

/* Display. */
#define LCD_DISP_GPIO GPIO1
#define LCD_DISP_GPIO_PIN 2
/* Back light. */
#define LCD_BL_GPIO GPIO2
#define LCD_BL_GPIO_PIN 31

/* Frame buffer data alignment, for better performance, the LCDIF frame buffer should be 64B align. */
#define FRAME_BUFFER_ALIGN 64
/*
 * For better performance, three frame buffers are used in this demo.
 */

#define APP_PXP PXP
#define APP_PS_WIDTH  1280 /* 1280,800,image resolution*/
#define APP_PS_HEIGHT 800  /* 1280,800,image resolution*/

#define APP_PS_ULC_X 0U
#define APP_PS_ULC_Y 0U

/*******************************************************************************
 * Prototypes
 ******************************************************************************/


/*******************************************************************************
 * Variables
 ******************************************************************************/
video_lcd_cfg_t g_videoLcdCfg;

static pxp_output_buffer_config_t s_outputBufferConfig;
static pxp_ps_buffer_config_t s_psBufferConfig;
#if VIDEO_PXP_CONV_BLOCKING == 0
AT_NONCACHEABLE_SECTION(static uint8_t s_convBufferYUV[3][APP_PS_HEIGHT][APP_PS_WIDTH]);
#endif
AT_NONCACHEABLE_SECTION_ALIGN(uint8_t g_psBufferLcd[APP_LCD_FB_NUM][APP_IMG_HEIGHT][APP_IMG_WIDTH][APP_BPP], FRAME_BUFFER_ALIGN);

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Initialize the LCD_DISP. */
void BOARD_InitLcd(void)
{
    gpio_pin_config_t config = {
        kGPIO_DigitalOutput, 0,
    };

#if (VIDEO_LCD_RESOLUTION_HVGA272 == 1) || (VIDEO_LCD_RESOLUTION_SVGA600 == 1)
    /* Reset the LCD. */
    GPIO_PinInit(LCD_DISP_GPIO, LCD_DISP_GPIO_PIN, &config);
    GPIO_PinWrite(LCD_DISP_GPIO, LCD_DISP_GPIO_PIN, 0);
    volatile uint32_t i = 0x100U;
    while (i--)
    {
    }
    GPIO_PinWrite(LCD_DISP_GPIO, LCD_DISP_GPIO_PIN, 1);
#elif VIDEO_LCD_RESOLUTION_WXGA800 == 1
    // Do nothing
#endif

    /* Backlight. */
    config.outputLogic = 1;
    GPIO_PinInit(LCD_BL_GPIO, LCD_BL_GPIO_PIN, &config);
}

void BOARD_InitLcdifPixelClock(void)
{
#if VIDEO_LCD_RESOLUTION_HVGA272 == 1
    /*
     * The desired output frame rate is 60Hz. So the pixel clock frequency is:
     * (480 + 41 + 4 + 18) * (272 + 10 + 4 + 2) * 60,30,25 = 9.2M,4.6M,3.83M.
     * Here set the LCDIF pixel clock to 9.3M,4.65M,3.875M.
     */

    /*
     * Initialize the Video PLL.
     * Video PLL output clock is OSC24M * (loopDivider + (denominator / numerator)) / postDivider = 93MHz.
     */
    clock_video_pll_config_t config = {
        .loopDivider = 31, .postDivider = 8, .numerator = 0, .denominator = 0,
    };
#elif VIDEO_LCD_RESOLUTION_SVGA600 == 1
    /*
     * The desired output frame rate is 60Hz. So the pixel clock frequency is:
     * (800 + 48 + 88 + 112) * (600 + 3 + 39 + 21) * 60,30,25 = 39.8M,19.9M,16.58M.
     * Here set the LCDIF pixel clock to 40M,20M,17.14M.
     */

    /*
     * Initialize the Video PLL.
     * Video PLL output clock is OSC24M * (loopDivider + (denominator / numerator)) / postDivider = 120MHz.
     */
    clock_video_pll_config_t config = {
        .loopDivider = 40, .postDivider = 8, .numerator = 0, .denominator = 0,
    };
#elif VIDEO_LCD_RESOLUTION_WXGA800 == 1
    /*
     * The desired output frame rate is 30Hz. So the pixel clock frequency is:
     * (1280 + 10 + 80 + 70) * (800 + 3 + 10 + 10) * 60,30,25 = 71M,35.5M,29.58M.
     * Here set the LCDIF pixel clock to 70.5M,35.25M,31.33M.
     */

    /*
     * Initialize the Video PLL.
     * Video PLL output clock is OSC24M * (loopDivider + (denominator / numerator)) / postDivider = 282MHz.
     */
    clock_video_pll_config_t config = {
        .loopDivider = 47, .postDivider = 4, .numerator = 0, .denominator = 0,
    };
#endif

    CLOCK_InitVideoPll(&config);

    /*
     * 000 derive clock from PLL2
     * 001 derive clock from PLL3 PFD3
     * 010 derive clock from PLL5
     * 011 derive clock from PLL2 PFD0
     * 100 derive clock from PLL2 PFD1
     * 101 derive clock from PLL3 PFD1
     */
    CLOCK_SetMux(kCLOCK_LcdifPreMux, 2);
#if VIDEO_LCD_RESOLUTION_HVGA272 == 1
#if VIDEO_LCD_REFRESH_FREG_60Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 4);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 1);
#elif VIDEO_LCD_REFRESH_FREG_30Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 4);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 3);
#elif VIDEO_LCD_REFRESH_FREG_25Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 3);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 5);
#endif
#elif VIDEO_LCD_RESOLUTION_SVGA600 == 1
#if VIDEO_LCD_REFRESH_FREG_60Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 2);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 0);
#elif VIDEO_LCD_REFRESH_FREG_30Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 2);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 1);
#elif VIDEO_LCD_REFRESH_FREG_25Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 6);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 0);
#endif
#elif VIDEO_LCD_RESOLUTION_WXGA800 == 1
#if VIDEO_LCD_REFRESH_FREG_60Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 3);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 0);
#elif VIDEO_LCD_REFRESH_FREG_30Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 3);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 1);
#elif VIDEO_LCD_REFRESH_FREG_25Hz == 1
    CLOCK_SetDiv(kCLOCK_LcdifPreDiv, 2);
    CLOCK_SetDiv(kCLOCK_LcdifDiv, 2);
#endif
#endif
}

static void APP_InitPxp(uint32_t psWidth)
{
    PXP_Init(APP_PXP);

    /* PS configure. */
    s_psBufferConfig.pixelFormat = kPXP_PsPixelFormatYVU420;
    s_psBufferConfig.swapByte = false;
    s_psBufferConfig.bufferAddr = 0U;
    s_psBufferConfig.bufferAddrU = 0U;
    s_psBufferConfig.bufferAddrV = 0U;
    s_psBufferConfig.pitchBytes = psWidth;
    PXP_SetProcessSurfaceBackGroundColor(APP_PXP, 0U);
    PXP_SetProcessSurfaceBufferConfig(APP_PXP, &s_psBufferConfig);
    /* Disable AS. */
    PXP_SetAlphaSurfacePosition(APP_PXP, 0xFFFFU, 0xFFFFU, 0U, 0U);

    /* Output config. */
#if VIDEO_PIXEL_FMT_RGB888 == 1
    s_outputBufferConfig.pixelFormat = kPXP_OutputPixelFormatRGB888;
#elif VIDEO_PIXEL_FMT_RGB565 == 1
    s_outputBufferConfig.pixelFormat = kPXP_OutputPixelFormatRGB565;
#endif
    s_outputBufferConfig.interlacedMode = kPXP_OutputProgressive;
    s_outputBufferConfig.buffer0Addr = (uint32_t)g_psBufferLcd[0];
    s_outputBufferConfig.buffer1Addr = 0U;
    s_outputBufferConfig.pitchBytes = APP_IMG_WIDTH * APP_BPP;
    s_outputBufferConfig.width = APP_IMG_WIDTH;
    s_outputBufferConfig.height = APP_IMG_HEIGHT;
    PXP_SetOutputBufferConfig(APP_PXP, &s_outputBufferConfig);

    /* Disable CSC1, it is enabled by default. */
    PXP_SetCsc1Mode(APP_PXP, kPXP_Csc1YCbCr2RGB);
    PXP_EnableCsc1(APP_PXP, true);
}

static void APP_InitLcdif(void)
{
    const elcdif_rgb_mode_config_t config = {
        .panelWidth = APP_IMG_WIDTH,
        .panelHeight = APP_IMG_HEIGHT,
        .hsw = APP_HSW,
        .hfp = APP_HFP,
        .hbp = APP_HBP,
        .vsw = APP_VSW,
        .vfp = APP_VFP,
        .vbp = APP_VBP,
        .polarityFlags = APP_POL_FLAGS,
        .bufferAddr = (uint32_t)g_psBufferLcd[0],
#if VIDEO_PIXEL_FMT_RGB888 == 1
        .pixelFormat = kELCDIF_PixelFormatXRGB8888,
#elif VIDEO_PIXEL_FMT_RGB565 == 1
        .pixelFormat = kELCDIF_PixelFormatRGB565,
#endif
        .dataBus = APP_LCDIF_DATA_BUS,
    };

    ELCDIF_RgbModeInit(APP_ELCDIF, &config);

#if VIDEO_LCD_RESOLUTION_HVGA272 == 1
    // Do nothing
#elif (VIDEO_LCD_RESOLUTION_WXGA800 == 1) || (VIDEO_LCD_RESOLUTION_SVGA600 == 1)
    /* Update the eLCDIF AXI master features for better performance */
    APP_ELCDIF->CTRL2 = 0x00700000;
#endif

#if (MP4_LCD_TIME_ENABLE == 0) || (MP4_LCD_DISP_OFF == 0)
    ELCDIF_RgbModeStart(APP_ELCDIF);
#endif
}

#if MP4_LCD_TIME_ENABLE == 1
extern void time_measure_start(void);
extern uint64_t time_measure_done(void);
lcd_measure_context_t g_lcdMeasureContext;
#endif //#if MP4_LCD_TIME_ENABLE

typedef enum _lcd_time_type
{
    kLcdTimeType_Start = 0U,
    kLcdTimeType_Pxp   = 1U,
    kLcdTimeType_Lcd   = 2U,
} lcd_time_type_t;

static void lcd_time_measure_utility(lcd_time_type_t type)
{
#if MP4_LCD_TIME_ENABLE == 1
    if (type == kLcdTimeType_Start)
    {
        time_measure_start();
    }
    else if (type == kLcdTimeType_Pxp)
    {
        g_lcdMeasureContext.costTimePxp_ns = time_measure_done();
    }
    else if (type == kLcdTimeType_Lcd)
    {
        g_lcdMeasureContext.costTimeLcd_ns = time_measure_done();
    }
#else
    // Do nothing
#endif
}

void lcd_video_display(uint32_t activeBufferAddr)
{
    ELCDIF_SetNextBufferAddr(APP_ELCDIF, activeBufferAddr);
    ELCDIF_ClearInterruptStatus(APP_ELCDIF, kELCDIF_CurFrameDone);
    while (!(kELCDIF_CurFrameDone & ELCDIF_GetInterruptStatus(APP_ELCDIF)))
    {
    }
}

void set_pxp_master_priority(uint32_t priority)
{
    // NIC-301(SIM_MAIN GPV) read qos
    *(uint32_t *)0x41046100 = priority;
    // NIC-301(SIM_MAIN GPV) write qos
    *(uint32_t *)0x41046104 = priority;
}

void set_lcd_master_priority(uint32_t priority)
{
    // NIC-301(SIM_MAIN GPV) read qos
    *(uint32_t *)0x41044100 = priority;
    // NIC-301(SIM_MAIN GPV) write qos
    *(uint32_t *)0x41044104 = priority;
}

/*!
 * @brief config_lcd function
 */
void config_lcd(video_lcd_cfg_t *lcdCfg)
{
    BOARD_InitLcdifPixelClock();
    BOARD_InitLcd();

    APP_InitPxp(lcdCfg->srcWidth);
    APP_InitLcdif();
    ELCDIF_EnableInterrupts(APP_ELCDIF, kELCDIF_CurFrameDoneInterruptEnable);

#if (VIDEO_LCD_RESOLUTION_WXGA800 == 1) || (VIDEO_LCD_RESOLUTION_SVGA600 == 1)
    set_lcd_master_priority(15);
    set_pxp_master_priority(14);
#elif VIDEO_LCD_RESOLUTION_HVGA272 == 1
    // Do nothing
    set_lcd_master_priority(15);
    set_pxp_master_priority(14);
#endif
}

