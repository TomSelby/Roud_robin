
add_executable(Round_robin
		Round_robin.c
        )


# pico_generate_pio_header(adc_test ${CMAKE_CURRENT_LIST_DIR}/resistor_dac.pio)

pico_enable_stdio_usb(Round_robin 1)
pico_enable_stdio_uart(Round_robin 0)

target_link_libraries(Round_robin
		pico_stdlib
		hardware_adc
		hardware_dma
		# For the control pin
		hardware_gpio
		# For the dummy output:
		# hardware_pio
		pico_multicore
		)

# create map/bin/hex file etc.
pico_add_extra_outputs(Round_robin)

# add url via pico_set_program_url
#example_auto_set_url(adc_test)
