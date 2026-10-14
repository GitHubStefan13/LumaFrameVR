# Pack a verified flat release payload into the immutable release archive.
#
# Invoked by the `installer` target, right after AssemblePayload.cmake has built
# the payload:
#
#   cmake -DPAYLOAD_DIR=... -DCANDIDATE=... -DOUTPUT_DIR=... -DSOURCE_DIR=...
#         [-DALLOW_HOST_ARCHITECTURE=ON]
#         -P PackageRelease.cmake
#
# The archive is what the desktop installer embeds and what a user can also
# unpack by hand. It is refused unless the payload is complete, self consistent
# and AArch64, and the destination must not exist yet: a published release is
# never overwritten.
#
# It is CMake throughout, so this step needs no interpreter.

set(ARCHIVE_PREFIX "lumaframevr-steam-frame-arm64-")
set(EXCLUDED build target dist artifacts __pycache__ .git investigation .venv .env)
set(CARRIED_SOURCE_BUNDLE "source")
set(REQUIRED lumaframevr launch.sh BUILD_INFO.json branding/icon.png)
set(LIBCARRY_ARCHIVE "source/lumaframevr-source.zip")
set(CHANGE "Single shell desktop installer with repair, uninstall and optional Steam shortcut")
# An ELF64, little endian header, and e_machine 183 at offset 18.
set(ELF64_LITTLE "7f454c460201")
set(AARCH64_MACHINE "b700")
# A safe name is one alphanumeric character then up to a hundred more from
# [A-Za-z0-9._-]. This regex engine has no {m,n} intervals, so the pattern is
# anchored and the length bound is checked beside it.
set(SAFE_NAME "^[A-Za-z0-9][A-Za-z0-9._-]*$")
set(SAFE_NAME_LIMIT 101)

foreach(required PAYLOAD_DIR CANDIDATE OUTPUT_DIR SOURCE_DIR)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "PackageRelease.cmake: ${required} is required")
  endif()
endforeach()

# The paths the release does not carry: any path with such a component, and
# anything under a top level source/ directory, which this step builds itself.
string(REPLACE ";" "|" excluded_alternation "${EXCLUDED}")
string(REPLACE "." "\\." excluded_alternation "${excluded_alternation}")
set(EXCLUDED_PATH "(^|/)(${excluded_alternation})(/|$)")

function(package_safe_name value refusal)
  string(LENGTH "${value}" length)
  if(NOT value MATCHES "${SAFE_NAME}" OR length GREATER SAFE_NAME_LIMIT)
    message(FATAL_ERROR "${refusal}")
  endif()
endfunction()

