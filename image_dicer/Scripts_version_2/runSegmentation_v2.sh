#!/bin/bash
echo $FARSIGHT_BIN_EXE
$FARSIGHT_BIN_EXE/darpa_tracer_w_seg_v2 $DAPI_LOCAL\_BS.nrrd $GFP_LOCAL\_BS_CV.mhd $Cy5_LOCAL\_BS.nrrd $LOCAL_PARAMETERS_PATH/Seg_Params.ini $LOCAL_PARAMETERS_PATH/ProjectDefinition.xml 0 $SMALLIMAGE