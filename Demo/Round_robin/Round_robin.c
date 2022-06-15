/**
 * Copyright (c) 2021 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// standard includes
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// pico icludes
#include "pico/stdlib.h"
// For ADC input:
#include "hardware/adc.h"
#include "hardware/dma.h"
// For the digital trigger
#include "hardware/gpio.h"
// For resistor DAC output:
#include "pico/multicore.h"
//#include "hardware/pio.h"
//#include "resistor_dac.pio.h"

// This example uses the DMA to capture many samples from the ADC.
//
// - We are putting the ADC in free-running capture mode at 0.5 Msps
//
// - A DMA channel will be attached to the ADC sample FIFO
//
// - Configure the ADC to right-shift samples to 8 bits of significance, so we
//   can DMA into a byte buffer
//
// This could be extended to use the ADC's round robin feature to sample two
// channels concurrently at 0.25 Msps each.
//
// It would be nice to have some analog samples to measure! This example also
// drives waves out through a 5-bit resistor DAC, as found on the reference
// VGA board. If you have that board, you can take an M-F jumper wire from
// GPIO 26 to the Green pin on the VGA connector (top row, next-but-rightmost
// hole). Or you can ignore that part of the code and connect your own signal
// to the ADC input.

// Define pin numbers
// Channel 0 is GPIO26
#define CAPTURE_CHANNEL 0
#define VOLTAGE_CHANNEL 2
#define CONTROL_PIN 17

// define physical parameters
#define CAPTURE_DEPTH 127000 // 10 cycles
#define ADC_VREF_VOLTAGE 3.3
// Frequency    : 3 kHz
// ADC          : 0.5 Ms/s
// # of samples : 166.6 / cycle

uint16_t capture_buf[CAPTURE_DEPTH] ;


double adc_to_voltage(uint16_t read ){
 return (double)read * ADC_VREF_VOLTAGE / (1<<12);
}


int main() {
    
    sleep_ms(1000);
    stdio_init_all();

    bool do_start = true;

    // Send core 1 off to start driving the "DAC" whilst we configure the ADC.
    // multicore_launch_core1(core1_main); 
    // We DO NOT GENERATE THE TRIANGLE anymore ..

    // Init GPIO for analogue use: hi-Z, no pulls, disable digital input buffer.
    adc_gpio_init(26 + CAPTURE_CHANNEL);

    gpio_set_dir(CONTROL_PIN, GPIO_IN);
    gpio_set_pulls(CONTROL_PIN, // sets a pulldown resistor on the contol pin
     false, // we don't want a pullup
     true   // only the pulldown (ofc..)
     );
     
     //light up the internal LED
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    //printf("Ready\r");
    sleep_ms(1000);
    // now the board is ready to start

    adc_init();
    adc_select_input(CAPTURE_CHANNEL);
    adc_set_round_robin(1 << CAPTURE_CHANNEL | 1 << VOLTAGE_CHANNEL); //we are using channels 0 and 2
                                //for Signal and Voltage respectively

    adc_fifo_setup(
        true,    // Write each completed conversion to the sample FIFO
        true,    // Enable DMA data request (DREQ)
        1,       // DREQ (and IRQ) asserted when at least 1 sample present
        true,    // to later check error flags
        false    // we want to keep the LSB of the ADC
    );
    
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
    //printf("Ready\r");
    sleep_ms(1000);

    // Divisor of 0 -> full speed. Free-running capture with the divider is
    // equivalent to pressing the ADC_CS_START_ONCE button once per `div + 1`
    // cycles (div not necessarily an integer). Each conversion takes 96
    // cycles, so in general you want a divider of 0 (hold down the button
    // continuously) or > 95 (take samples less frequently than 96 cycle
    // intervals). This is all timed by the 48 MHz ADC clock.
    adc_set_clkdiv(0);

    //printf("Arming DMA\n");
    sleep_ms(1000);
    // Set up the DMA to start transferring data as soon as it appears in FIFO
    uint dma_chan = dma_claim_unused_channel(true);
    // Will panic if all channels are bein used
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);

    // Reading from constant address, writing to incrementing byte addresses
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&cfg, false);
    channel_config_set_write_increment(&cfg, true);

    channel_config_set_bswap(&cfg, false); // true to swap endianness of the 2 bytes. NEEDS to be tested

    // Pace transfers based on availability of ADC samples
    channel_config_set_dreq(&cfg, DREQ_ADC);

    dma_channel_configure(dma_chan, &cfg,
        capture_buf,    // dst
        &adc_hw->fifo,  // src
        CAPTURE_DEPTH,  // transfer count
        do_start            // true to start immediately
    );

    //printf("Starting capture\n");
    
    
    
    
    

    while (true)
    {
        
        if (do_start = gpio_get(CONTROL_PIN)){
            break;
        } sleep_ms(100);
    }
    gpio_put(LED_PIN, 1);
    adc_run(true);
    //printf("Starting capture\n");
    
    // Once DMA finishes, stop any new conversions from starting, and clean up
    // the FIFO in case the ADC was still mid-conversion.
    dma_channel_wait_for_finish_blocking(dma_chan);
    //printf("Capture finished\n");
    adc_run(false);
    adc_fifo_drain();
    
    printf("StartMidas\n");


    uint16_t value;
    // Print samples to stdout so you can display them in pyplot, excel, matlab
    for (int i = 0; i < CAPTURE_DEPTH; ++i) {
        value = capture_buf[i] & 0b111111111111;
        printf("%3x\n", value); // data is only in the first 12 bit
        //sleep_ms(10);
        
        //if (capture_buf[i] >> 15)
         //   printf("\n FIFO ERROR at measure %d\n", i);
    }
    
    //printf("\n Done \n");
    
    printf("EndMidas\n");
    
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    sleep_ms(1000);
    
    
}

// ----------------------------------------------------------------------------
// Code for driving the "DAC" output for us to measure

// Core 1 is just going to sit and drive samples out continously. PIO provides
// consistent sample frequency.

// #define OUTPUT_FREQ_KHZ 3
// #define SAMPLE_WIDTH 5
// // This is the green channel on the VGA board
// #define DAC_PIN_BASE 6

// void core1_main() {
//     PIO pio = pio0;
//     uint sm = pio_claim_unused_sm(pio0, true);
//     uint offset = pio_add_program(pio0, &resistor_dac_5bit_program);
//     resistor_dac_5bit_program_init(pio0, sm, offset,
//         OUTPUT_FREQ_KHZ * 1000 * 2 * (1 << SAMPLE_WIDTH), DAC_PIN_BASE);
//     while (true) {
//         // Triangle wave
//         for (int i = 0; i < (1 << SAMPLE_WIDTH); ++i)
//             pio_sm_put_blocking(pio, sm, i);
//         for (int i = 0; i < (1 << SAMPLE_WIDTH); ++i)
//             pio_sm_put_blocking(pio, sm, (1 << SAMPLE_WIDTH) - 1 - i);
//     }
// }


