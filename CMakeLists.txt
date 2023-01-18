add_executable(kippenhok
        kippenhok.c
        )

add_library(Dusk2Dawn Dusk2Dawn.c)

# pull in common dependencies
target_link_libraries(kippenhok pico_stdlib hardware_rtc hardware_adc hardware_sleep pico_sync pico_multicore Dusk2Dawn)

# create map/bin/hex file etc.
pico_add_extra_outputs(kippenhok)

# add url via pico_set_program_url
example_auto_set_url(kippenhok)
