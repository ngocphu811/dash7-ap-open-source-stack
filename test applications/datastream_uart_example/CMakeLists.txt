INCLUDE_DIRECTORIES(${D7AOSS_SOURCE_DIR}/d7aoss/)
INCLUDE_DIRECTORIES(${D7AOSS_SOURCE_DIR}/d7aoss/hal)
INCLUDE_DIRECTORIES(${D7AOSS_SOURCE_DIR}/d7aoss/dll)

ADD_EXECUTABLE(datastream_uart_example_example main.c)
TARGET_LINK_LIBRARIES(datastream_uart_example_example D7AOSS PHY DLL NWL TRANS)

