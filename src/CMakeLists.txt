include(CheckIncludeFile)
CHECK_INCLUDE_FILE("link.h" HAVE_LINK)
CHECK_INCLUDE_FILE("bfd.h" HAVE_BFD)

if(NOT (HAVE_LINK AND HAVE_BFD))
    message(WARNING "link.h and bfd.h not found, the stacktrace functionality will not be available. Install the binutils-dev package to fix this.")
endif(NOT (HAVE_LINK AND HAVE_BFD))

configure_file(
    "Teuchos_config.h.in"
    "Teuchos_config.h"
    )


# These are all Teuchos files, we only uncommented the ones implementing RCP:
SET(SOURCES
  #Teuchos_BLAS.cpp
  #Teuchos_CTimeMonitor.cpp
  #Teuchos_CommandLineProcessor.cpp
  #Teuchos_CompObject.cpp
  #Teuchos_Describable.cpp
  #Teuchos_ErrorPolling.cpp
  #Teuchos_Flops.cpp
  #Teuchos_GlobalMPISession.cpp
  #Teuchos_LabeledObject.cpp
  #Teuchos_MPIComm.cpp
  #Teuchos_MPISession.cpp
  #Teuchos_Object.cpp
  #Teuchos_ParameterEntry.cpp
  #Teuchos_ParameterList.cpp
  #Teuchos_ParameterListAcceptorHelpers.cpp
  #Teuchos_ParameterListAcceptor.cpp
  #Teuchos_ParameterListAcceptorDefaultBase.cpp
  #Teuchos_ParameterListNonAcceptor.cpp
  #Teuchos_PerformanceMonitorUtils.cpp
  Teuchos_Ptr.cpp
  Teuchos_RCPNode.cpp
  #Teuchos_Range1D.cpp
  #Teuchos_ScalarTraits.cpp
  #Teuchos_StandardParameterEntryValidators.cpp
  #Teuchos_StringToIntMap.cpp
  #Teuchos_TableColumn.cpp
  #Teuchos_TableEntry.cpp
  #Teuchos_TableFormat.cpp
  #Teuchos_TabularOutputter.cpp
  Teuchos_TestForException.cpp
  #Teuchos_TestingHelpers.cpp
  #Teuchos_Time.cpp
  #Teuchos_TimeMonitor.cpp
  Teuchos_TypeNameTraits.cpp
  #Teuchos_UnitTestBase.cpp
  #Teuchos_UnitTestRepository.cpp
  #Teuchos_Utils.cpp
  #Teuchos_VerboseObject.cpp
  #Teuchos_VerboseObjectParameterListHelpers.cpp
  #Teuchos_VerbosityLevel.cpp
  #Teuchos_VerbosityLevelCommandLineProcessorHelpers.cpp
  #Teuchos_Workspace.cpp
  Teuchos_dyn_cast.cpp

  Teuchos_stacktrace.cpp
  )

ADD_LIBRARY(
  teuchosmm
  SHARED
  ${SOURCES}
)

target_link_libraries(teuchosmm iberty bfd)