# Verify one package directory and report, in the caller's scope:
#   VERIFIED_PATHS         the manifest paths, in manifest order
#   VERIFIED_CANDIDATE     BUILD_INFO.json "candidate"
#   VERIFIED_VERSION       BUILD_INFO.json "version"
#   VERIFIED_BUILD_TYPE    BUILD_INFO.json "build_type"
#   VERIFIED_VERIFICATION  BUILD_INFO.json "verification" as JSON text; {} when absent
#   VERIFIED_HOST_ARCH     TRUE when the player is not AArch64
function(package_verify dir)
  if(IS_SYMLINK "${dir}" OR NOT IS_DIRECTORY "${dir}")
    message(FATAL_ERROR "The package root must be a directory, not a symlink: ${dir}")
  endif()

  # A versioned Release package, and nothing else, may be published.
  if(NOT EXISTS "${dir}/BUILD_INFO.json")
    message(FATAL_ERROR "The package has no BUILD_INFO.json: ${dir}")
  endif()
  file(READ "${dir}/BUILD_INFO.json" metadata)
  foreach(field candidate version build_type)
    string(JSON ${field} ERROR_VARIABLE absent GET "${metadata}" ${field})
    if(absent)
      message(FATAL_ERROR "BUILD_INFO.json has no \"${field}\": ${dir}/BUILD_INFO.json")
    endif()
  endforeach()
  if(NOT build_type STREQUAL "Release")
    message(FATAL_ERROR "Only a versioned Release package may be published: ${dir}")
  endif()
  package_safe_name("${candidate}" "Only a versioned Release package may be published: ${dir}")
  string(JSON verification ERROR_VARIABLE absent GET "${metadata}" verification)
  if(absent)
    set(verification "{}")
  endif()

  # The player is an ELF64, and unless the host architecture is explicitly
  # allowed, an AArch64 one. The header is read as bytes, not parsed.
  if(NOT EXISTS "${dir}/lumaframevr")
    message(FATAL_ERROR "The package has no player: ${dir}/lumaframevr")
  endif()
  file(READ "${dir}/lumaframevr" header HEX OFFSET 0 LIMIT 20)
  string(TOLOWER "${header}" header)
  string(LENGTH "${header}" header_length)
  set(host_arch FALSE)
  if(NOT header_length EQUAL 40 OR NOT header MATCHES "^${ELF64_LITTLE}")
    message(FATAL_ERROR "The player is not a Linux ELF64 binary: ${dir}/lumaframevr")
  endif()
  string(SUBSTRING "${header}" 36 4 machine)
  if(NOT machine STREQUAL "${AARCH64_MACHINE}")
    if(NOT ALLOW_HOST_ARCHITECTURE)
      message(FATAL_ERROR "The player is not a Linux AArch64 ELF64 binary: ${dir}/lumaframevr")
    endif()
    set(host_arch TRUE)
  endif()

  # The manifest covers exactly the package: every line is a safe, unique,
  # relative POSIX path naming a regular file that hashes to the value given,
  # and the set of listed paths equals the set of files present.
  if(NOT EXISTS "${dir}/SHA256SUMS.txt")
    message(FATAL_ERROR "The package has no SHA256SUMS.txt: ${dir}")
  endif()
  file(STRINGS "${dir}/SHA256SUMS.txt" lines)
  set(paths "")
  foreach(line IN LISTS lines)
    string(LENGTH "${line}" line_length)
    if(line_length LESS 66)
      message(FATAL_ERROR "Malformed package manifest line: ${line}")
    endif()
    string(SUBSTRING "${line}" 0 64 checksum)
    string(SUBSTRING "${line}" 64 2 separator)
    string(SUBSTRING "${line}" 66 -1 name)
    if(NOT separator STREQUAL "  " OR NOT checksum MATCHES "^[0-9a-f]*$" OR name STREQUAL "")
      message(FATAL_ERROR "Malformed package manifest line: ${line}")
    endif()
    string(FIND "${name}" "/" leading)
    string(FIND "${name}" "//" doubled)
    string(FIND "${name}" "\\" backslash)
    string(LENGTH "${name}" name_length)
    math(EXPR last "${name_length} - 1")
    string(SUBSTRING "${name}" "${last}" 1 trailing)
    if(leading EQUAL 0 OR NOT doubled EQUAL -1 OR NOT backslash EQUAL -1 OR trailing STREQUAL "/"
       OR name MATCHES "(^|/)[.]($|/)" OR name MATCHES "(^|/)[.][.]($|/)")
      message(FATAL_ERROR "Unsafe or duplicate package path: ${name}")
    endif()
    list(FIND paths "${name}" duplicate)
    if(NOT duplicate EQUAL -1)
      message(FATAL_ERROR "Unsafe or duplicate package path: ${name}")
    endif()
    if(IS_SYMLINK "${dir}/${name}" OR NOT EXISTS "${dir}/${name}" OR IS_DIRECTORY "${dir}/${name}")
      message(FATAL_ERROR "Package checksum or type failure: ${name}")
    endif()
    file(SHA256 "${dir}/${name}" actual)
    if(NOT actual STREQUAL checksum)
      message(FATAL_ERROR "Package checksum or type failure: ${name}")
    endif()
    list(APPEND paths "${name}")
  endforeach()

  file(GLOB_RECURSE entries RELATIVE "${dir}" "${dir}/*")
  set(present "")
  foreach(entry IN LISTS entries)
    if(IS_SYMLINK "${dir}/${entry}")
      message(FATAL_ERROR "Linked package member: ${dir}/${entry}")
    endif()
    if(NOT IS_DIRECTORY "${dir}/${entry}")
      list(APPEND present "${entry}")
    endif()
  endforeach()
  list(REMOVE_ITEM present "SHA256SUMS.txt")
  set(manifest_paths "${paths}")
  list(SORT manifest_paths)
  list(SORT present)
  list(LENGTH manifest_paths listed)
  list(LENGTH present found)
  if(NOT listed EQUAL found OR NOT "${manifest_paths}" STREQUAL "${present}")
    message(FATAL_ERROR "The manifest does not cover exactly the complete package: "
      "${listed} paths listed, ${found} files present, in ${dir}")
  endif()

  foreach(required IN LISTS REQUIRED)
    list(FIND paths "${required}" index)
    if(index EQUAL -1)
      message(FATAL_ERROR "The package is missing: ${required}")
    endif()
  endforeach()

  set(VERIFIED_PATHS "${paths}" PARENT_SCOPE)
  set(VERIFIED_CANDIDATE "${candidate}" PARENT_SCOPE)
  set(VERIFIED_VERSION "${version}" PARENT_SCOPE)
  set(VERIFIED_BUILD_TYPE "${build_type}" PARENT_SCOPE)
  set(VERIFIED_VERIFICATION "${verification}" PARENT_SCOPE)
  set(VERIFIED_HOST_ARCH "${host_arch}" PARENT_SCOPE)
