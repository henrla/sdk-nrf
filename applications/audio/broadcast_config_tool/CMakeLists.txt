#
# Copyright (c) 2024 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(BCT)

# Include application events and configuration headers
zephyr_library_include_directories(app PRIVATE
    ${ZEPHYR_NRF_MODULE_DIR}/applications/audio/broadcast_config_tool/include
    ${ZEPHYR_NRF_MODULE_DIR}/applications/nrf5340_audio/include
    ${ZEPHYR_NRF_MODULE_DIR}/applications/nrf5340_audio/src/bluetooth
    ${ZEPHYR_NRF_MODULE_DIR}/applications/nrf5340_audio/src/modules
    ${ZEPHYR_NRF_MODULE_DIR}/applications/nrf5340_audio/src/utils/macros
)

add_subdirectory(${ZEPHYR_NRF_MODULE_DIR}/applications/nrf5340_audio/src/bluetooth bluetooth_build)

target_sources_ifdef(CONFIG_NRF5340_AUDIO_SD_CARD_MODULE app PRIVATE ${ZEPHYR_NRF_MODULE_DIR}/applications/nrf5340_audio/src/modules/sd_card.c)


FILE(GLOB app_sources src/*.c)

target_sources(app PRIVATE ${app_sources})
