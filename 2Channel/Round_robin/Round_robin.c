// Continious dual analog sampling using two alternating DMA
// buffers.

//#include <Arduino.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
//#include "hardware/pio.h"

// Diagnostics LEDs.
// #define LED1_PIN 16
// #define LED2_PIN 17
// #define LED3_PIN 18

#define ADC_CHANNEL_1 0 // voltage
#define ADC_CHANNEL_2 2 // current

// define physical parameters
#define CAPTURE_DEPTH 1024 // 1666 : 10 cycles
#define ADC_VREF_VOLTAGE 3.3
// Frequency    : 3 kHz
// ADC          : 0.5 Ms/s
// # of samples : 166.6 / cycle

#define CAPTURE_RING_BITS 11

// Aligned for the DMA ring address warp.
uint16_t capture_buf1[CAPTURE_DEPTH] __attribute__((aligned(2 * CAPTURE_DEPTH)));
uint16_t capture_buf2[CAPTURE_DEPTH] __attribute__((aligned(2 * CAPTURE_DEPTH)));

static char buffer[200];

// TODO: How to redirect the standard printf to the Serial output?
// void xprintf(const char *format, ...)
// {
//     va_list argptr;
//     va_start(argptr, format);
//     vsnprintf(buffer, sizeof(buffer), format, argptr);
//     Serial.print(buffer);
//     va_end(argptr);
// }

uint dma_chan1;
uint dma_chan2;

uint_fast16_t irq_counter1 = 0;
uint_fast16_t irq_counter2 = 0;

// Called when capture_buf1 is full.
void dma_handler1()
{
    // Toggle debug LED.
    // gpio_xor_mask(1 << LED1_PIN);
    irq_counter1++;
    // Clear the interrupt request.
    dma_hw->ints0 = 1u << dma_chan1;
}

// Called when capture_buf2 is full.
void dma_handler2()
{
    // Toggle debug led.
    // gpio_xor_mask(1 << LED2_PIN);
    irq_counter2++;
    // Clear the interrupt request.
    dma_hw->ints0 = 1u << dma_chan2;
}

void main()
{
    // Serial.begin(115200);
    // Useless, this is the default...

    // TODO: why gpio_set_dir_out_masked doesn't work?
    // gpio_set_dir_out_masked(1 << LED_BUILTIN | 1 << LED1_PIN | 1 << LED2_PIN |
    // 1 << LED3_PIN);

    // LEDs init
    // gpio_set_dir(LED_BUILTIN, GPIO_OUT);
    //   pinMode(LED1_PIN, OUTPUT);
    //   pinMode(LED2_PIN, OUTPUT);
    //   pinMode(LED3_PIN, OUTPUT);

    // gpio_set_mask(1 << LED_BUILTIN);
    //   gpio_clr_mask(1 << LED1_PIN | 1 << LED2_PIN | 1 << LED3_PIN);

    // -------ADC init

    adc_gpio_init(26 + ADC_CHANNEL_1);
    adc_gpio_init(26 + ADC_CHANNEL_2);

    adc_init();
    // This determines the first channel that will be scanned in
    // each round robin cycle.
    adc_select_input(ADC_CHANNEL_1);
    // Alternating ADC sampling.
    adc_set_round_robin(1 << ADC_CHANNEL_1 | 1 << ADC_CHANNEL_2);

    adc_fifo_setup(true, // Write each completed conversion to the sample FIFO
                   true, // Enable DMA data request (DREQ)
                   1,    // DREQ (and IRQ) asserted when at least 1 sample present
                   true, // Collect also the error bit.
                   false // Do not reduce samples to 8 bits.
    );

    // Determines the ADC sampling rate as a divisor of the basic
    // 48Mhz clock. Set to have 100k sps on each of the two ADC
    // channels.
    adc_set_clkdiv(0); // Total rate 200k sps.

    // --------------- DMA

    dma_chan1 = dma_claim_unused_channel(true);
    dma_chan2 = dma_claim_unused_channel(true);

    // Chan 1
    {
        dma_channel_config dma_config1 = dma_channel_get_default_config(dma_chan1);
        channel_config_set_transfer_data_size(&dma_config1, DMA_SIZE_16);
        channel_config_set_read_increment(&dma_config1, false); // ADC fifo
        channel_config_set_write_increment(&dma_config1, true); // RAM buffer.
        // channel_config_set_write_increment(&dma_config1, true);
        // Wrap to begining of buffer. Assuming buffer is well alligned.
        channel_config_set_ring(&dma_config1, true, CAPTURE_RING_BITS);
        // Paced by ADC genered requests.
        channel_config_set_dreq(&dma_config1, DREQ_ADC);
        // When done, start the other channel.
        channel_config_set_chain_to(&dma_config1, dma_chan2);
        // Using interrupt channel 0
        dma_channel_set_irq0_enabled(dma_chan1, true);
        // Set IRQ handler.
        irq_set_exclusive_handler(DMA_IRQ_0, dma_handler1);
        irq_set_enabled(DMA_IRQ_0, true);
        dma_channel_configure(dma_chan1, &dma_config1,
                              capture_buf1,  // dst
                              &adc_hw->fifo, // src
                              CAPTURE_DEPTH, // transfer count
                              true           // start immediately
        );
    }

    // Chan 2
    {
        dma_channel_config dma_config2 = dma_channel_get_default_config(dma_chan2);
        channel_config_set_transfer_data_size(&dma_config2, DMA_SIZE_16);
        channel_config_set_read_increment(&dma_config2, false);
        channel_config_set_write_increment(&dma_config2, true);
        channel_config_set_ring(&dma_config2, true, CAPTURE_RING_BITS);
        channel_config_set_dreq(&dma_config2, DREQ_ADC);
        // When done, start the other channel.
        channel_config_set_chain_to(&dma_config2, dma_chan1);
        dma_channel_set_irq1_enabled(dma_chan2, true);
        irq_set_exclusive_handler(DMA_IRQ_1, dma_handler2);
        irq_set_enabled(DMA_IRQ_1, true);
        dma_channel_configure(dma_chan2, &dma_config2,
                              capture_buf2,  // dst
                              &adc_hw->fifo, // src
                              CAPTURE_DEPTH, // transfer count
                              false          // Do not start immediately
        );
    }

    //light up the internal LED
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    printf("Ready\r");
    // now the board is ready to start
    char ch;
    while(false){

        ch = getchar_timeout_us(0);

        if (ch == '\r'){ break;}

        else{ sleep_ms(500);}
    }

    // Start the ADC free run sampling.
    adc_run(true);

    dma_channel_wait_for_finish_blocking(dma_chan1);
    dma_channel_wait_for_finish_blocking(dma_chan2);
    //printf("Capture finished\n");
    adc_run(false);
    adc_fifo_drain();

    for (uint_fast16_t i = 0; i < CAPTURE_DEPTH; i++){

    uint16_t voltage = capture_buf1[i] ;
        printf("v%x\r", voltage);
    
    uint16_t current = capture_buf2[i] ;
        printf("i%x\r", current);
        }
}

