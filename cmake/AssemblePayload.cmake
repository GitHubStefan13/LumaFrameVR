# Assemble a flat release payload from the build outputs.
#
# Invoked by the `payload` target:
#   cmake -DPAYLOAD_DIR=... -DBINARY=... -DSOURCE_DIR=... -DSHADER_DIR=...
#         -DVERSION=... -DBUILD_TYPE=... -P AssemblePayload.cmake
#
# The payload is the flat tree the packaging step consumes: the player,
# launch.sh, BUILD_INFO.json, branding/ and shaders/ at the top level, plus a
# SHA256SUMS.txt covering everything else. PackageRelease.cmake verifies it and
# packs it into the release archive.

foreach(required PAYLOAD_DIR BINARY SOURCE_DIR SHADER_DIR VERSION BUILD_TYPE)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "AssemblePayload.cmake: ${required} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${PAYLOAD_DIR}")
file(MAKE_DIRECTORY "${PAYLOAD_DIR}")

file(COPY "${BINARY}" DESTINATION "${PAYLOAD_DIR}")
file(COPY "${SOURCE_DIR}/launch.sh" "${SOURCE_DIR}/update.sh" "${SOURCE_DIR}/update.py" DESTINATION "${PAYLOAD_DIR}")
file(COPY "${SOURCE_DIR}/assets/branding" DESTINATION "${PAYLOAD_DIR}")
file(COPY "${SHADER_DIR}" DESTINATION "${PAYLOAD_DIR}")
# The language catalogues. The player looks for lang/ next to the executable
# (LUMAFRAMEVR_PACKAGE_ROOT/lang), so without them every interface string falls
# back to its raw key.
file(COPY "${SOURCE_DIR}/cpp/i18n/lang" DESTINATION "${PAYLOAD_DIR}")
# The eye assets are addressed relative to the package root (see assets/eye and the
# install rule in ApplicationRuntime.cmake), so the payload carries them too.
file(COPY "${SOURCE_DIR}/assets/eye" DESTINATION "${PAYLOAD_DIR}")

# No FFmpeg travels in the payload. The player links the frame's own libavcodec, libavformat,
# libavutil and libswresample, and those are present on the frame (SteamOS ships them, and the
# AArch64 SDK the build links against is a copy of them). A build host therefore needs no local
# FFmpeg, and there is nothing to copy in here.

# Optional: the loaders are deliberately not shipped. The Vulkan loader and the OpenXR
# loader must come from the same build as the driver and the runtime they talk to, and both
# are on the frame (SteamVR's own loader is on the launcher's path). Shipping a two year old
# loader next to a current driver is a crash inside the driver. What does stay is the OpenVR
# API library, which the player opens by name from the package root.
if(RUNTIME_DIR AND EXISTS "${RUNTIME_DIR}")
  file(COPY "${RUNTIME_DIR}/" DESTINATION "${PAYLOAD_DIR}/runtime")
endif()
# The licences travel with the release: the project's own text at the top level and the whole
# licenses directory beside it, which is the set 0.9.1 shipped plus the two added for the typeface
# and the icon set. There is deliberately no NOTICE file.
if(NOT EXISTS "${SOURCE_DIR}/LICENSE")
  message(FATAL_ERROR "AssemblePayload.cmake: the project has no LICENSE to ship")
endif()
file(COPY "${SOURCE_DIR}/LICENSE" DESTINATION "${PAYLOAD_DIR}")
file(COPY "${SOURCE_DIR}/licenses" DESTINATION "${PAYLOAD_DIR}")

if(LIBS_AUDIT AND EXISTS "${LIBS_AUDIT}")
  file(COPY "${LIBS_AUDIT}" DESTINATION "${PAYLOAD_DIR}")
endif()

file(CHMOD "${PAYLOAD_DIR}/lumaframevr" PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
file(CHMOD "${PAYLOAD_DIR}/launch.sh" "${PAYLOAD_DIR}/update.sh" PERMISSIONS
  OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

file(WRITE "${PAYLOAD_DIR}/BUILD_INFO.json"
"{
  \"candidate\": \"${VERSION}\",
  \"version\": \"${VERSION}\",
  \"build_type\": \"${BUILD_TYPE}\"
}
")

file(GLOB_RECURSE payload_entries RELATIVE "${PAYLOAD_DIR}" "${PAYLOAD_DIR}/*")
set(payload_files "")
foreach(entry IN LISTS payload_entries)
  if(NOT IS_DIRECTORY "${PAYLOAD_DIR}/${entry}")
    list(APPEND payload_files "${entry}")
  endif()
endforeach()
list(REMOVE_ITEM payload_files "SHA256SUMS.txt")
list(SORT payload_files)

set(manifest "")
foreach(relative IN LISTS payload_files)
  file(SHA256 "${PAYLOAD_DIR}/${relative}" checksum)
  string(APPEND manifest "${checksum}  ${relative}\n")
endforeach()
file(WRITE "${PAYLOAD_DIR}/SHA256SUMS.txt" "${manifest}")

message(STATUS "Assembled payload ${VERSION} (${BUILD_TYPE}) at ${PAYLOAD_DIR}")
