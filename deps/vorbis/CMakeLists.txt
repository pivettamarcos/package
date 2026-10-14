IF(TARGET ogg)
  set(CMAKE_UNITY_BUILD false)

  add_subdirectory(tree)
  target_output_to_toolchain(vorbis)
  target_output_to_toolchain(vorbisfile)
ENDIF()
