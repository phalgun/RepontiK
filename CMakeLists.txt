project(resourcebrowser)

find_package(KDE4 REQUIRED)
find_package(Nepomuk REQUIRED)
include(MacroLibrary)
include(SopranoAddOntology)
include (KDE4Defaults)

include_directories(
  ${QT_INCLUDES}
  ${KDE4_INCLUDES}
  ${SOPRANO_INCLUDE_DIR}
  ${NEPOMUK_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}
  ${NEPOMUK_INCLUDE_DIR}
  )

set( SRCS
  main.cpp
  resourcebrowser.cpp
  linkresourcedialog.cpp
  removeduplicates.cpp
)

#kde4_add_ui_files(SRCS
	#browserview.ui

	#   )
kde4_add_executable(resourcebrowser ${SRCS})


target_link_libraries( resourcebrowser
  nepomukutils
  ${KDE4_KDEUI_LIBS}
  ${KDE4_KIO_LIBS}
  ${NEPOMUK_LIBRARIES}
  ${NEPOMUK_QUERY_LIBRARIES}
  ${SOPRANO_LIBRARIES}
  )
install(TARGETS resourcebrowser
	${INSTALL_TARGETS_DEFAULT_ARGS})

#install(FILES resourcebrowser.desktop DESTINATION ${SERVICES_INSTALL_DIR})

#install(PROGRAMS resourcebrowser.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
