MACRO(ADD_PCH_TO_TARGET PrecompiledHeader PrecompiledSource TargetVar )
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)
    SET(PrecompiledBinaryCmake "$(IntDir)/${PrecompiledBasename}.pch")

    file(TO_NATIVE_PATH "${PrecompiledBinaryCmake}" PrecompiledBinary)

    target_sources(${TargetVar} PRIVATE "${PrecompiledHeader}" "${PrecompiledSource}")
    set_source_files_properties(${PrecompiledSource}
                                PROPERTIES COMPILE_OPTIONS "/Yc${PrecompiledHeader};/Fp${PrecompiledBinary}")
    set_target_properties(${TargetVar}
                                PROPERTIES COMPILE_OPTIONS "/Yu${PrecompiledHeader};/FI${PrecompiledHeader};/Fp${PrecompiledBinary}")
  ENDIF(MSVC)
ENDMACRO(ADD_PCH_TO_TARGET)