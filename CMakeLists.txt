cmake_minimum_required(VERSION 3.19)

# Указываем файл тулчейна (настройки компилятора из вашей папки cmake)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gcc-riscv-none-elf.cmake)

# Имя проекта на основе названия папки
get_filename_component(BASE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PROJECT_NAME ${BASE_FOLDER})

# Инициализация проекта с поддержкой C, C++ и Ассемблера
project(${PROJECT_NAME} C CXX ASM)

# Стандарт C/C++ (в makefile используется gnu11 для C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON) # Включает расширения gnu (gnu11)
set(CMAKE_CXX_STANDARD 17)

# ==============================================================================
# Выбор конфигурации сборки (Аналог переменных MCU и CONFIG в makefile)
# ==============================================================================
# Переменные кэша позволяют менять их через CMake GUI или параметры командной строки (-DMCU=...)
set(MCU "MDR1206" CACHE STRING "Target MCU: MDR1206 or MDR32F02")
set(CONFIG "FLASH" CACHE STRING "Build Configuration: FLASH, RAM_TCM_debug, or RAM_TCM_release")

# Настройки в зависимости от выбранного микроконтроллера
if(MCU STREQUAL "MDR1206")
    set(ARCH_FLAGS "-march=rv32imc_zicsr_zifencei_zicntr_zba_zbb_zbc_zbs_zbkb_zbkc_zbkx_zknd_zkne_zknh_zksed_zksh")
    set(LD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Library/CMSIS/DeviceSupport/src/gcc/MDR1206")
    set(STARTUP_FILE "${LD_DIR}/startup_MDR1206.S")
elseif(MCU STREQUAL "MDR32F02")
    set(ARCH_FLAGS "-march=rv32imc_zicsr_zifencei_zicntr")
    set(LD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Library/CMSIS/DeviceSupport/src/gcc/MDR32F02")
    set(STARTUP_FILE "${LD_DIR}/startup_MDR32F02.S")
else()
    message(FATAL_ERROR "Unknown MCU: ${MCU}. Should be MDR1206 or MDR32F02")
endif()

# Выбор linker скрипта в зависимости от конфигурации
if(CONFIG STREQUAL "FLASH")
    set(LINKER_SCRIPT "link_FLASH.ld")
elseif(CONFIG STREQUAL "RAM_TCM_debug")
    set(LINKER_SCRIPT "link_RAM_TCM_debug.ld")
elseif(CONFIG STREQUAL "RAM_TCM_release")
    set(LINKER_SCRIPT "link_RAM_TCM_release.ld")
else()
    message(FATAL_ERROR "Unknown CONFIG: ${CONFIG}. Should be FLASH, RAM_TCM_debug, or RAM_TCM_release")
endif()

# ==============================================================================
# Исходные файлы
# ==============================================================================
# Автоматически находим общие исходники библиотек (C и Asm)
# Важно: мы не используем GLOB на всю папку gcc/ или iar/, чтобы избежать дублирования
file(GLOB_RECURSE LIB_C_SRCS
    "Library/CMSIS/CoreSupport/src/*.c"
    "Library/CMSIS/DeviceSupport/src/*.c"
    "Library/SPL/src/*.c"
)

file(GLOB_RECURSE LIB_ASM_SRCS
    "Library/SPL/src/*.S"
    "Library/SPL/src/*.s"
)

file(GLOB_RECURSE C_CPP_SRCS
    "src/*.c"
    "src/*.cpp"
)

# Создание главного исполняемого файла
# Включаем main.cpp, файлы библиотек и специфичный для выбранного МК startup-файл
add_executable(${PROJECT_NAME}
    ${C_CPP_SRCS}
    ${LIB_C_SRCS}
    ${LIB_ASM_SRCS}
    ${STARTUP_FILE}
)

# ==============================================================================
# Директории с заголовочными файлами (Аналог INC_DIRS)
# ==============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Library/CMSIS/DeviceSupport
    ${CMAKE_CURRENT_SOURCE_DIR}/Library/CMSIS/DeviceSupport/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Library/CMSIS/CoreSupport/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Library/SPL/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/Library/SPL
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ==============================================================================
# Флаги компиляции (Аналог COMMON_FLAGS и CFLAGS)
# ==============================================================================
# Общие флаги для всех языков (C, C++, ASM)
target_compile_options(${PROJECT_NAME} PRIVATE
    ${ARCH_FLAGS}
    -mabi=ilp32
    -mcmodel=medany
    -msmall-data-limit=8
    -mno-save-restore
    -Og
    -g3
    -fmessage-length=0
    -fsigned-char
    -ffunction-sections
    -fdata-sections
    -fno-move-loop-invariants
    -Wall
    -Wextra
    -Wunused
    -Wuninitialized
    -Wconversion
    -Wpointer-arith
    -Wshadow
    -Wlogical-op
    -Wfloat-equal
)

# Флаги, специфичные ТОЛЬКО для языка C (чтобы не ломать компиляцию main.cpp)
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:
        -Wmissing-declarations
        -Waggregate-return
        -Wmissing-prototypes
        -Wstrict-prototypes
        -Wbad-function-cast
    >
)

# Специфичные флаги для ассемблера
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<COMPILE_LANGUAGE:ASM>:-x assembler-with-cpp>
)

# Определения препроцессора (как в вашем старом CMake)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    HSECLK_VAL=8000000
    SYSCLK_PLL
    CKO_PLL0
)

# ==============================================================================
# Настройки линковщика (Аналог LDFLAGS)
# ==============================================================================
# Добавляем путь к linker скрипту (аналог -L"Library/.../gcc/MDR1206")
target_link_directories(${PROJECT_NAME} PRIVATE ${LD_DIR})

target_link_options(${PROJECT_NAME} PRIVATE
    ${ARCH_FLAGS}
    -mabi=ilp32
    -nostartfiles
    -T${LINKER_SCRIPT}
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--gc-sections
    -Wl,--enable-non-contiguous-regions
    # Опционально: скрывает лишние варнинги о секциях в новых версиях GCC
    -Wl,--no-warn-rwx-segments
    -Wl,--print-memory-usage
)

# Подключение базовых библиотек (nano и nosys определены в тулчейне gcc-riscv-none-elf.cmake)
# Это добавляет --specs=nano.specs (и nosys.specs для заглушек системных вызовов)
target_link_libraries(${PROJECT_NAME}
    MDR32::NoSys
    MDR32::Nano
)

# ==============================================================================
# Создание дополнительных артефактов сборки (.hex, .bin, .lss)
# ==============================================================================
# Функции берутся из файла тулчейна cmake/gcc-riscv-none-elf.cmake
mdr32_generate_binary_file(${PROJECT_NAME})
mdr32_generate_hex_file(${PROJECT_NAME})
mdr32_generate_lss_file(${PROJECT_NAME})
mdr32_print_size_of_target(${PROJECT_NAME})
