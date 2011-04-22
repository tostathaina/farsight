/* 
 * Copyright 2009 Rensselaer Polytechnic Institute
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 * for more details.
 * 
 * You should have received a copy of the GNU General Public License along 
 * with this program; if not, write to the Free Software Foundation, Inc., 
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*=========================================================================

  Program:   Farsight Biological Image Segmentation and Visualization Toolkit
  Language:  C++
  Date:      $Date:  $
  Version:   $Revision: 0.00 $

=========================================================================*/
#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif
#include "ftkProjectProcessor.h"

#include <time.h>

namespace ftk
{

//Constructor
ProjectProcessor::ProjectProcessor()
{
	inputImage = NULL;
	outputImage = NULL;
	definition = NULL;
	tasks.clear();
	table = NULL;
	NucAdjTable = NULL;
	CellAdjTable = NULL;
	numTasks = 0;
	lastTask = -1;
	resultIsEditable = false;
	inputTypeNeeded = 0;
	save_path = ".";
}

void ProjectProcessor::Initialize(void)
{
	if(!definition) return;

	tasks.clear();

	for(int i=0; i<(int)definition->pipeline.size(); ++i)
	{
		Task t;
		t.type = definition->pipeline.at(i);
		switch(t.type)
		{
		case ProjectDefinition::PREPROCESSING:
			break;
		case ProjectDefinition::NUCLEAR_SEGMENTATION:
			t.inputChannel1 = definition->FindInputChannel("NUCLEAR");
			break;
		case ProjectDefinition::FEATURE_COMPUTATION:
			t.inputChannel1 = definition->FindInputChannel("NUCLEAR");
			break;
		case ProjectDefinition::CYTOPLASM_SEGMENTATION:
			t.inputChannel2 = definition->FindInputChannel("CYTOPLASM");
			t.inputChannel3 = definition->FindInputChannel("MEMBRANE");
			break;
		case ProjectDefinition::RAW_ASSOCIATIONS:
			break;
		case ProjectDefinition::CLASSIFY:
			break;
		case ProjectDefinition::ANALYTE_MEASUREMENTS:
			break;
		case ProjectDefinition::PIXEL_ANALYSIS:
			break;
		case ProjectDefinition::QUERY:
			break;
		}
		t.done = false;
		tasks.push_back(t);
	}

	numTasks = (int)tasks.size();
	std::cout<<"Number of tasks to be done:"<<numTasks<<std::endl;
	lastTask = -1;
}

void ProjectProcessor::ProcessNext(void)
{
	if(DoneProcessing()) return;

	int thisTask = lastTask + 1;
	bool taskDone = false;
	switch(tasks.at(thisTask).type)
	{
	case ProjectDefinition::PREPROCESSING:
		taskDone = PreprocessImage();
		break;
	case ProjectDefinition::NUCLEAR_SEGMENTATION:
		taskDone = SegmentNuclei(tasks.at(thisTask).inputChannel1);
		break;
	case ProjectDefinition::FEATURE_COMPUTATION:
		taskDone = ComputeFeatures(tasks.at(thisTask).inputChannel1);
		break;
	case ProjectDefinition::CYTOPLASM_SEGMENTATION:
		taskDone = SegmentCytoplasm(tasks.at(thisTask).inputChannel2, tasks.at(thisTask).inputChannel3);
		break;
	case ProjectDefinition::RAW_ASSOCIATIONS:
		taskDone = ComputeAssociations();
		break;
	case ProjectDefinition::ANALYTE_MEASUREMENTS:
		taskDone = false;
		break;
	case ProjectDefinition::CLASSIFY:
		taskDone = Classify();
		break;
	case ProjectDefinition::PIXEL_ANALYSIS:
		taskDone = PixLevAnalysis();
		break;
	case ProjectDefinition::QUERY:
		taskDone = RunQuery();
		break;
	}

	if(taskDone)
	{
		tasks.at(thisTask).done = true;
		lastTask++;
	}
}

bool ProjectProcessor::PreprocessImage(){

	if(!inputImage)
		return false;

	if(definition->preprocessingParameters.size() == 0)
	{
		inputTypeNeeded = 3;
		return false;
	}

	for(std::vector<ftk::ProjectDefinition::preprocessParam>::iterator ppit=definition->preprocessingParameters.begin(); ppit!=definition->preprocessingParameters.end(); ++ppit ){
		int chNum = definition->FindInputChannel( ppit->channelName );
		if( chNum == -1 ){
			std::cerr<<"ERROR: Cannot find the channel "<<ppit->channelName<<" for preprocessing\n";
			continue;
		}
		ftk::Preprocess *prep = new ftk::Preprocess( inputImage->GetItkPtr<unsigned char>(0,chNum) );
		TiXmlDocument tempXML;   
		TiXmlElement *genericName = new TiXmlElement( "Preprocess" );
		tempXML.LinkEndChild( genericName );
		//Write Filter:
		TiXmlElement * filterElement = new TiXmlElement(ppit->filterName.c_str());
		if( !ppit->paramenter1.empty() ){
			filterElement->SetAttribute( ppit->paramenter1.c_str(), ftk::NumToString( ppit->value1 ) );
		}
		if( !ppit->paramenter2.empty() ){
			filterElement->SetAttribute( ppit->paramenter2.c_str(), ftk::NumToString( ppit->value2 ) );
		}
		if( !ppit->paramenter3.empty() ){
			filterElement->SetAttribute( ppit->paramenter3.c_str(), ftk::NumToString( ppit->value3 ) );
		}
		if( !ppit->paramenter4.empty() ){
			filterElement->SetAttribute( ppit->paramenter4.c_str(), ftk::NumToString( ppit->value4 ) );
		}
		if( !ppit->paramenter5.empty() ){
			filterElement->SetAttribute( ppit->paramenter5.c_str(), ftk::NumToString( ppit->value5 ) );
		}
		if( !ppit->paramenter6.empty() ){
			filterElement->SetAttribute( ppit->paramenter6.c_str(), ftk::NumToString( ppit->value6 ) );
		}
		genericName->LinkEndChild(filterElement);
		std::string tempFilename;
		tempFilename = save_path + ppit->channelName + ppit->filterName;
		tempFilename.append( ".xml" );
		tempXML.SaveFile( tempFilename.c_str() );
		prep->RunPipe( tempFilename );
		const ftk::Image::Info * Iinfo = inputImage->GetImageInfo();
		int byte_to_cpy = Iinfo->numZSlices * Iinfo->numRows * Iinfo->numColumns * Iinfo->bytesPerPix;
		memcpy( inputImage->GetDataPtr(0,chNum), prep->GetImage()->GetBufferPointer(), byte_to_cpy );
		delete prep;
	}
	return true;
}

bool ProjectProcessor::SegmentNuclei(int nucChannel)
{
	clock_t start_time = clock();
	if(!inputImage)
		return false;

	ftk::NuclearSegmentation * nucSeg = new ftk::NuclearSegmentation();
	nucSeg->SetInput(inputImage, "data_image", nucChannel);

	//Setup the Parameters:
	bool finalize = false;	//The Default
	for(int i=0; i<(int)definition->nuclearParameters.size(); ++i)
	{
		nucSeg->SetParameter(definition->nuclearParameters.at(i).name, int(definition->nuclearParameters.at(i).value));
		if(definition->nuclearParameters.at(i).name == "finalize_segmentation")
		{
			if(definition->nuclearParameters.at(i).value == 1)
				finalize = true;
		}
	}

	//Process:
	nucSeg->Binarize(false);
	nucSeg->DetectSeeds(false);
	if(finalize)
	{
		nucSeg->RunClustering(false);
		nucSeg->Finalize();
	}
	else
	{
		nucSeg->RunClustering(true);
	}
	nucSeg->ReleaseSegMemory();
	outputImage = nucSeg->GetLabelImage();

	//Update For params actually used:
	definition->nuclearParameters.clear();
	std::vector<std::string> paramNames = nucSeg->GetParameterNames();
	for(int i=0; i<(int)paramNames.size(); ++i)
	{	
		ProjectDefinition::Parameter p;
		p.name = paramNames.at(i);
		p.value = nucSeg->GetParameter(p.name);
		definition->nuclearParameters.push_back(p);
	}

	delete nucSeg;
	cout << "Total time to segmentation is : " << (clock() - start_time)/(float) CLOCKS_PER_SEC << endl;
	std::cout << "Done Nucleus Segmentation\nComputing intrisic features for the nuclei\n";

	//Calc Features:
	ftk::IntrinsicFeatureCalculator *iCalc = new ftk::IntrinsicFeatureCalculator();
	iCalc->SetInputImages(inputImage,outputImage,nucChannel,0);
	if(definition->intrinsicFeatures.size() > 0)
		iCalc->SetFeaturesOn( GetOnIntrinsicFeatures() );
	//iCalc->SetFeaturePrefix("nuc_");
	table = iCalc->Compute();									//Create a new table
	delete iCalc;

	FTKgraph* NucRAG = new FTKgraph();
	NucAdjTable = NucRAG->AdjacencyGraph_All(inputImage->GetItkPtr<IPixelT>(0,nucChannel), outputImage->GetItkPtr<LPixelT>(0,0));

	std::cout << "Done: Instrinsic Nuclear Features\n";

	resultIsEditable = true;
	
	return true;
}



bool ProjectProcessor::ComputeFeatures(int nucChannel)
{
	ftk::IntrinsicFeatureCalculator *iCalc = new ftk::IntrinsicFeatureCalculator();
	iCalc->SetInputImages(inputImage,outputImage,nucChannel,0);
	if(definition->intrinsicFeatures.size() > 0)
		iCalc->SetFeaturesOn( GetOnIntrinsicFeatures() );
	//iCalc->SetFeaturePrefix("nuc_");
	table = iCalc->Compute();									//Create a new table
	delete iCalc;
	std::cout << "Done: Instrinsic Nuclear Features\n";
	resultIsEditable = true;
	return true;
}


bool ProjectProcessor::SegmentCytoplasm(int cytChannel, int memChannel)
{
	if(!inputImage || !outputImage || !table)
		return false;

	std::cout << "Begin Cytoplasm Segmentation\n";

	ftk::CytoplasmSegmentation * cytoSeg = new ftk::CytoplasmSegmentation();
	cytoSeg->SetDataInput(inputImage, "data_image", cytChannel,memChannel);
	cytoSeg->SetNucleiInput(outputImage, "label_image");		//Will append the result to outputImage

	for(int i=0; i<(int)definition->cytoplasmParameters.size(); ++i)
		cytoSeg->SetParameter(definition->cytoplasmParameters.at(i).name, int(definition->cytoplasmParameters.at(i).value));

	cytoSeg->Run();

	definition->cytoplasmParameters.clear();
	std::vector<std::string> paramNames = cytoSeg->GetParameterNames();
	for(int i=0; i<(int)paramNames.size(); ++i)
	{	
		ProjectDefinition::Parameter p;
		p.name = paramNames.at(i);
		p.value = cytoSeg->GetParameter(p.name);
		definition->cytoplasmParameters.push_back(p);
	}


	delete cytoSeg;

	std::cout << "Done: Cytoplasm Segmentation\nComputing intrisic features for the whole cell\n";

	//Calc Features:
	if( cytChannel > -1 ){
		ftk::IntrinsicFeatureCalculator *iCalc = new ftk::IntrinsicFeatureCalculator();
		iCalc->SetInputImages(inputImage,outputImage,cytChannel,1);
		if(definition->intrinsicFeatures.size() > 0)
			iCalc->SetFeaturesOn( GetOnIntrinsicFeatures() );
		iCalc->SetFeaturePrefix("cyto_");
		iCalc->Append(table); //Append features to the table
		delete iCalc;
	}

	FTKgraph* CellRAG = new FTKgraph();
	CellAdjTable = CellRAG->AdjacencyGraph_All(inputImage->GetItkPtr<IPixelT>(0,cytChannel), outputImage->GetItkPtr<LPixelT>(0,1), true);


	std::cout << "Done: Intrisic features for the whole cell\n";

	resultIsEditable = false;
	return true;
}

bool ProjectProcessor::ComputeAssociations(void)
{
	if(!inputImage || !outputImage || !table)
		return false;

	if(definition->associationRules.size() == 0)
	{
		inputTypeNeeded = 3;
		return false;
	}

	for(std::vector<ftk::AssociationRule>::iterator ascit=definition->associationRules.begin(); ascit!=definition->associationRules.end(); ++ascit ){

		int seg_channel_number=-1;
		int inp_channel_number=-1;
		if( strcmp( ascit->GetSegmentationFileName().c_str(), "NUCLEAR" ) == 0 ){
			if( !outputImage->GetImageInfo()->numChannels ){
				std::cout<<"Unable to access labeled nuclear image while computing associative features\n";
				return false;
			}
			seg_channel_number = 0;
			
		} else if( strcmp( ascit->GetSegmentationFileName().c_str(), "CYTOPLASM" ) == 0 ){
			if( outputImage->GetImageInfo()->numChannels < 2 ){
				std::cout<<"Unable to access labeled cytoplasmic image while computing associative features\n";
				return false;
			}
			seg_channel_number = 1;
		} else{
			std::cout<<"Please check region type for associative feature computation\n";
			return false;
		}

		for( int j=0; j<(int)inputImage->GetImageInfo()->channelNames.size(); ++j )
			if( strcmp( inputImage->GetImageInfo()->channelNames.at(j).c_str(), ascit->GetTargetFileNmae().c_str() ) == 0 ){
				inp_channel_number=j;
				break;
			}

		if( inp_channel_number == -1 ){
			std::cout<<"Unable to access grayscale image while computing associative feature: "<<ascit->GetRuleName()<<std::endl;
			return false;
		}

		(*ascit).set_path( save_path );

		ftk::AssociativeFeatureCalculator * assocCal = new ftk::AssociativeFeatureCalculator();
		assocCal->SetInputs(inputImage, inp_channel_number, outputImage, seg_channel_number, &(*ascit) );
		if(table){
			assocCal->Append(table);
		}
		delete assocCal;
	}

	resultIsEditable = false;
	std::cout << "Done Associations\n";
	return true;
}

bool ProjectProcessor::PixLevAnalysis(void){
	if( !inputImage )
		return false;

	if( !definition->pixelLevelRules.size() )
		return false;

	bool success_run=false;
	for(std::vector<ftk::PixelAnalysisDefinitions>::iterator pait=definition->pixelLevelRules.begin(); pait!=definition->pixelLevelRules.end(); ++pait ){
		ftk::PixelLevelAnalysis *PAn = new ftk::PixelLevelAnalysis();
		if( (*pait).mode == 1 ){
			PAn->SetInputs( (*pait).regionChannelName, (*pait).targetChannelName, (*pait).outputFilename, 0 );
			success_run = PAn->RunAnalysis1();
		}
		else if( (*pait).mode == 2 ){
			PAn->SetInputs( (*pait).regionChannelName, (*pait).targetChannelName, (*pait).outputFilename, (*pait).radius );
			success_run = PAn->RunAnalysis2();
		}
		else if( (*pait).mode == 3 ){
			PAn->SetInputs( (*pait).regionChannelName, (*pait).targetChannelName, (*pait).outputFilename, (*pait).radius );
			success_run = PAn->RunAnalysis3();
		}
		else if( !success_run ){
			std::cerr<<"ERROR: Run Failed, Check Definitions\n";
		}
		else{
			std::cerr<<"ERROR: Check Pixel Anaysis Mode\n";
		}
		delete PAn;
	}
	return success_run;
}

std::set<int> ProjectProcessor::GetOnIntrinsicFeatures(void)
{
	std::set<int> retSet;

	for(int f=0; f<IntrinsicFeatures::N; ++f)
	{
		std::string name = IntrinsicFeatures::Info[f].name;
		for(int p=0; p<(int)definition->intrinsicFeatures.size(); ++p)
		{
			if( definition->intrinsicFeatures.at(p) == name )
			{
				retSet.insert(f);
			}
		}
	}
	return retSet;
}

bool ProjectProcessor::Classify(void){
	std::cout<<"Entered Classification step\n";
	if(!table)
		return false;
	std::cout<<"Table present strating classification\n";
	TrainingDialogNoGUI *d = new TrainingDialogNoGUI(table);
	d->loadModelFromFile1(definition->classificationTrainingData);
	delete d;
	std::cout<<"Model Loaded\n";
	for(int j=0; j<(int)definition->classificationParameters.size(); ++j){
		bool training_col_found = false;
		std::vector<int> KPLsColumnsToUse;
		std::string output_col_name;
		for( int i=0; i<table->GetNumberOfColumns(); ++i ){
			std::string current_column;
			current_column = table->GetColumnName(i);
			if( strcmp (current_column.c_str(),definition->classificationParameters.at(j).TrainingColumn.c_str()) == 0 ){
				std::string::iterator it;
				it=current_column.begin();
				current_column.erase ( current_column.begin(), current_column.begin()+6 );
				output_col_name = "prediction_" + current_column;
				training_col_found = true;
			}
			else{
				for( int k=0; k<(int)definition->classificationParameters.at(j).ClassificationColumns.size(); ++k ){
					if( strcmp (current_column.c_str(),definition->classificationParameters.at(j).ClassificationColumns.at(k).c_str()) == 0 ){
						KPLsColumnsToUse.push_back( i );
					}
				}
			}
		}
		std::cout<<"Running classifier:"<<j+1<<"\n";
		if( training_col_found && !KPLsColumnsToUse.empty() ){
			PatternAnalysisWizardNoGUI *p = new PatternAnalysisWizardNoGUI(this->table,definition->classificationParameters.at(j).TrainingColumn.c_str(), output_col_name.c_str() );
			p->KPLSrun1(KPLsColumnsToUse);
			delete p;
		}
		else{
			std::cerr<<"Check classification parameter:"<<j<<std::endl;
		}
	}
	return true;
}


/*void ProjectProcessor::sqliteCloseConnection(sqlite3 *dbConn)
{   
	//  sqlite3_close() routine is the destructor for the sqlite3 object. 
	//  Calls to sqlite3_close() return SQLITE_OK if the sqlite3 object is successfullly destroyed 
	//  and all associated resources are deallocated.
	
	sqlite3_close(dbConn);
	fprintf(stderr,"Execution complete!\n");

}
*/

bool ProjectProcessor::RunQuery(void)
{
	int sql_db_img_id=-1;
	for(int j=0; j<(int)definition->queryParameters.size(); ++j){
		sqlite3 *dbConn;
	
		//  This sql string can be passed to this function from any UI module in Farsight.
		//  1) Open connection
		//  2) Execute query
		//  3) Dynamic binding od results to UI
		//  4) Close connection.

		char * sql;
    
		//   int sqlite3_open(
		//   const char *filename,  Database filename (UTF-8) 
		//   sqlite3 **ppDb         OUT: SQLite db handle )
		std::string db_name = executable_path + "\\NE.s3db";
		std::cout<<"Opening database: "<<db_name<<std::endl;
		dbConn = ftk::sqliteGetConnection( db_name.c_str() );
		//dbConn = ftk::sqliteOpenConnection();
		if( dbConn ){
			if(j==0){
				std::vector<std::string> column_names;
				//std::string temp1,temp2;
				//temp1 = "IMG_ID"; temp2 = "CELL_ID";
				//column_names.push_back( temp1 );
				//column_names.push_back( temp2 );
					for (int col = 1; col< table->GetNumberOfColumns(); ++col){
					std::string temp3=table->GetColumnName(col);
					column_names.push_back(temp3);
				}
				std::string table_name;
				table_name = "IMAGE_TEST";
				ftk::checkForUpdate( dbConn, column_names );

				std::string image_name;
				image_name = save_path + definition->name;
				char *im_nm_cstr = new char [image_name.size()+1];
				strcpy (im_nm_cstr, image_name.c_str());
				char *path_nm_cstr = new char [save_path.size()+1];
				strcpy (path_nm_cstr, save_path.c_str());
				std::vector< double > table_array;
				for (int row = 0; row< table->GetNumberOfRows(); ++row){
					for (int col = 0; col< table->GetNumberOfColumns(); ++col){
						table_array.push_back(table->GetValue(row,col).ToDouble());
					}
				}
				sql_db_img_id = ftk::GenericInsert( dbConn, im_nm_cstr, table_name.c_str(), path_nm_cstr, table_array,table->GetNumberOfColumns(), table->GetNumberOfRows(), column_names );	

				//sql = "select * from IMAGE;";
				//sql = "select * from IMAGE_TEST where IMG_ID = 1;";
				//sql = "select * from IMAGE where IMG_ID = 1;";
				//sql = "select * from IMAGE_TEST where IMG_ID = 1 and CELL_ID = 2;";
				//sql = "select * from IMAGE where IMG_ID = 2 and cell_id = 3 and eccentricity = 2.24 ;";
			}
			if( strcmp( definition->queryParameters.at(j).name.c_str(), "count" ) == 0 ){
				std::string temp = "SELECT COUNT IMG_ID FROM ( " + definition->queryParameters.at(j).value + " AND IMG_ID = " + ftk::NumToString(sql_db_img_id) + ");";
				sql = new char [temp.size()+1];
				strcpy (sql, temp.c_str());
			}
			else{
				std::string temp = definition->queryParameters.at(j).value + " AND IMG_ID = " + ftk::NumToString(sql_db_img_id) + ";";
					sql = new char [temp.size()+1];
				strcpy (sql, temp.c_str());
			}

			std::cout<<sql<<"\t---this is my query" <<std::endl;

			//Generic API.Can be called from any module
			ftk::sqliteExecuteQuery2(dbConn,sql,save_path,j+1);

			//Close the connetion.Free allocated resources
			ftk::sqliteCloseConnection(dbConn);
			return true;
		}
	}
	return false;
}


//************************************************************************
//************************************************************************
//************************************************************************
}  // end namespace ftk
