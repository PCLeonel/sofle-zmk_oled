# Copyright (c) 2026 Leonel Pinheiro Correa
# SPDX-License-Identifier: MIT

if(CONFIG_LEONEL_SOFLE_OLED)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/custom_status_screen.c
  )
endif()
