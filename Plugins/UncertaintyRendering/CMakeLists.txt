INCLUDE_DIRECTORIES(
  ${VTK_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  )

IF (PARAVIEW_BUILD_QT_GUI)
  SET(SM_SOURCES
    vtkUncertaintySurfacePainter.cxx
    vtkUncertaintySurfaceDefaultPainter.cxx
    vtkUncertaintySurfaceRepresentation.cxx
    vtkSMUncertaintySurfaceRepresentationProxy.cxx
  )

  SET(SM_XMLS
    vtkUncertaintyRendering.xml
    vtkUncertaintyRenderingExtension.xml
  )

  encode_files_as_strings(SHADER_SOURCES
    vtkUncertaintySurfacePainter_vs.glsl
    vtkUncertaintySurfacePainter_fs.glsl
  )

  ADD_PARAVIEW_PLUGIN(UncertaintyRendering "1.0"
    SERVER_MANAGER_XML
      ${SM_XMLS}

    SERVER_MANAGER_SOURCES
      ${SM_SOURCES}
      ${SHADER_SOURCES}
  )

  # add dependency with required vtk modules.
  target_link_libraries(UncertaintyRendering vtkRenderingParallel)
ENDIF()

if(BUILD_TESTING)
  add_subdirectory(Testing)
endif(BUILD_TESTING)