endfunction()

# --- the payload ------------------------------------------------------------

package_verify("${PAYLOAD_DIR}")
set(manifest_paths "${VERIFIED_PATHS}")
set(payload_candidate "${VERIFIED_CANDIDATE}")
set(payload_version "${VERIFIED_VERSION}")
set(payload_build_type "${VERIFIED_BUILD_TYPE}")
set(payload_verification "${VERIFIED_VERIFICATION}")
set(payload_host_arch "${VERIFIED_HOST_ARCH}")

package_safe_name("${CANDIDATE}" "Unsafe candidate: ${CANDIDATE}")

# --- the destination --------------------------------------------------------

file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(name "${ARCHIVE_PREFIX}${CANDIDATE}")
set(final "${OUTPUT_DIR}/${name}")
set(archive "${OUTPUT_DIR}/${name}.tar.gz")
if(EXISTS "${final}" OR EXISTS "${archive}")
  message(FATAL_ERROR "Immutable Release destination already exists: ${final}")
endif()

# Stage under a unique name so a failure leaves no half-written release: the
# archive is built from the staging parent, and only then are the staging tree
# and the archive renamed into place.
string(RANDOM LENGTH 32 ALPHABET "0123456789abcdef" unique)
set(staging_parent "${OUTPUT_DIR}/.incoming-${name}-${unique}")
set(staging "${staging_parent}/${name}")
file(MAKE_DIRECTORY "${staging}")

# Every manifest path, apart from the excluded names and the carried source
# bundle, which is built here.
foreach(relative IN LISTS manifest_paths)
  if(relative MATCHES "${EXCLUDED_PATH}")
    continue()
  endif()
  string(REPLACE "/" ";" parts "${relative}")
  list(GET parts 0 first_part)
  if(first_part STREQUAL CARRIED_SOURCE_BUNDLE)
    continue()
  endif()
  get_filename_component(parent "${staging}/${relative}" DIRECTORY)
  file(MAKE_DIRECTORY "${parent}")
  file(COPY_FILE "${PAYLOAD_DIR}/${relative}" "${staging}/${relative}" ONLY_IF_DIFFERENT)
endforeach()

# The project's own installation document travels with the release, under both
# names the installer and the download page look for.
if(NOT EXISTS "${SOURCE_DIR}/docs/INSTALLATION.md")
  message(FATAL_ERROR "PackageRelease.cmake: there is no docs/INSTALLATION.md to ship")
endif()
file(COPY_FILE "${SOURCE_DIR}/docs/INSTALLATION.md" "${staging}/INSTALLATION.md" ONLY_IF_DIFFERENT)
file(READ "${SOURCE_DIR}/docs/INSTALLATION.md" installation)
file(WRITE "${staging}/README-TEST.md" "${installation}")

