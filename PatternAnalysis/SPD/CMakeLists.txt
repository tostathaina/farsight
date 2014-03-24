SET( Libraries
    ${VXL_LIBRARIES}
    ${VTK_LIBRARIES}
    ${ITK_LIBRARIES}
    vcl
    vnl
    mbl
    ${QT_LIBRARIES})

QT4_WRAP_CPP( GUI_MOC_SRCS SPDMSTModuleMatch.h SPDkNNGModuleMatch.h TrendHeatmapWindow.h HeatmapWindowForNewSelection.h OrderedHeatmapWindow.h)

SET(SPD_SRCS
	SPDAnalysisModel.h
	SPDAnalysisModel.cpp
	transportSimplex.h
	OrderedHeatmapWindow.h
	OrderedHeatmapWindow.cpp
	HeatmapWindowForNewSelection.h
	HeatmapWindowForNewSelection.cpp
	TrendHeatmapWindow.h
	TrendHeatmapWindow.cpp
	SPDkNNGModuleMatch.h
	SPDkNNGModuleMatch.cpp
	SPDMSTModuleMatch.h
	SPDMSTModuleMatch.cpp
	clusclus.cpp
	clusclus.h)

IF ( NOT BUILD_CLUSCLUS )
	MESSAGE( SEND_ERROR "Must turn on BUILD_CLUSCLUS" )
ENDIF ( NOT BUILD_CLUSCLUS )
	
ADD_LIBRARY( SPD ${SPD_SRCS} ${GUI_MOC_SRCS})
TARGET_LINK_LIBRARIES(SPD ${Libraries} ftkGUI ftkCommon)