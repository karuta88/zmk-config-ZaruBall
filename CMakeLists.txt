zephyr_include_directories(include)

if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_SCROLL_AXIS_LOCK app PRIVATE
      src/behaviors/behavior_scroll_axis_lock.c
  )
  target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_SCROLL_AXIS_LOCK_MAPPER app PRIVATE
      src/input_processors/input_processor_scroll_axis_lock_mapper.c
  )
  target_sources_ifdef(CONFIG_ZMK_INPUT_PROCESSOR_AML_TEMP_LAYER app PRIVATE
      src/input_processors/input_processor_aml_temp_layer.c
  )
endif()
