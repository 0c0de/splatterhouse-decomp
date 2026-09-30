# Guard de codegen para bootstrap sin XEX.
# Local patch sobre generated/rexglue.cmake: si el XEX no esta presente,
# se omite codegen (proyecto bootstrap) en lugar de fallar el build entero.
# Argumentos: -DMANIFEST= -DXEX= -DCODEGEN_D= -DREXGLUE_EXE=

if(NOT EXISTS "${XEX}")
  message(STATUS "[splatterhouse] XEX no presente: ${XEX} - codegen omitido (modo bootstrap).")
  # Depfile vacio para que el transform posterior no falle.
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../generated")
  file(WRITE "${CODEGEN_D}" "")
else()
  execute_process(COMMAND "${REXGLUE_EXE}" codegen "${MANIFEST}" RESULT_VARIABLE res)
  if(NOT res EQUAL 0)
    message(FATAL_ERROR "[splatterhouse] rexglue codegen fallo (usa --force en rexglue para continuar a pesar de errores de validacion)")
  endif()
endif()