# The project's own source tree, which the GPL requires a binary release to
# carry, as one deflate zip of files only, with paths relative to the root.
file(MAKE_DIRECTORY "${staging}/source")
file(GLOB_RECURSE source_entries RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")
set(source_files "")
foreach(entry IN LISTS source_entries)
  if(entry MATCHES "${EXCLUDED_PATH}")
    continue()
  endif()
  # A build tree is a directory whose name starts with build: build, build-release,
  # build-arm64-gnu, and the twenty-odd others a working tree accumulates. Only the
  # components before the last one name directories, which is what keeps a project
  # file such as build.sh in the source bundle.
  get_filename_component(entry_directory "${entry}" DIRECTORY)
  if(entry_directory MATCHES "(^|/)build[^/]*(/|$)")
    continue()
  endif()
  if(IS_SYMLINK "${SOURCE_DIR}/${entry}" OR IS_DIRECTORY "${SOURCE_DIR}/${entry}")
    continue()
  endif()
  list(APPEND source_files "${entry}")
endforeach()
list(SORT source_files)
# The zip format already implies deflate compression, so no COMPRESSION
# argument is given: CMake rejects it for zip archives.
file(ARCHIVE_CREATE OUTPUT "${staging}/${LIBCARRY_ARCHIVE}"
  PATHS ${source_files}
  FORMAT zip
  WORKING_DIRECTORY "${SOURCE_DIR}")

# The release metadata: what this package was built from, and when.
string(TIMESTAMP packaged_at "%Y-%m-%dT%H:%M:%SZ" UTC)
set(metadata "{\n")
string(APPEND metadata "  \"candidate\": \"${CANDIDATE}\",\n")
string(APPEND metadata "  \"version\": \"${payload_version}\",\n")
string(APPEND metadata "  \"build_type\": \"${payload_build_type}\"")
if(payload_host_arch)
  string(APPEND metadata ",\n  \"host_architecture\": true")
endif()
string(APPEND metadata ",\n  \"payload_verification\": ${payload_verification},\n")
string(APPEND metadata "  \"packaged_at_utc\": \"${packaged_at}\",\n")
string(APPEND metadata "  \"payload_origin\": \"${payload_candidate}\",\n")
string(APPEND metadata "  \"installer_format\": 3,\n")
string(APPEND metadata "  \"change\": \"${CHANGE}\"\n")
string(APPEND metadata "}\n")
file(WRITE "${staging}/BUILD_INFO.json" "${metadata}")

# The archive carries executables.
foreach(executable lumaframevr launch.sh)
  file(CHMOD "${staging}/${executable}" PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE)
endforeach()

# Rebuild the manifest over the staged tree, then verify the staged tree again.
file(GLOB_RECURSE staged_entries RELATIVE "${staging}" "${staging}/*")
set(staged_files "")
foreach(entry IN LISTS staged_entries)
  if(NOT IS_DIRECTORY "${staging}/${entry}")
    list(APPEND staged_files "${entry}")
  endif()
endforeach()
list(SORT staged_files)
set(manifest "")
foreach(relative IN LISTS staged_files)
  file(SHA256 "${staging}/${relative}" checksum)
  string(APPEND manifest "${checksum}  ${relative}\n")
endforeach()
file(WRITE "${staging}/SHA256SUMS.txt" "${manifest}")

package_verify("${staging}")

# Archive with a single top level directory, named for the release.
set(archive_incoming "${OUTPUT_DIR}/.${name}.tar.gz.incoming")
file(ARCHIVE_CREATE OUTPUT "${archive_incoming}"
  PATHS "${name}"
  FORMAT gnutar COMPRESSION GZip
  WORKING_DIRECTORY "${staging_parent}")
file(RENAME "${staging}" "${final}")
file(RENAME "${archive_incoming}" "${archive}")
file(REMOVE_RECURSE "${staging_parent}")

file(SHA256 "${archive}" archive_sha)
file(WRITE "${archive}.sha256" "${archive_sha}  ${name}.tar.gz\n")

package_verify("${final}")

message(STATUS "Packaged ${name} as ${archive}")
