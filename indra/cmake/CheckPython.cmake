# -*- cmake -*-
#
# Simply finds the Python interpreter and makes sure
# it is the right version.
include (FindPythonInterp)

if(${PYTHONINTERP_FOUND})
  if(${PYTHON_VERSION_MAJOR} GREATER 2 OR ${PYTHON_VERSION_MINOR} LESS 7)
    message(FATAL_ERROR "Wrong version of Python found. We need 2.7.x to run!")
  endif()
else(${PYTHONINTERP_FOUND})
  message(FATAL_ERROR "Can't build without Python. Bailing!'")
endif(${PYTHONINTERP_FOUND})
