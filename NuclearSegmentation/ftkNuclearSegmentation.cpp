/*=========================================================================
Copyright 2009 Rensselaer Polytechnic Institute
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 
=========================================================================*/

/*=========================================================================

  Program:   Farsight Biological Image Segmentation and Visualization Toolkit
  Language:  C++
  Date:      $Date:  $
  Version:   $Revision: 0.00 $

=========================================================================*/
#include "ftkNuclearSegmentation.h"
#include <itkImageRegionConstIteratorWithIndex.h>
#include <ctime>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdio.h>

#include "yousef_core/graphColLearn_3D/sequential_coloring.cpp"

namespace ftk 
{

//Constructor
NuclearSegmentation::NuclearSegmentation()
{
	dataFilename.clear();		
	labelFilename.clear();
	paramFilename.clear();

	myParameters.clear();
	myObjects.clear();
	featureNames.clear();
	maxID = 0;
	errorMessage.clear();
	classes.clear();

	dataImage = NULL;
	channelNumber = 0;
	labelImage = NULL;
	NucleusSeg = NULL;
	clustImage = NULL;
	logImage = NULL;
	lastRunStep = 0;
	editsNotSaved = false;
}

NuclearSegmentation::~NuclearSegmentation()
{
	if(NucleusSeg)
		delete NucleusSeg;
}

//*************************************************************************************************************
// This function will initialize the pipeline for a new nuclear segmentation.
// It should be called first.  If this class already contains data and the datafilenames do not match, we delete
// all data and prepare for a new segmentation.
//*************************************************************************************************************
bool NuclearSegmentation::SetInputs(std::string datafile, std::string paramfile)
{
	if( this->dataFilename.compare(datafile) != 0 ||
		this->paramFilename.compare(paramfile) != 0 )	//The names do not match so clear things and start over:
	{
		this->dataFilename.clear();
		this->paramFilename.clear();
		dataImage = 0;
		if(NucleusSeg)
		{
			delete NucleusSeg;
			lastRunStep = 0;
			NucleusSeg = 0;
		}
	}

	this->dataFilename = datafile;
	this->paramFilename = paramfile;
	return true;
}

//***********************************************************************************************************
// Will load the data image into memory (using ftk::Image)
//***********************************************************************************************************
bool NuclearSegmentation::LoadData()
{
	//if(dataImage)
	//{
	//	errorMessage = "Data already loaded";
	//	return false;
	//}

	dataImage = ftk::Image::New();
	if(!dataImage->LoadFile(dataFilename))	//Load for display
	{
		errorMessage = "Data Image failed to load";
		dataImage = 0;
		return false;
	}
	return true;
}

//***********************************************************************************************************
// Will load the label image into memory (using ftk::Image)
//***********************************************************************************************************
bool NuclearSegmentation::LoadLabel()
{
	//if(labelImage)
	//{
	//	errorMessage = "Label already loaded";
	//	return false;
	//}

	labelImage = ftk::Image::New();
	if(!labelImage->LoadFile(labelFilename))	//Load for display
	{
		errorMessage = "Label Image failed to load";
		labelImage = 0;
		return false;
	}
	editsNotSaved = false;
	return true;
}

//*****
// If you are not going to need to return the resulting image as ftk::Image, then pass false to this function
//*****
bool NuclearSegmentation::Binarize(bool getResultImg)
{
	if(!dataImage)
	{
		errorMessage = "No data loaded";
		return false;
	}

	const Image::Info *info = dataImage->GetImageInfo();
	int numStacks = info->numZSlices;
	int numRows = info->numRows;				//y-direction
	int numColumns = info->numColumns; 			//x-direction
	int numChannels = info->numChannels;
	if(channelNumber >= numChannels)
		channelNumber = 0;

	//We assume that the image is unsigned char, but just in case it isn't we make it so:
	dataImage->Cast<unsigned char>();
	unsigned char *dptr = dataImage->GetSlicePtr<unsigned char>(0,channelNumber,0);	//Expects grayscale image

	if(NucleusSeg)
		delete NucleusSeg;
	NucleusSeg = new yousef_nucleus_seg();
	NucleusSeg->readParametersFromFile(paramFilename.c_str());
	NucleusSeg->setDataImage( dptr, numColumns, numRows, numStacks, dataFilename.c_str() );
	NucleusSeg->runBinarization();
	lastRunStep = 1;

	//Get the output
	if(getResultImg)
		GetResultImage();
	return true;
}

bool NuclearSegmentation::DetectSeeds(bool getResultImg)
{
	if(!NucleusSeg)
	{
		errorMessage = "No Binarization";
		return false;
	}
	NucleusSeg->runSeedDetection();
	lastRunStep = 2;
	if(getResultImg)
		GetResultImage();
	return true;
}

bool NuclearSegmentation::RunClustering()
{
	if(!NucleusSeg || lastRunStep < 2)
	{
		errorMessage = "No Seeds";
		return false;
	}
	NucleusSeg->runClustering();
	lastRunStep = 3;
	this->GetParameters();
	this->GetResultImage();
	return true;
}

void NuclearSegmentation::GetParameters()
{
	Parameter p;
	p.name = "sampling_ratio";
	p.value = NucleusSeg->getSamplingRatio();
	this->myParameters.push_back(p);
}

bool NuclearSegmentation::Finalize()
{
	if(!NucleusSeg || lastRunStep < 3)
	{
		errorMessage = "No Initial Clustering";
		return false;
	}
	NucleusSeg->runAlphaExpansion();
	lastRunStep = 4;
	this->GetResultImage();
	return true;
}

bool NuclearSegmentation::GetResultImage()
{
	if(!NucleusSeg)
	{
		errorMessage = "Nothing To Get";
		return false;
	}

	vector<int> size = NucleusSeg->getImageSize();
	unsigned short *dptr = NULL;

	switch(lastRunStep)
	{
	case 0:
		errorMessage = "Nothing To Get";
		return false;
		break;
	case 1:		
		dptr = NucleusSeg->getBinImage();
		break;
	case 2:	//Seeds:
		dptr = NucleusSeg->getSeedImage();
		break;
	case 3:
		dptr = NucleusSeg->getClustImage();
		break;
	case 4:
		dptr = NucleusSeg->getSegImage();
		break;
	}

	if(dptr)
	{
		std::vector<unsigned char> color;
		color.assign(3,255);
		if(labelImage)
			labelImage = 0;
		labelImage = ftk::Image::New();
		if(lastRunStep == 2)
		{
			Cleandptr(dptr,size); // Temporarily deletes the seeds in the bacground from dptr
			labelImage->AppendChannelFromData3D(dptr, itk::ImageIOBase::INT, sizeof(unsigned short), size[2], size[1], size[0], "gray", color, true);		
			Restoredptr(dptr); // Adds the seeds to dptr which were deleted in Cleandptr
		}
		else
			labelImage->AppendChannelFromData3D(dptr, itk::ImageIOBase::INT, sizeof(unsigned short), size[2], size[1], size[0], "gray", color, true);		
		labelImage->Cast<unsigned short>();
	}
	return true;
}

bool NuclearSegmentation::SaveOutput()
{
	if(!NucleusSeg || !labelImage)
	{
		errorMessage = "Nothing To Save";
		return false;
	}

	std::string tag;

	switch(lastRunStep)
	{
	case 1:
		tag = "_bin";
		break;
	case 2:	//Seeds:
		tag = "_seed";
		break;
	case 3:
		tag = "_label";
		break;
	case 4:
		tag = "_label";
		break;
	default:
		errorMessage = "Nothing To Save";
		return false;
		break;
	}

	size_t pos = dataFilename.find_last_of(".");
	std::string base = dataFilename.substr(0,pos);
	std::string ext = dataFilename.substr(pos);
	labelImage->SaveChannelAs(0, base + tag, ext );
	editsNotSaved = false;

	return true;
}

void NuclearSegmentation::ReleaseSegMemory()
{
	if(NucleusSeg)
	{
		delete NucleusSeg;
		NucleusSeg = NULL;
	}
}

//Calculate the object information from the data/result images:
bool NuclearSegmentation::LabelsToObjects(void)
{
	if(!dataImage)
	{
		errorMessage = "No Data Image";
		return false;
	}
	if(!labelImage)
	{
		errorMessage = "No Label Image";
		return false;		
	}

	typedef ftk::LabelImageToFeatures< unsigned char, unsigned short, 3 > FeatureCalcType;
	FeatureCalcType::Pointer labFilter = FeatureCalcType::New();
	labFilter->SetImageInputs( dataImage->GetItkPtr<unsigned char>(0,0), labelImage->GetItkPtr<unsigned short>(0,0) );
	labFilter->SetLevel(3);
	labFilter->ComputeHistogramOn();
	labFilter->ComputeTexturesOn();
	labFilter->Update();

	//Set Feature Names
	featureNames.clear();
	for (int i=0; i < IntrinsicFeatures::N; ++i)
	{
		featureNames.push_back( IntrinsicFeatures::Info[i].name );
	}

	//Now populate the objects
	myObjects.clear();
	IdToIndexMap.clear();
	std::vector< FeatureCalcType::LabelPixelType > labels = labFilter->GetLabels();
	for (int i=0; i<(int)labels.size(); ++i)
	{
		FeatureCalcType::LabelPixelType id = labels.at(i);
		if(id == 0) continue;

		if(id > maxID) maxID = id;

		myObjects.push_back( GetNewObject(id, labFilter->GetFeatures(id) ) );
		IdToIndexMap[id] = (int)myObjects.size() - 1;
	}

	/*
	if(associationFile.size())
		LoadAssociationsFromFile(associationFile);
	if(classFile.size())
		LoadClassInfoFromFile(classFile);
	*/
	editsNotSaved = true;
	return true;
}


//This function will take the data and result filenames, and create the NuclearSegmentation structure
// file should be filename only.  Path should come from path set for project
bool NuclearSegmentation::LoadFromImages(std::string dfile, std::string rfile)
{
	//See if data file exists:
	if( !FileExists(dfile) )
	{
		errorMessage = "Could not find data file";
		return 0;
	}
	//See if result file exists
	if( !FileExists(rfile) )
	{
		errorMessage = "Could not find result file";
		return 0;
	}
	//Save the filenames
	dataFilename = dfile;
	labelFilename = rfile;

	LoadData();
	LoadLabel();


	editsNotSaved = false;

	return LabelsToObjects();
}

void NuclearSegmentation::LoadFromDAT(std::string dfile, std::string rfile)
{
	dataFilename = dfile;
	LoadData();
	
	if(NucleusSeg)
		delete NucleusSeg;
	NucleusSeg = new yousef_nucleus_seg();
	const Image::Info *info = dataImage->GetImageInfo();
	int numStacks = info->numZSlices;
	int numRows = info->numRows;				//y-direction
	int numColumns = info->numColumns; 			//x-direction

	//We assume that the image is unsigned char, but just in case it isn't we make it so:
	dataImage->Cast<unsigned char>();
	unsigned char *dptr = dataImage->GetSlicePtr<unsigned char>(0,0,0);	//Expects grayscale image	
	NucleusSeg->setDataImage( dptr, numColumns, numRows, numStacks, dataFilename.c_str() );
	NucleusSeg->readFromIDLFormat(rfile);
	lastRunStep = 4;
	GetResultImage();
}


//The function will read the given file and load the association measures into the object info;
bool NuclearSegmentation::LoadAssociationsFromFile(std::string fName)
{
	if(myObjects.size() == 0)
	{
		errorMessage = "No Objects";
		return false;
	}
	if(!FileExists(fName.c_str()))
	{
		errorMessage = "File does not exist";
		return false;
	}


	TiXmlDocument doc;
	if ( !doc.LoadFile( fName.c_str() ) )
	{
		errorMessage = "Unable to load XML File";
		return false;
	}

	TiXmlElement* rootElement = doc.FirstChildElement();
	const char* docname = rootElement->Value();
	if ( strcmp( docname, "ObjectAssociationRules" ) != 0 )
	{
		errorMessage = "Incorrect XML root Element: ";
		errorMessage.append(rootElement->Value());
		return false;
	}

	std::string source = rootElement->Attribute("SegmentationSource");
	//int numMeasures = atoi( rootElement->Attribute("NumberOfAssociativeMeasures") );
	unsigned int numObjects = atoi( rootElement->Attribute("NumberOfObjects") );

	if( numObjects != myObjects.size() )
	{
		errorMessage = "Number of Objects does not match";
		return false;
	}

	bool firstRun = true;
	//Parents we know of: Object
	TiXmlElement* parentElement = rootElement->FirstChildElement();
	while (parentElement)
	{
		const char * parent = parentElement->Value();

		if ( strcmp( parent, "Object" ) == 0 )
		{
			int id = atoi( parentElement->Attribute("ID") );
			std::vector<float> feats = myObjects.at( IdToIndexMap[id] ).GetFeatures();

			TiXmlElement *association = parentElement->FirstChildElement();
			while (association)
			{
				std::string name = association->Attribute("Name");
				float value = atof( association->Attribute("Value") );

				if(firstRun)
				{
					featureNames.push_back( name );
				}
				feats.push_back(value);

				association = association->NextSiblingElement();
			}
			firstRun = false;
			myObjects.at( IdToIndexMap[id] ).SetFeatures(feats);
		}
		else
		{
			errorMessage = "Unrecognized parent element: ";
			errorMessage.append(parent);
			return false;
		}
		parentElement = parentElement->NextSiblingElement();
	} // end while(parentElement)
	return true;
}

bool NuclearSegmentation::LoadClassInfoFromFile( std::string fName )
{
	if(myObjects.size() == 0)
	{
		errorMessage = "No Objects";
		return false;
	}

	if(!FileExists(fName.c_str()))
	{
		errorMessage = "No Class File";
		return false;
	}

	ifstream inFile; 
	inFile.open( fName.c_str() );
	if ( !inFile.is_open() )
	{
		errorMessage = "Failed to Load Document";
		return false;
	}

	const int MAXLINESIZE = 512;	//Numbers could be in scientific notation in this file
	char line[MAXLINESIZE];

	//std::map< int, int > classNumber; 
	std::vector<int> classNumber;
	inFile.getline(line, MAXLINESIZE);
	while ( !inFile.eof() ) //Get all rows
	{
		char * pch = strtok (line," \t\n");
		//int id = (int)atof(pch);
		//pch = strtok (NULL, " \t\n");
		int clss = (int)atof(pch);

		//classNumber[id] = clss;
		classNumber.push_back(clss);
	
		inFile.getline(line, MAXLINESIZE);
	}
	inFile.close();

	/*
	std::map< int, int >::iterator it;
	for ( it=classNumber.begin() ; it != classNumber.end(); it++ )

	{
		ftk::Object * obj = GetObjectPtr( (*it).first );
		obj->SetClass( (*it).second );
	}
	*/

	std::vector<Object> *objects = GetObjectsPtr();
	if(classNumber.size() != objects->size())
	{
		errorMessage = "File sizes do not match";
		return false;
	}

	std::set<int> classList;
	std::set<int>::iterator it;
	for(unsigned int i=0; i<objects->size(); ++i)
	{
		int c = classNumber.at(i);

		it=classList.find(c);
		if(it==classList.end())
			classList.insert(c);

		objects->at(i).SetClass( char(c) );
	}

	classes.clear();
	for(it=classList.begin(); it!=classList.end(); ++it)
	{
		classes.push_back(*it);
	}
	return true;
}

bool NuclearSegmentation::LoadFromMETA(std::string META_file, std::string header_file, std::string data_file, std::string label_file)
{
	const int MAXLINESIZE = 512;	//Numbers could be in scientific notation in this file
	char line[MAXLINESIZE];

	//Save the filenames & Load the Images
	dataFilename=data_file;
	labelFilename=label_file;
	dataImage = ftk::Image::New();
	dataImage->LoadFile(data_file);	//Assume there is just one data file and one result file
	labelImage = ftk::Image::New();
	labelImage->LoadFile(label_file);

	//NOW LOAD THE HEADER INFO:
	ifstream headerFile; 
	headerFile.open( header_file.c_str() );
	if ( !headerFile.is_open() )
	{
		std::cerr << "Failed to Load Document: " << headerFile << std::endl;
		return 0;
	}
	std::vector< std::string > header; 
	headerFile.getline(line, MAXLINESIZE);
	while ( !headerFile.eof() ) //Get all values
	{
		std::string h;
		char * pch = strtok (line," \t");
		while (pch != NULL)
		{
			h = pch;
			pch = strtok (NULL, " \t");
		}
		header.push_back( h );
		headerFile.getline(line, MAXLINESIZE);
	}
	headerFile.close();

	//SEARCH FOR CLASS/RESPONSE AND ID COLUMNS
	//IF I DO NOT FIND ID I ASSUME ORDERED BY MAGNITUDE, AND GET IDS FROM LABEL IMAGE
	int classColumn = -1;
	int idColumn = -1;
	for (int i=0; i<(int)header.size(); ++i)
	{
		if ( !header.at(i).compare("CLASS") || !header.at(i).compare("class")|| !header.at(i).compare("RESPONSE") )
			classColumn = i;
		else if ( !header.at(i).compare("ID") )
			idColumn = i;
	}

	//NOW LOAD ALL OF THE FEATURES INFO:
	ifstream metaFile; 
	metaFile.open( META_file.c_str() );
	if ( !metaFile.is_open() )
	{
		std::cerr << "Failed to Load Document: " << metaFile << std::endl;
		return 0;
	}
	std::vector< std::vector<double> > meta; 
	metaFile.getline(line, MAXLINESIZE);
	while ( !metaFile.eof() ) //Get all values
	{
		std::vector<double> row;
		char * pch = strtok (line," \t");
		while (pch != NULL)
		{
			row.push_back( atof(pch) );
			pch = strtok (NULL, " \t");
		}
		meta.push_back( row );
		metaFile.getline(line, MAXLINESIZE);
	}
	metaFile.close();

	//NOW RUN THE FEATURES FILTER TO GET BOUNDING BOXES:
	typedef unsigned char IPixelT;
	dataImage->Cast<IPixelT>();
	typedef unsigned short LPixelT;
	labelImage->Cast<LPixelT>();

	typedef ftk::LabelImageToFeatures< IPixelT, LPixelT, 3 > FeatureCalcType;
	FeatureCalcType::Pointer labFilter = FeatureCalcType::New();
	labFilter->SetImageInputs( dataImage->GetItkPtr<IPixelT>(0,0), labelImage->GetItkPtr<LPixelT>(0,0) );
	labFilter->SetLevel(1);
	labFilter->ComputeHistogramOff();
	labFilter->ComputeTexturesOff();
	labFilter->Update();

	//Set Feature Names
	featureNames.clear();
	for (int i=0; i<(int)header.size(); ++i)
	{
		if (i == classColumn || i == idColumn) continue;
		featureNames.push_back( header.at(i) );
	}

	//Now populate the objects
	myObjects.clear();
	IdToIndexMap.clear();
	std::vector< FeatureCalcType::LabelPixelType > labels = labFilter->GetLabels();
	for(int row=0; row < (int)labels.size(); row++)
	{
		FeatureCalcType::LabelPixelType id = labels.at(row);
		if(id == 0) continue;

		if(id > maxID) maxID = id;

		int metaRow = -1;
		if(idColumn != -1)
		{
			//Search meta for the current id
			for(int i=0; i<(int)meta.size(); ++i)
			{
				if(meta.at(i).at(idColumn) == id)
					metaRow = i;
			}
		}
		else
		{
			metaRow = row-1;
		}

		Object object("nucleus");
		object.SetId(id);
		object.SetValidity(ftk::Object::VALID);
		object.SetDuplicated(0);
		if( classColumn != -1 && metaRow != -1 )
			object.SetClass( char(meta.at(metaRow).at(classColumn)) );
		else
			object.SetClass(-1);

		ftk::IntrinsicFeatures *features = labFilter->GetFeatures(id);

		Object::Point c;
		c.x = (int)features->Centroid[0];
		c.y = (int)features->Centroid[1];
		c.z = (int)features->Centroid[2];
		c.t = 0;
		object.SetCentroid(c);

		Object::Box b;
		b.min.x = (int)features->BoundingBox[0];
		b.max.x = (int)features->BoundingBox[1];
		b.min.y = (int)features->BoundingBox[2];
		b.max.y = (int)features->BoundingBox[3];
		b.min.z = (int)features->BoundingBox[4];
		b.max.z = (int)features->BoundingBox[5];
		b.min.t = 0;
		b.max.t = 0;
		object.SetBoundingBox(b);

		vector< float > f(0);
		for (int i=0; i<(int)header.size(); ++i)
		{
			if (i == classColumn || i == idColumn) continue;
			if(metaRow == -1)
				f.push_back(0.0);
			else
				f.push_back( (float)meta.at(metaRow).at(i) );
		}
		object.SetFeatures( f );
		myObjects.push_back( object );
		IdToIndexMap[id] = (int)myObjects.size() - 1;
	}

	return 1;
}

bool NuclearSegmentation::SaveLabelByClass()
{
	if(!labelImage)
	{
		errorMessage = "Label Image has not be loaded";
		return false;
	}

	//Cast the label Image & Get ITK Pointer
	typedef unsigned short PixelType;
	typedef itk::Image<PixelType, 3> ImageType;
	labelImage->Cast<PixelType>();
	ImageType::Pointer img = labelImage->GetItkPtr<PixelType>(0,0);

	//Create an image for each class:
	int numClasses = (int)classes.size();
	std::vector<ImageType::Pointer> outImgs;
	for(int i=0; i<numClasses; ++i)
	{
		ImageType::Pointer tmp = ImageType::New();   
		tmp->SetOrigin( img->GetOrigin() );
		tmp->SetRegions( img->GetLargestPossibleRegion() );
		tmp->SetSpacing( img->GetSpacing() );
		tmp->Allocate();
		tmp->FillBuffer(0);
		tmp->Update();

		outImgs.push_back(tmp);
	}

	//create lists of object ids in each class:
	std::vector< std::set<int> > objClass(numClasses);
	for(int i=0; i<(int)myObjects.size(); ++i)
	{
		int c = (int)myObjects.at(i).GetClass();
		int id = (int)myObjects.at(i).GetId();
		int p = 0;
		for(int j=0; j<numClasses; ++j)
		{
			if(c == classes.at(j))
				break;
			++p;
		}
		if(p < numClasses)
			objClass.at(p).insert(id);
	}

	//Iterate through Image & populate all of the other images
	typedef itk::ImageRegionConstIteratorWithIndex< ImageType > IteratorType;
	IteratorType it(img,img->GetRequestedRegion());
	for(it.GoToBegin(); !it.IsAtEnd(); ++it)
	{
		int id = it.Get();
		for(int j=0; j<numClasses; ++j)
		{
			if( objClass.at(j).find(id) != objClass.at(j).end() )
			{
				outImgs.at(j)->SetPixel(it.GetIndex(), 1); 
			}
		}	
	}

	//Now Write All of the Images to File
	typedef itk::ImageFileWriter<ImageType> WriterType;
	for(int i=0; i<numClasses; ++i)
	{
		WriterType::Pointer writer = WriterType::New();
		size_t pos = dataFilename.find_last_of(".");
		std::string base = dataFilename.substr(0,pos);
		std::string ext = dataFilename.substr(pos);
		writer->SetFileName( base + "_class" + NumToString(classes.at(i)) + ext );
		writer->SetInput( outImgs.at(i) );
    
		try
		{
			writer->Update();
		}
		catch( itk::ExceptionObject & excp )
		{
			std::cerr << excp << std::endl;
			writer = 0;
			errorMessage = "Problem saving file to disk";
			return false;
		}
		
		writer = 0;
	}

	editsNotSaved = false;
	return true;
	
}

bool NuclearSegmentation::SaveLabel()
{
	if(!labelImage)
	{
		errorMessage = "No Label Image to Save";
		return false;
	}

	labelFilename.clear();
	size_t pos = dataFilename.find_last_of(".");
	std::string base = dataFilename.substr(0,pos);
	//std::string ext = dataFilename.substr(pos+1);
	std::string ext = "tif";
	labelFilename = base + "_label" + "." + ext;

	labelImage->Cast<unsigned short>();		//Cannot Save as int type to tiff
	if(!labelImage->SaveChannelAs(0, base + "_label", ext))
		std::cerr << "FAILED TO SAVE LABEL IMAGE" << std::endl;
	std::cout<<" segmentation output saved into "<<base<<"_label."<<ext<<std::endl;

	//Added by Yousef on 1/18/2009: save results into a format readable by the IDL farsight
	//if(NucleusSeg)
	//	NucleusSeg->saveIntoIDLFormat(base);

	editsNotSaved = false;
	return true;
}


ftk::Object::Box NuclearSegmentation::ExtremaBox(vector<int> ids)
{
	ftk::Object::Box extreme = myObjects.at( GetObjectIndex(ids.at(0),"nucleus") ).GetBoundingBox();
	for(int i=1; i<(int)ids.size(); ++i)
	{
		ftk::Object::Box test = myObjects.at( GetObjectIndex(ids.at(i),"nucleus") ).GetBoundingBox();

		extreme.min.x = min(extreme.min.x, test.min.x);
		extreme.min.y = min(extreme.min.y, test.min.y);
		extreme.min.z = min(extreme.min.z, test.min.z);
		extreme.min.t = min(extreme.min.t, test.min.t);
		extreme.max.x = max(extreme.max.x, test.max.x);
		extreme.max.y = max(extreme.max.y, test.max.y);
		extreme.max.z = max(extreme.max.z, test.max.z);
		extreme.max.t = max(extreme.max.t, test.max.t);
	}

	return extreme;
}

std::vector< int > NuclearSegmentation::Split(ftk::Object::Point P1, ftk::Object::Point P2)
{
	//if no label (segmentation) or no data image is available then return
	if(!labelImage || !dataImage)
	{
		errorMessage = "label image or data image doesn't exist";	
		std::vector <int> ids_err;
		ids_err.push_back(0);
		ids_err.push_back(0);
		return ids_err;
	}
	//Check if the two points inside the same cell
	int id1 = (int)labelImage->GetPixel(0,0,P1.z,P1.y,P1.x);
	int id2 = (int)labelImage->GetPixel(0,0,P2.z,P2.y,P2.x);
	if(id1 != id2)
	{		
		std::vector <int> ids_err;
		ids_err.push_back(0);
		ids_err.push_back(0);
		return ids_err;
	}
	if(id1==0 || id2==0)
	{		
		std::vector <int> ids_err;
		ids_err.push_back(0);
		ids_err.push_back(0);
		return ids_err;
	}
	
	//Update the segmentation image
	//Now get the bounding box around the object
	std::vector <int> ids;
	ids.push_back(id1);
	ftk::Object::Box region = ExtremaBox(ids);

	//get the indexes of the two seeds with respect to the begining of the bounding box
	std::vector <int> sz;
	sz.push_back(region.max.x - region.min.x + 1);
	sz.push_back(region.max.y - region.min.y + 1);
	sz.push_back(region.max.z - region.min.z + 1);
	
	int ind1 = ((P1.z- region.min.z)*sz[0]*sz[1]) + ((P1.y - region.min.y)*sz[0]) + (P1.x - region.min.x);
	int ind2 = ((P2.z- region.min.z)*sz[0]*sz[1]) + ((P2.y - region.min.y)*sz[0]) + (P2.x - region.min.x);

	//create two new itk images with the same size as the bounding box	 	 
	typedef    float     InputPixelType;
	typedef itk::Image< InputPixelType,  3 >   InputImageType;
	InputImageType::Pointer sub_im1;
	sub_im1 = InputImageType::New();
	InputImageType::Pointer sub_im2;
	sub_im2 = InputImageType::New();
	InputImageType::PointType origin;
    origin[0] = 0.; 
    origin[1] = 0.;    
	origin[2] = 0.;    
    sub_im1->SetOrigin( origin );
	sub_im2->SetOrigin( origin );

    InputImageType::IndexType start;
    start[0] =   0;  // first index on X
    start[1] =   0;  // first index on Y    
	start[2] =   0;  // first index on Z    
    InputImageType::SizeType  size;
    size[0]  = sz[0];  // size along X
    size[1]  = sz[1];  // size along Y
	size[2]  = sz[2];  // size along Z
  
    InputImageType::RegionType rgn;
    rgn.SetSize( size );
    rgn.SetIndex( start );
    
   
    sub_im1->SetRegions( rgn );
    sub_im1->Allocate();
    sub_im1->FillBuffer(0);
	sub_im1->Update();	
	sub_im2->SetRegions( rgn );
    sub_im2->Allocate();
    sub_im2->FillBuffer(0);
	sub_im2->Update();

	//set all the points in those images to zeros except for the two points corresponding to the two new seeds
	//notice that one seed is set in each image
	typedef itk::ImageRegionIteratorWithIndex< InputImageType > IteratorType;
	IteratorType iterator1(sub_im1,sub_im1->GetRequestedRegion());
	IteratorType iterator2(sub_im2,sub_im2->GetRequestedRegion());	
		
	for(int i=0; i<sz[0]*sz[1]*sz[2]; i++)
	{				
		if(i==ind1)
			iterator1.Set(255.0);
		else
			iterator1.Set(0.0);		
		if(i==ind2)
			iterator2.Set(255.0);
		else
		iterator2.Set(0.0);
		
		++iterator1;	
		++iterator2;	
	}
	

	//compute the distance transforms of those binary itk images
	typedef    float     OutputPixelType;
	typedef itk::Image< OutputPixelType, 3 >   OutputImageType;
	typedef itk::DanielssonDistanceMapImageFilter<InputImageType, OutputImageType > DTFilter ;
	DTFilter::Pointer dt_obj1= DTFilter::New() ;
	DTFilter::Pointer dt_obj2= DTFilter::New() ;
	dt_obj1->UseImageSpacingOn();
	dt_obj1->SetInput(sub_im1) ;
	dt_obj2->UseImageSpacingOn();
	dt_obj2->SetInput(sub_im2) ;

	try{
		dt_obj1->Update() ;
	}
	catch( itk::ExceptionObject & err ){
		std::cerr << "Error calculating distance transform: " << err << endl ;
	}
	try{
		dt_obj2->Update() ;
	}
	catch( itk::ExceptionObject & err ){
		std::cerr << "Error calculating distance transform: " << err << endl ;
	}

	//Now, relabel the cell points into either the old id (id1) or a new id (newID) based on the distances to the seeds
	++maxID;		
	int newID1 = maxID;
	++maxID;		
	int newID2 = maxID;
	IteratorType iterator3(dt_obj1->GetOutput(),dt_obj1->GetOutput()->GetRequestedRegion());
	IteratorType iterator4(dt_obj2->GetOutput(),dt_obj2->GetOutput()->GetRequestedRegion());	
	for(int k=region.min.z; k<=region.max.z; k++)
	{
		for(int i=region.min.y; i<=region.max.y; i++)
		{			
			for(int j=region.min.x; j<=region.max.x; j++)
			{
				int d1 = (int) fabs(iterator3.Get());	 
				int d2 = (int) fabs(iterator4.Get());	
				++iterator3;
				++iterator4;
				int pix = (int)labelImage->GetPixel(0,0,k,i,j);
				if(pix != id1)
					continue;
				if(d1>d2)
					labelImage->SetPixel(0,0,k,i,j,newID1);
				else
					labelImage->SetPixel(0,0,k,i,j,newID2);

			}
		}
	}	

	//set the old object to invalid
	//and add this edit to the editing record
	int inn = GetObjectIndex(id1,"nucleus");
	myObjects.at(inn).SetValidity(ftk::Object::SPLIT);
	ftk::Object::EditRecord record;
	record.date = TimeStamp();
	std::string msg = "SPLIT TO BECOME ";
	msg.append(NumToString(newID1));
	msg.append(" and ");
	msg.append(NumToString(newID2));
	record.description = msg;
	myObjects.at(inn).AddEditRecord(record);	

	//Calculate features of the two new objects using feature filter
	typedef unsigned char IPixelT;
	typedef unsigned short LPixelT;
	typedef itk::Image< IPixelT, 3 > IImageT;
	typedef itk::Image< LPixelT, 3 > LImageT;

	dataImage->Cast<IPixelT>();
	labelImage->Cast<LPixelT>();

	IImageT::Pointer itkIntImg = dataImage->GetItkPtr<IPixelT>(0,0);
	LImageT::Pointer itkLabImg = labelImage->GetItkPtr<LPixelT>(0,0);

	typedef ftk::LabelImageToFeatures< IPixelT, LPixelT, 3 > FeatureCalcType;
	FeatureCalcType::Pointer labFilter = FeatureCalcType::New();
	labFilter->SetLevel(3);
	labFilter->ComputeHistogramOn();
	labFilter->ComputeTexturesOn();

	
	IImageT::RegionType intRegion;
	IImageT::SizeType intSize;
	IImageT::IndexType intIndex;
	LImageT::RegionType labRegion;
	LImageT::SizeType labSize;
	LImageT::IndexType labIndex;

	//start with the first one
	intIndex[0] = region.min.x;
	intIndex[1] = region.min.y;
	intIndex[2] = region.min.z;
	intSize[0] = region.max.x - region.min.x + 1;
	intSize[1] = region.max.y - region.min.y + 1;
	intSize[2] = region.max.z - region.min.z + 1;

	labIndex[0] = region.min.x;
	labIndex[1] = region.min.y;
	labIndex[2] = region.min.z;
	labSize[0] = region.max.x - region.min.x + 1;
	labSize[1] = region.max.y - region.min.y + 1;
	labSize[2] = region.max.z - region.min.z + 1;

	intRegion.SetSize(intSize);
    intRegion.SetIndex(intIndex);
    itkIntImg->SetRequestedRegion(intRegion);

    labRegion.SetSize(labSize);
    labRegion.SetIndex(labIndex);
    itkLabImg->SetRequestedRegion(labRegion);
	
	labFilter->SetImageInputs( itkIntImg, itkLabImg );
	labFilter->Update();

	//add them to the features list
	myObjects.push_back( GetNewObject(newID1, labFilter->GetFeatures(newID1) ) );
	myObjects.push_back( GetNewObject(newID2, labFilter->GetFeatures(newID2) ) );
	IdToIndexMap[newID1] = (int)myObjects.size() - 2;
	IdToIndexMap[newID2] = (int)myObjects.size() - 1;

	//return the ids of the two cells resulting from spliting
	std::vector <int> ids_ok;
	ids_ok.push_back(newID1);
	ids_ok.push_back(newID2);

	//also, add the old ID to the end of the list
	ids_ok.push_back(id1);

	// Update the editing history of the new cells - Aytekin
	record.date = TimeStamp();	
	int index;
	for(int i=0; i<(((int)ids_ok.size())-1); ++i)
	{
		msg = "SPLITTED FROM: ";		
		msg.append(NumToString(id1));
		record.description = msg;
		index = GetObjectIndex(ids_ok.at(i),"nucleus");
		myObjects.at(index).AddEditRecord(record);
	}
	editsNotSaved = true;
	return ids_ok;
}

//This function is applied on the initial segmentation image and updates the LoG response image
std::vector< int > NuclearSegmentation::SplitInit(ftk::Object::Point P1, ftk::Object::Point P2)
{
	//if no label (segmentation) or no data image is available then return
	if(!labelImage || !dataImage)
	{
		errorMessage = "label image or data image doesn't exist";	
		std::vector <int> ids_err;
		ids_err.push_back(0);
		ids_err.push_back(0);
		return ids_err;
	}	
	
	//Apply the splitting
	std::vector <int> ids_ok = NucleusSeg->SplitInit(P1, P2);
		
	return ids_ok;
}


std::vector< int > NuclearSegmentation::SplitAlongZ(int objID, int cutSlice)
{
	//if no label (segmentation) or no data image is available then return
	if(!labelImage || !dataImage)
	{
		errorMessage = "label image or data image doesn't exist";	
		std::vector <int> ids_err;
		ids_err.push_back(0);
		ids_err.push_back(0);
		return ids_err;
	}			
	
	//Update the segmentation image
	//Now get the bounding box around the object
	std::vector <int> ids;
	ids.push_back(objID);
	ftk::Object::Box region = ExtremaBox(ids);	

	//Now, relabel the cell points into either newID1 or newID2 based on the z-slice
	++maxID;		
	int newID1 = maxID;
	++maxID;		
	int newID2 = maxID;		
	for(int k=region.min.z; k<=region.max.z; k++)
	{
		for(int i=region.min.y; i<=region.max.y; i++)
		{			
			for(int j=region.min.x; j<=region.max.x; j++)
			{				
				int pix = (int)labelImage->GetPixel(0,0,k,i,j);
				if(pix != objID)
					continue;
				if(k<cutSlice)
					labelImage->SetPixel(0,0,k,i,j,newID1);
				else
					labelImage->SetPixel(0,0,k,i,j,newID2);

			}
		}
	}	

	//set the old object to invalid
	//and add this edit to the editing record
	int inn = GetObjectIndex(objID,"nucleus");
	myObjects.at(inn).SetValidity(ftk::Object::SPLIT);
	ftk::Object::EditRecord record;
	record.date = TimeStamp();
	std::string msg = "SPLIT TO BECOME ";
	msg.append(NumToString(newID1));
	msg.append(" and ");
	msg.append(NumToString(newID2));
	record.description = msg;
	myObjects.at(inn).AddEditRecord(record);	

	//Calculate features of the two new objects using feature filter
	typedef unsigned char IPixelT;
	typedef unsigned short LPixelT;
	typedef itk::Image< IPixelT, 3 > IImageT;
	typedef itk::Image< LPixelT, 3 > LImageT;

	dataImage->Cast<IPixelT>();
	labelImage->Cast<LPixelT>();

	IImageT::Pointer itkIntImg = dataImage->GetItkPtr<IPixelT>(0,0);
	LImageT::Pointer itkLabImg = labelImage->GetItkPtr<LPixelT>(0,0);

	typedef ftk::LabelImageToFeatures< IPixelT, LPixelT, 3 > FeatureCalcType;
	FeatureCalcType::Pointer labFilter = FeatureCalcType::New();
	labFilter->SetLevel(3);
	labFilter->ComputeHistogramOn();
	labFilter->ComputeTexturesOn();

	
	IImageT::RegionType intRegion;
	IImageT::SizeType intSize;
	IImageT::IndexType intIndex;
	LImageT::RegionType labRegion;
	LImageT::SizeType labSize;
	LImageT::IndexType labIndex;

	//start with the first one
	intIndex[0] = region.min.x;
	intIndex[1] = region.min.y;
	intIndex[2] = region.min.z;
	intSize[0] = region.max.x - region.min.x + 1;
	intSize[1] = region.max.y - region.min.y + 1;
	intSize[2] = region.max.z - region.min.z + 1;

	labIndex[0] = region.min.x;
	labIndex[1] = region.min.y;
	labIndex[2] = region.min.z;
	labSize[0] = region.max.x - region.min.x + 1;
	labSize[1] = region.max.y - region.min.y + 1;
	labSize[2] = region.max.z - region.min.z + 1;

	intRegion.SetSize(intSize);
    intRegion.SetIndex(intIndex);
    itkIntImg->SetRequestedRegion(intRegion);

    labRegion.SetSize(labSize);
    labRegion.SetIndex(labIndex);
    itkLabImg->SetRequestedRegion(labRegion);
	
	labFilter->SetImageInputs( itkIntImg, itkLabImg );
	labFilter->Update();

	//add them to the features list
	myObjects.push_back( GetNewObject(newID1, labFilter->GetFeatures(newID1) ) );
	myObjects.push_back( GetNewObject(newID2, labFilter->GetFeatures(newID2) ) );
	IdToIndexMap[newID1] = (int)myObjects.size() - 2;
	IdToIndexMap[newID2] = (int)myObjects.size() - 1;

	//return the ids of the two cells resulting from spliting
	std::vector <int> ids_ok;
	ids_ok.push_back(newID1);
	ids_ok.push_back(newID2);

	//also, add the old ID to the end of the list
	ids_ok.push_back(objID);

	// Update the editing history of the new cells - Aytekin
	record.date = TimeStamp();	
	int index;
	for(int i=0; i<(((int)ids_ok.size())-1); ++i)
	{
		msg = "SPLITTED FROM: ";		
		msg.append(NumToString(objID));
		record.description = msg;
		index = GetObjectIndex(ids_ok.at(i),"nucleus");
		myObjects.at(index).AddEditRecord(record);
	}
	editsNotSaved = true;
	return ids_ok;
}
int NuclearSegmentation::Merge(vector<int> ids)
{
	if(!labelImage)
		return 0;

	//Merge is difficult because we are going to invalidate all merged objects,
	// create a new label and assign the old object labels to it,
	// calculate the features for this new label
	// then the model should also be updated to display properly
	// Return the new ID

	int newID = ++maxID;
	for(int i=0; i<(int)ids.size(); ++i)
	{
		int index = GetObjectIndex(ids.at(i),"nucleus");
		if(index < 0 ) return 0;

		myObjects.at(index).SetValidity(ftk::Object::MERGED);
		ftk::Object::EditRecord record;
		record.date = TimeStamp();
		std::string msg = "MERGED TO BECOME ";
		msg.append(NumToString(newID));
		record.description = msg;
		myObjects.at(index).AddEditRecord(record);
	}
	ftk::Object::Box region = ExtremaBox(ids);
	ReassignLabels(ids, newID, region);		//Assign all old labels to this new label

	if(!labelImage || !dataImage)
	{
		errorMessage = "label image or data image doesn't exist";
		return 0;
	}

	//Calculate features using feature filter
	typedef unsigned char IPixelT;
	typedef unsigned short LPixelT;
	typedef itk::Image< IPixelT, 3 > IImageT;
	typedef itk::Image< LPixelT, 3 > LImageT;

	dataImage->Cast<IPixelT>();
	labelImage->Cast<LPixelT>();

	IImageT::Pointer itkIntImg = dataImage->GetItkPtr<IPixelT>(0,0);
	LImageT::Pointer itkLabImg = labelImage->GetItkPtr<LPixelT>(0,0);

	IImageT::RegionType intRegion;
	IImageT::SizeType intSize;
	IImageT::IndexType intIndex;
	LImageT::RegionType labRegion;
	LImageT::SizeType labSize;
	LImageT::IndexType labIndex;

	intIndex[0] = region.min.x;
	intIndex[1] = region.min.y;
	intIndex[2] = region.min.z;
	intSize[0] = region.max.x - region.min.x + 1;
	intSize[1] = region.max.y - region.min.y + 1;
	intSize[2] = region.max.z - region.min.z + 1;

	labIndex[0] = region.min.x;
	labIndex[1] = region.min.y;
	labIndex[2] = region.min.z;
	labSize[0] = region.max.x - region.min.x + 1;
	labSize[1] = region.max.y - region.min.y + 1;
	labSize[2] = region.max.z - region.min.z + 1;

	intRegion.SetSize(intSize);
    intRegion.SetIndex(intIndex);
    itkIntImg->SetRequestedRegion(intRegion);

    labRegion.SetSize(labSize);
    labRegion.SetIndex(labIndex);
    itkLabImg->SetRequestedRegion(labRegion);

	typedef ftk::LabelImageToFeatures< IPixelT, LPixelT, 3 > FeatureCalcType;
	FeatureCalcType::Pointer labFilter = FeatureCalcType::New();
	labFilter->SetImageInputs( itkIntImg, itkLabImg );
	labFilter->SetLevel(3);
	labFilter->ComputeHistogramOn();
	labFilter->ComputeTexturesOn();
	labFilter->Update();

	myObjects.push_back( GetNewObject(newID, labFilter->GetFeatures(newID) ) );
	
	ftk::Object::EditRecord record;
	record.date = TimeStamp();
	std::string msg = "MERGED TO FROM: ";
	msg.append(NumToString(ids.at(0)));
	for(int i=1; i<(int)ids.size(); ++i)
	{
		msg.append(", ");
		msg.append(NumToString(ids.at(i)));
	}
	record.description = msg;
	myObjects.back().AddEditRecord(record);
	IdToIndexMap[newID] = (int)myObjects.size() - 1;

	editsNotSaved = true;
	return newID;
}
//this is used when we apply merging on the initial segmentation
ftk::Object::Point NuclearSegmentation::MergeInit(ftk::Object::Point P1, ftk::Object::Point P2, int* new_id)
{
	//if no label (segmentation) or no data image is available then return
	if(!labelImage || !dataImage)
	{
		errorMessage = "label image or data image doesn't exist";			
		ftk::Object::Point err;
		err.t = err.x = err.y = err.z = 0;
		return err;
	}	
	
	//Apply the splitting
	ftk::Object::Point newSeed = NucleusSeg->MergeInit(P1, P2, new_id);
		
	return newSeed;
}

int NuclearSegmentation::AddObject(ftk::Object::Point P1, ftk::Object::Point P2)
{
	//if no label (segmentation) or no data image is available then return
	if(!labelImage || !dataImage)
	{
		errorMessage = "label image or data image doesn't exist";					
		return 0;
	}	
	std::vector<unsigned short> size = labelImage->Size();
	////get the coordinates of the two points and the size of the box
	int x1 = P1.x;
	int x2 = P2.x;
	int y1 = P1.y;
	int y2 = P2.y;
	int z1 = P1.z;
	int z2 = P2.z;

	int sz_x = x2-x1+1;
	int sz_y = y2-y1+1;
	if(z1==z2)
	{
		//assume that the sampling ratio is 2
		int dz;
		if(sz_x > sz_y)
			dz = sz_x/4;
		else
			dz = sz_y/4;

		z1 -= dz;
		if(z1<0)
			z1=0;
		z2 += dz;
		if(z2>size[1]-1)
			z2=size[1]-1;
	}
	
	int sz_z = z2-z1+1;
	if(sz_x<1 || sz_y<1 || sz_z<1)
		return 0;

	
	
	//We assume that the image is unsigned char, but just in case it isn't we make it so:
	dataImage->Cast<unsigned char>();
	unsigned char *dptr = dataImage->GetSlicePtr<unsigned char>(0,channelNumber,0);	//Expects grayscale image
	//also get a pointer to the label image
	unsigned short *lptr = labelImage->GetSlicePtr<unsigned short>(0,channelNumber,0);	//Expects grayscale image

	if(NucleusSeg)
		delete NucleusSeg;
	NucleusSeg = new yousef_nucleus_seg();

	//create std vectors of the points
	//I am doing that because I want to make yousef_seg isolated from ftk
	std::vector<int> p1;
	std::vector<int> p2;	
	p1.push_back(x1);
	p1.push_back(y1);
	p1.push_back(z1);
	p2.push_back(x2);
	p2.push_back(y2);
	p2.push_back(z2);
	
	int newID = 0;
	if(size[1] == 1)
		newID = NucleusSeg->AddObject2D(dptr, lptr, p1,p2,size, maxID);
	else
		newID = NucleusSeg->AddObject(dptr, lptr, p1,p2,size, maxID);

	if(newID == 0)
		return 0;
	else
		maxID = newID;

	lastRunStep = 4;
	this->GetResultImage();

	//update the corner points
	P1.z = z1;
	P2.z = z2;
	ftk::Object::Box region;
	region.min = P1;
	region.max = P2;
	

	//Calculate features using feature filter
	typedef unsigned char IPixelT;
	typedef unsigned short LPixelT;
	typedef itk::Image< IPixelT, 3 > IImageT;
	typedef itk::Image< LPixelT, 3 > LImageT;

	dataImage->Cast<IPixelT>();
	labelImage->Cast<LPixelT>();

	IImageT::Pointer itkIntImg = dataImage->GetItkPtr<IPixelT>(0,0);
	LImageT::Pointer itkLabImg = labelImage->GetItkPtr<LPixelT>(0,0);

	IImageT::RegionType intRegion;
	IImageT::SizeType intSize;
	IImageT::IndexType intIndex;
	LImageT::RegionType labRegion;
	LImageT::SizeType labSize;
	LImageT::IndexType labIndex;

	intIndex[0] = region.min.x;
	intIndex[1] = region.min.y;
	intIndex[2] = region.min.z;
	intSize[0] = region.max.x - region.min.x + 1;
	intSize[1] = region.max.y - region.min.y + 1;
	intSize[2] = region.max.z - region.min.z + 1;

	labIndex[0] = region.min.x;
	labIndex[1] = region.min.y;
	labIndex[2] = region.min.z;
	labSize[0] = region.max.x - region.min.x + 1;
	labSize[1] = region.max.y - region.min.y + 1;
	labSize[2] = region.max.z - region.min.z + 1;

	intRegion.SetSize(intSize);
    intRegion.SetIndex(intIndex);
    itkIntImg->SetRequestedRegion(intRegion);

    labRegion.SetSize(labSize);
    labRegion.SetIndex(labIndex);
    itkLabImg->SetRequestedRegion(labRegion);

	typedef ftk::LabelImageToFeatures< IPixelT, LPixelT, 3 > FeatureCalcType;
	FeatureCalcType::Pointer labFilter = FeatureCalcType::New();
	labFilter->SetImageInputs( itkIntImg, itkLabImg );
	labFilter->SetLevel(3);
	labFilter->ComputeHistogramOn();
	labFilter->ComputeTexturesOn();
	labFilter->Update();

	myObjects.push_back( GetNewObject(newID, labFilter->GetFeatures(newID) ) );
	
	ftk::Object::EditRecord record;
	record.date = TimeStamp();
	std::string msg = "ADDED";
	record.description = msg;
	myObjects.back().AddEditRecord(record);
	IdToIndexMap[newID] = (int)myObjects.size() - 1;

	editsNotSaved = true;
	return newID;
}

bool NuclearSegmentation::Delete(vector<int> ids)
{
	if(!labelImage)
		return false;

	for(int i=0; i<(int)ids.size(); ++i)
	{
		//Find the object with the ID
		int index = GetObjectIndex(ids.at(i),"nucleus");
		if (index < 0) return false;

		// 1. Invalidate
		myObjects.at( index ).SetValidity(ftk::Object::DELETED);
		// 2. Add to Edit Record
		ftk::Object::EditRecord record;
		record.date = TimeStamp();
		record.description = "DELETED";
		myObjects.at( index ).AddEditRecord(record);
		ReassignLabel(ids.at(i),0);
	}

	editsNotSaved = true;
	return true;
}
bool NuclearSegmentation::DeleteInit(ftk::Object::Point P1)
{
	//if no label (segmentation) or no data image is available then return
	if(!labelImage || !dataImage)
	{
		errorMessage = "label image or data image doesn't exist";			
		return false;
	}	
	
	//Apply the splitting
	bool ids_ok = NucleusSeg->DeleteInit(P1);
		
	return ids_ok;
}

ftk::Object NuclearSegmentation::GetNewObject(int id, IntrinsicFeatures *features )
{
	Object object("nucleus");
	object.SetId(id);
	object.SetValidity(ftk::Object::VALID);
	object.SetDuplicated(0);
	object.SetClass(-1);

	if(features == NULL)
		return object;

	Object::Point c;
	c.x = (int)features->Centroid[0];
	c.y = (int)features->Centroid[1];
	c.z = (int)features->Centroid[2];
	c.t = 0;
	object.SetCentroid(c);

	Object::Box b;
	b.min.x = (int)features->BoundingBox[0];
	b.max.x = (int)features->BoundingBox[1];
	b.min.y = (int)features->BoundingBox[2];
	b.max.y = (int)features->BoundingBox[3];
	b.min.z = (int)features->BoundingBox[4];
	b.max.z = (int)features->BoundingBox[5];
	b.min.t = 0;
	b.max.t = 0;
	object.SetBoundingBox(b);

	vector< float > f(0);
	for (int i=0; i< IntrinsicFeatures::N; ++i)
	{
		f.push_back( features->ScalarFeatures[i] );
	}

	object.SetFeatures( f );

	return object;
}


//This function finds the bounding box of the object with id "fromId",
// and uses that area to change the pixels to "toId"
void NuclearSegmentation::ReassignLabel(int fromId, int toId)
{
	int C = labelImage->Size()[3];
	int R = labelImage->Size()[2];
	int Z = labelImage->Size()[1];

	ftk::Object::Box region = myObjects.at( GetObjectIndex(fromId,"nucleus") ).GetBoundingBox();

	if(region.min.x < 0) region.min.x = 0;
	if(region.min.y < 0) region.min.y = 0;
	if(region.min.z < 0) region.min.z = 0;
	if(region.max.x >= C) region.max.x = C-1;
	if(region.max.y >= R) region.max.y = R-1;
	if(region.max.z >= Z) region.max.z = Z-1;

	for(int z = region.min.z; z <= region.max.z; ++z)
	{
		for(int r=region.min.y; r <= region.max.y; ++r)
		{
			for(int c=region.min.x; c <= region.max.x; ++c)
			{
				int pix = (int)labelImage->GetPixel(0,0,z,r,c);
				if( pix == fromId )
					labelImage->SetPixel(0,0,z,r,c,toId);
			}
		}
	}
}

//This function finds the bounding box of the objects with ids "fromIds",
// and uses that area to change the pixels to "toId" in 1 pass through the whole region
void NuclearSegmentation::ReassignLabels(vector<int> fromIds, int toId, ftk::Object::Box region)
{
	int C = labelImage->Size()[3];
	int R = labelImage->Size()[2];
	int Z = labelImage->Size()[1];

	if(region.min.x < 0) region.min.x = 0;
	if(region.min.y < 0) region.min.y = 0;
	if(region.min.z < 0) region.min.z = 0;
	if(region.max.x >= C) region.max.x = C-1;
	if(region.max.y >= R) region.max.y = R-1;
	if(region.max.z >= Z) region.max.z = Z-1;

	for(int z = region.min.z; z <= region.max.z; ++z)
	{
		for(int r=region.min.y; r <= region.max.y; ++r)
		{
			for(int c=region.min.x; c <= region.max.x; ++c)
			{
				int pix = (int)labelImage->GetPixel(0,0,z,r,c);
				for(int i = 0; i < (int)fromIds.size(); ++i)
				{
					if( pix == fromIds.at(i) )
						labelImage->SetPixel(0,0,z,r,c,toId);
				}
			}
		}
	}
}

string NuclearSegmentation::TimeStamp()
{
	time_t rawtime;
	struct tm *timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	string dt = asctime(timeinfo);
	size_t end = dt.find('\n');
	dt.erase(end);
	return dt;
}

int NuclearSegmentation::GetObjectIndex(int objectID, string type)
{
	//Find the object with the ID
	for( int i=0; i<(int)myObjects.size(); ++i )
	{
		ftk::Object obj = myObjects[i];
		if (obj.GetId() == objectID && obj.GetType() == type)
		{
			return i;
		}
	}
	return -1;
}



//This function runs graph coloring
bool NuclearSegmentation::RunGraphColoring(std::string labelname, std::string filename)
{
	//get the label image (if not already done)
	std::cout<<"Loading Label Image ... ";
	labelImage = ftk::Image::New();
	labelImage->LoadFile(labelname);
	std::cout<<"done!"<<endl;

    //int*** labs_im;
    int max_lab;
    //int ncolors;
    int** RAG;    
    int* ColorOut;        
	int L, L1, L2, L3, L4, L5, L6, L7;
	
	int c = labelImage->Size()[3];
	int r = labelImage->Size()[2];
	int z = labelImage->Size()[1];	
	unsigned short* labs_vals = static_cast<unsigned short*> (labelImage->GetDataPtr(0,0));
	
	//get the maximum label
	std::cout<<"image size is "<<r<<"x"<<c<<"x"<<z<<std::endl;	
	max_lab = 0;
	for(int i=0; i<r-1; i++)
        {        		
        for(int j=0; j<c-1; j++)
        {						
			for(int k=0; k<z-1; k++)
			{	
				if((int)labs_vals[(k*r*c)+(j*r)+i]>max_lab)
					max_lab = (int)labs_vals[(k*r*c)+(j*r)+i];
			}
		}
	}
	std::cout<<"The maximum cell label is "<<max_lab<<std::endl;
	  
        
    //Build the region adjacency graph    
    std::cout<<"Building Region Adjacency Graph...";
    RAG = (int **) malloc(max_lab*sizeof(int*));
    for(int i=0; i<max_lab; i++)
    {        
		RAG[i] = (int *) malloc(max_lab*sizeof(int));
        for(int j=0; j<max_lab; j++)
            RAG[i][j] = 0;
    }
    
	
    for(int i=0; i<r-1; i++)
    {        
        for(int j=0; j<c-1; j++)
        {	
			for(int k=0; k<z-1; k++)
			{
				L = labs_vals[(k*r*c)+(j*r)+i];
				if( L == 0)
					continue;
				else
				{			
					L1 = labs_vals[(k*r*c)+(j*r)+(i+1)]; 
					L2 = labs_vals[(k*r*c)+((j+1)*r)+i];
					L3 = labs_vals[(k*r*c)+((j+1)*r)+(i+1)];
					L4 = labs_vals[((k+1)*r*c)+(j*r)+i];
					L5 = labs_vals[((k+1)*r*c)+((j+1)*r)+i];
					L6 = labs_vals[((k+1)*r*c)+(j*r)+(i+1)];
					L7 = labs_vals[((k+1)*r*c)+((j+1)*r)+(i+1)];

					if(L!=L1 && L1!=0)
						RAG[L-1][L1-1] = RAG[L1-1][L-1] = 1;
					if(L!=L2 && L2!=0)
						RAG[L-1][L2-1] = RAG[L2-1][L-1] = 1;
					if(L!=L3 && L3!=0)
						RAG[L-1][L3-1] = RAG[L3-1][L-1] = 1;
					if(L!=L4 && L4!=0)
						RAG[L-1][L4-1] = RAG[L4-1][L-1] = 1;
					if(L!=L5 && L5!=0)
						RAG[L-1][L5-1] = RAG[L5-1][L-1] = 1;
					if(L!=L6 && L6!=0)
						RAG[L-1][L6-1] = RAG[L6-1][L-1] = 1;
					if(L!=L7 && L7!=0)
						RAG[L-1][L7-1] = RAG[L7-1][L-1] = 1;
				}
            }                		
        }		
    }    
	std::cout<<"done!"<<endl;

    //copy the RAG into an std vector of vectors
	std::vector<std::vector<int> > MAP;
	MAP.resize(max_lab);
    std::vector<std::vector<int> > MAP2;
	MAP2.resize(max_lab);
	
	ColorOut = (int *) malloc(max_lab*sizeof(int));
	for(int i=0; i<max_lab; i++)
	{	
		ColorOut[i] = 0;
		int isIsolated = 1;
		for(int j=0; j<max_lab; j++)
		{
			if(RAG[i][j]==1)
            {
				MAP[i].push_back(j+1);   
				isIsolated = 0;
            }
		} 
		if(isIsolated==1)
			ColorOut[i] = 1;
		free(RAG[i]);
	}    
	free(RAG);
    
	       
    //start the graph coloring using Sumit's sequential coloring code
    GVC* Gcol = new GVC(); 			
 	Gcol->sequential_coloring(max_lab,  max_lab, ColorOut, MAP );
	int numColors = 0;
	for(int i=0; i<max_lab; i++)
	{
		int c = ColorOut[i]+1;
		if(c>numColors)
			numColors=c;
	}
    std::cout<<"Graph Coloring Done"<<endl;
	//write the resulting colors into a file
	FILE *fp = fopen(filename.c_str(),"w");
	
	if(fp == NULL)
	{
		fprintf(stderr,"can't open %s for writing\n",filename.c_str());
		exit(1);
	}
	for(int i=0; i<max_lab; i++)
	{
		fprintf(fp,"%d\n",ColorOut[i]+1);
		
	}
	fclose(fp);
	//Try this: save the colors into the classes list
	classes.clear();
	for(int i=0; i<numColors; i++)
		classes.push_back(i+1);

			

	//Cast the label Image & Get ITK Pointer
	typedef unsigned short PixelType;
	typedef itk::Image<PixelType, 3> ImageType;
	labelImage->Cast<PixelType>();
	ImageType::Pointer img = labelImage->GetItkPtr<PixelType>(0,0);

	//Create an image for each class:	
	std::cout<<"Creating an image for each class...";
	std::vector<ImageType::Pointer> outImgs;
	for(int i=0; i<numColors; ++i)
	{
		ImageType::Pointer tmp = ImageType::New();   
		tmp->SetOrigin( img->GetOrigin() );
		tmp->SetRegions( img->GetLargestPossibleRegion() );
		tmp->SetSpacing( img->GetSpacing() );
		tmp->Allocate();
		tmp->FillBuffer(0);
		tmp->Update();

		outImgs.push_back(tmp);
	}

	//create lists of object ids in each class:
	std::vector< std::set<int> > objClass(numColors);
	for(int i=0; i<max_lab; ++i)
	{
		int c = ColorOut[i]+1;
		int id = i+1;
		int p = 0;
		for(int j=0; j<numColors; ++j)
		{
			if(c == classes.at(j))
				break;
			++p;
		}
		if(p < numColors)
			objClass.at(p).insert(id);
	}

	//Iterate through Image & populate all of the other images
	typedef itk::ImageRegionConstIteratorWithIndex< ImageType > IteratorType;
	IteratorType it(img,img->GetRequestedRegion());
	for(it.GoToBegin(); !it.IsAtEnd(); ++it)
	{
		int id = it.Get();
		for(int j=0; j<numColors; ++j)
		{
			if( objClass.at(j).find(id) != objClass.at(j).end() )
			{
				outImgs.at(j)->SetPixel(it.GetIndex(), 1); 
			}
		}	
	}

	//Now Write All of the Images to File
	typedef itk::ImageFileWriter<ImageType> WriterType;
	for(int i=0; i<numColors; ++i)
	{
		WriterType::Pointer writer = WriterType::New();
		size_t pos = filename.find_last_of(".");
		std::string base = filename.substr(0,pos);
		std::string ext = filename.substr(pos);
		std::string colorImage;
		writer->SetFileName( base + "_class" + NumToString(classes.at(i)) + ext );
		writer->SetInput( outImgs.at(i) );
		colorImage = writer->GetFileName();
		this->colorImages.push_back(colorImage);

		try
		{
			writer->Update();
		}
		catch( itk::ExceptionObject & excp )
		{
			std::cerr << excp << std::endl;
			writer = 0;
			errorMessage = "Problem saving file to disk";
			return false;
		}
		
		writer = 0;
	}
	std::cout<<"done!"<<std::endl;
	return 1;
}

bool NuclearSegmentation::RestoreFromXML(std::string filename)
{
	dataFilename.clear();		
	labelFilename.clear(); 
	myParameters.clear();
	myObjects.clear();
	featureNames.clear();
	IdToIndexMap.clear();

	size_t pos = filename.find_last_of("/\\");
	std::string path = filename.substr(0,pos);

	TiXmlDocument doc;
	if ( !doc.LoadFile( filename.c_str() ) )
	{
		errorMessage = "Unable to load XML File";
		return 0;
	}

	TiXmlElement* rootElement = doc.FirstChildElement();
	const char* docname = rootElement->Value();
	if ( strcmp( docname, "NuclearSegmentation" ) != 0 && strcmp( docname, "SegmentationResult" ) != 0 )
	{
		errorMessage = "Incorrect XML root Element: ";
		errorMessage.append(rootElement->Value());
		return 0;
	}

	//Parents we know of: datafilename,resultfilename,object,parameter
	TiXmlElement* parentElement = rootElement->FirstChildElement();
	while (parentElement)
	{
		const char * parent = parentElement->Value();

		if ( strcmp( parent, "datafile" ) == 0 )
		{
			std::string dname = parentElement->GetText();
			dataFilename = path + "/" + dname;
		}
		else if ( strcmp( parent, "resultfile" ) == 0 )
		{
			std::string rname = parentElement->GetText();
			labelFilename = path + "/" + rname;
		}
		else if ( strcmp( parent, "parameter" ) == 0 )
		{
			Parameter p;
			p.name = parentElement->Attribute("name");
			p.value = atoi( parentElement->GetText() );
			myParameters.push_back( p );
		}
		else if ( strcmp( parent, "object" ) == 0 )
		{
			Object o = parseObject(parentElement);
			if ( o.GetType() != "null" )
			{
				myObjects.push_back( o );
				int id = o.GetId();
				IdToIndexMap[id] = (int)myObjects.size() - 1;
				if(id > maxID) maxID = id;
			}
		}
		else
		{
			errorMessage = "Unrecognized parent element: ";
			errorMessage.append(parent);
			return 0;
		}
		parentElement = parentElement->NextSiblingElement();
	} // end while(parentElement)

	//doc.close();

	if(!LoadData())
		return false;

	if(!LoadLabel())
		return false;

	editsNotSaved = false;
	return true;
}

Object NuclearSegmentation::parseObject(TiXmlElement *objectElement)
{
	if ( strcmp( objectElement->Value(), "object" ) != 0 )
	{
		errorMessage = "This is not an object: ";
		errorMessage.append(objectElement->Value());
		return Object("null");
	}

	Object object( objectElement->Attribute("type") );

	TiXmlElement *member = objectElement->FirstChildElement();
	while(member)
	{
		const char* memberName = member->Value();
		if ( strcmp( memberName, "id" ) == 0 )
		{
			object.SetId( atoi( member->GetText() ) );
		}
		else if ( strcmp( memberName, "validity" ) == 0 )
		{
			object.SetValidity( atoi( member->GetText() ) );
		}
		else if ( strcmp( memberName, "duplicated" ) == 0 )
		{
			object.SetDuplicated( atoi( member->GetText() ) );
		}
		else if ( strcmp( memberName, "class" ) == 0 )
		{
			object.SetClass( atoi( member->GetText() ) );
		}
		else if ( strcmp( memberName, "center") == 0 )
		{
			object.SetCentroid( parseCenter(member) );
		}
		else if ( strcmp( memberName, "bound") == 0 )
		{
			object.SetBoundingBox( parseBound(member) );
		}
		else if ( strcmp( memberName, "features") == 0 )
		{
			object.SetFeatures( parseFeatures(member) );
		}
		else if ( strcmp( memberName, "EditHistory") == 0 )
		{
			TiXmlElement *record = member->FirstChildElement();
			while(record)
			{
				if ( strcmp( record->Value(), "record" ) == 0 )
				{
					Object::EditRecord r;
					r.date = record->Attribute("date");
					r.description = record->GetText();
					object.AddEditRecord( r );
				}
				record = record->NextSiblingElement();
			}
		}
		else
		{
			errorMessage = "Unrecognized object member: ";
			errorMessage.append(memberName);
			return Object("null");
		}
		member = member->NextSiblingElement();
	}

	return object;
}

Object::Point NuclearSegmentation::parseCenter(TiXmlElement *centerElement)
{
	Object::Point center;

	TiXmlElement *coord = centerElement->FirstChildElement();
	while(coord)
	{
		const char* coordName = coord->Value();
		if ( strcmp( coordName, "x" ) == 0 )
		{
			center.x = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "y" ) == 0 )
		{
			center.y = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "z" ) == 0 )
		{
			center.z = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "t" ) == 0 )
		{
			center.t = atoi( coord->GetText() );
		}
		coord = coord->NextSiblingElement();
	}
	return center;
}

Object::Box NuclearSegmentation::parseBound(TiXmlElement *boundElement)
{
	Object::Box bound;

	TiXmlElement *coord = boundElement->FirstChildElement();
	while(coord)
	{
		const char* coordName = coord->Value();
		if ( strcmp( coordName, "xmin" ) == 0 )
		{
			bound.min.x = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "ymin" ) == 0 )
		{
			bound.min.y = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "zmin" ) == 0 )
		{
			bound.min.z = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "tmin" ) == 0 )
		{
			bound.min.t = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "xmax" ) == 0 )
		{
			bound.max.x = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "ymax" ) == 0 )
		{
			bound.max.y = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "zmax" ) == 0 )
		{
			bound.max.z = atoi( coord->GetText() );
		}
		else if ( strcmp( coordName, "tmax" ) == 0 )
		{
			bound.max.t = atoi( coord->GetText() );
		}
		coord = coord->NextSiblingElement();
	}
	return bound;
}

vector< float > NuclearSegmentation::parseFeatures(TiXmlElement *featureElement)
{
	vector< float > tempFeatures(0);
	vector< string > tempNames(0); 

	//Extract all of the features for this object
	TiXmlElement *feature = featureElement->FirstChildElement();
	while(feature)
	{
		tempNames.push_back( feature->Value() );
		tempFeatures.push_back( float( atof( feature->GetText() ) ) );
		
		feature = feature->NextSiblingElement();
	}

	//Now test for existing featureNames
	if (featureNames.size() == 0) //Meaning hasn't been set yet
	{
		featureNames = tempNames; //Set my feature names to these names
		return tempFeatures;
	}

	//Have already set the names, so need to make sure we return the features in the correct order
	if( featureNames.size() != tempNames.size() )
	{
		errorMessage = "Features do not match";
		return vector< float >(0);
	}
		
	vector<float> retFeatures( tempFeatures.size() );
	unsigned int numMatches = 0;
	for (unsigned int i=0; i<tempFeatures.size(); ++i)
	{
		string featName = tempNames[i];
		for (unsigned int j=0; j<featureNames.size(); ++j)
		{
			if( featName == featureNames[j] )
			{
				retFeatures[j] = tempFeatures[i];
				++numMatches;
			}
		}
	}

	if ( numMatches != tempFeatures.size() )
	{
		errorMessage = "Features do not match";
		return vector< float >(0);
	}

	return retFeatures;
}

//*******************************************************************************************
// This functions writes the objects to an XML file using a particular XML Format!!
//*******************************************************************************************
bool NuclearSegmentation::WriteToXML(std::string filename)
{
	TiXmlDocument doc;   
 
	TiXmlElement * root = new TiXmlElement( "NuclearSegmentation" );  
	doc.LinkEndChild( root );  
	root->SetAttribute("program", "Yousef_Nucleus_Seg");

	TiXmlComment * comment = new TiXmlComment();
	comment->SetValue(" Segmentation Results/Parameters/Features/Edits " );  
	root->LinkEndChild( comment );  
 
	TiXmlElement * dfile = new TiXmlElement("datafile");
	size_t pos1 = dataFilename.find_last_of("/\\");
	std::string dName = dataFilename.substr(pos1+1);
	dfile->LinkEndChild( new TiXmlText( dName.c_str() ) );
	root->LinkEndChild(dfile);

	TiXmlElement *rfile = new TiXmlElement("resultfile");
	size_t pos2 = labelFilename.find_last_of("/\\");
	std::string lName = labelFilename.substr(pos2+1);
	rfile->LinkEndChild( new TiXmlText( lName.c_str() ) );
	root->LinkEndChild(rfile);

	//Attach parameters
	for (unsigned int pnum = 0; pnum < myParameters.size(); ++pnum)
	{
		TiXmlElement *element = new TiXmlElement("parameter");
		element->SetAttribute( "name", myParameters[pnum].name );
		element->LinkEndChild( new TiXmlText( NumToString(myParameters[pnum].value) ) );
		root->LinkEndChild(element);

	}

	//Attach Objects
	for (unsigned int onum=0; onum<myObjects.size(); ++onum)
	{
		root->LinkEndChild( GetObjectElement(myObjects[onum]) );
	}

	doc.SaveFile( filename.c_str() );

	return true;
}

TiXmlElement* NuclearSegmentation::GetObjectElement(Object object)
{
	TiXmlElement *objectElement = new TiXmlElement("object");
	objectElement->SetAttribute("type", object.GetType());

	if ( object.GetId() != -1 )
	{
		TiXmlElement *idElement = new TiXmlElement("id");
		idElement->LinkEndChild( new TiXmlText( NumToString(object.GetId()) ) );
		objectElement->LinkEndChild(idElement);
	}
	if ( object.GetValidity() != -1 )
	{
		TiXmlElement *vElement = new TiXmlElement("validity");
		vElement->LinkEndChild( new TiXmlText( NumToString(object.GetValidity()) ) );
		objectElement->LinkEndChild(vElement);
	}
	if ( object.GetDuplicated() != -1 )
	{
		TiXmlElement *dElement = new TiXmlElement("duplicated");
		dElement->LinkEndChild( new TiXmlText( NumToString(object.GetDuplicated()) ) );
		objectElement->LinkEndChild(dElement);
	}
	if ( object.GetClass() != -1 )
	{
		TiXmlElement *cElement = new TiXmlElement("class");
		cElement->LinkEndChild( new TiXmlText( NumToString(object.GetClass()) ) );
		objectElement->LinkEndChild(cElement);
	}
	Object::Point center = object.GetCentroid();
	if(center.x != 0 || center.y != 0 || center.z != 0 || center.t != 0)
	{
		TiXmlElement *oElement = new TiXmlElement("center");
		TiXmlElement *xElement = new TiXmlElement("x");
		xElement->LinkEndChild( new TiXmlText( NumToString(center.x) ) );
		oElement->LinkEndChild(xElement);
		TiXmlElement *yElement = new TiXmlElement("y");
		yElement->LinkEndChild( new TiXmlText( NumToString(center.y) ) );
		oElement->LinkEndChild(yElement);
		TiXmlElement *zElement = new TiXmlElement("z");
		zElement->LinkEndChild( new TiXmlText( NumToString(center.z) ) );
		oElement->LinkEndChild(zElement);
		TiXmlElement *tElement = new TiXmlElement("t");
		tElement->LinkEndChild( new TiXmlText( NumToString(center.t) ) );
		oElement->LinkEndChild(tElement);
		objectElement->LinkEndChild(oElement);
	}
	Object::Box bound = object.GetBoundingBox();
	if(bound.max.x != center.x || bound.min.x != center.x ||
	   bound.max.y != center.y || bound.min.y != center.y ||
	   bound.max.z != center.z || bound.min.z != center.z ||
	   bound.max.t != center.t || bound.min.t != center.t)
	{
		TiXmlElement *bElement = new TiXmlElement("bound");
		TiXmlElement *xminElement = new TiXmlElement("xmin");
		xminElement->LinkEndChild( new TiXmlText( NumToString(bound.min.x) ) );
		bElement->LinkEndChild(xminElement);
		TiXmlElement *yminElement = new TiXmlElement("ymin");
		yminElement->LinkEndChild( new TiXmlText( NumToString(bound.min.y) ) );
		bElement->LinkEndChild(yminElement);
		TiXmlElement *zminElement = new TiXmlElement("zmin");
		zminElement->LinkEndChild( new TiXmlText( NumToString(bound.min.z) ) );
		bElement->LinkEndChild(zminElement);
		TiXmlElement *tminElement = new TiXmlElement("tmin");
		tminElement->LinkEndChild( new TiXmlText( NumToString(bound.min.t) ) );
		bElement->LinkEndChild(tminElement);
		TiXmlElement *xmaxElement = new TiXmlElement("xmax");
		xmaxElement->LinkEndChild( new TiXmlText( NumToString(bound.max.x) ) );
		bElement->LinkEndChild(xmaxElement);
		TiXmlElement *ymaxElement = new TiXmlElement("ymax");
		ymaxElement->LinkEndChild( new TiXmlText( NumToString(bound.max.y) ) );
		bElement->LinkEndChild(ymaxElement);
		TiXmlElement *zmaxElement = new TiXmlElement("zmax");
		zmaxElement->LinkEndChild( new TiXmlText( NumToString(bound.max.z) ) );
		bElement->LinkEndChild(zmaxElement);
		TiXmlElement *tmaxElement = new TiXmlElement("tmax");
		tmaxElement->LinkEndChild( new TiXmlText( NumToString(bound.max.t) ) );
		bElement->LinkEndChild(tmaxElement);
		objectElement->LinkEndChild(bElement);
	}
	if(featureNames.size() > 0)
	{
		TiXmlElement *fsElement = new TiXmlElement("features");
		vector<float> features = object.GetFeatures();
		for(unsigned int f=0; f<features.size(); ++f)
		{
			TiXmlElement *fElement = new TiXmlElement( featureNames[f].c_str() );
			fElement->LinkEndChild( new TiXmlText( NumToString(features[f]) ) );
			fsElement->LinkEndChild(fElement);
		}
		objectElement->LinkEndChild(fsElement);
	}
	vector<Object::EditRecord> records = object.getHistory();
	if(records.size() > 0)
	{
		TiXmlElement *histElement = new TiXmlElement("EditHistory");
		for (unsigned int r=0; r<records.size(); ++r)
		{
			TiXmlElement *recElement = new TiXmlElement("record");
			recElement->SetAttribute("date",records[r].date);
			recElement->LinkEndChild( new TiXmlText( records[r].description ) );
			histElement->LinkEndChild(recElement);
		}
		objectElement->LinkEndChild(histElement);
	}
	return objectElement;
}

bool NuclearSegmentation::WriteToMETA(std::string filename)
{
	//This function writes the features to a text file that can be read be MetaNeural program
	ofstream outFile; 
	outFile.open(filename.c_str(), ios::out | ios::trunc );
	if ( !outFile.is_open() )
	{
		std::cerr << "Failed to Load Document: " << outFile << std::endl;
		return false;
	}
	//Now write out the features
	for(unsigned int obj = 0; obj < myObjects.size(); ++obj)
	{
		if( myObjects.at(obj).GetValidity() != ftk::Object::VALID )
			continue;

		vector<float> feats = myObjects.at(obj).GetFeatures();
		for(unsigned int f = 0; f < feats.size(); ++f)
		{
			outFile << NumToString(feats.at(f)) << "\t";
		}
		outFile << (int)myObjects.at(obj).GetClass() << "\t";
		outFile << myObjects.at(obj).GetId() << endl;
	}
	outFile.close();

	//Now print header file:
	filename.append(".header");
	outFile.open(filename.c_str(), std::ios::out | std::ios::trunc );
	if ( !outFile.is_open() )
	{
		std::cerr << "Failed to Load Document: " << outFile << std::endl;
		return false;
	}

	for(int i=0; i<(int)featureNames.size(); ++i)
	{
		outFile << featureNames.at(i) << "\n";
	}

	outFile << "CLASS" << "\n";
	outFile << "ID" << std::endl;

	outFile.close();

	return true;
}

bool NuclearSegmentation::WriteToLibSVM(std::string filename)
{
	//This function writes the features to a text file that can be read by libsvm program

	ofstream outFile; 
	outFile.open(filename.c_str(), ios::out | ios::trunc );
	if ( !outFile.is_open() )
	{
		std::cerr << "Failed to Load Document: " << outFile << std::endl;
		return false;
	}
	//Now write out the features
	for(unsigned int obj = 0; obj < myObjects.size(); ++obj)
	{
		if( myObjects.at(obj).GetValidity() == ftk::Object::VALID )
			continue;

		outFile << myObjects.at(obj).GetId() << " ";			//This should be the class

		vector<float> feats = myObjects.at(obj).GetFeatures();
		for(unsigned int f = 0; f < feats.size(); ++f)
		{
			outFile << f+1 << ":" << NumToString(feats.at(f)) << " ";	//FeatureNumber:FeatureValue
		}
		outFile << std::endl;
	}
	outFile.close();
	return true;
}


Object* NuclearSegmentation::GetObjectPtr(int id)
{
	int index = IdToIndexMap[id];
	return &myObjects.at(index);
}

//Check to see if the file will filename fname exists in 
// the project path.
bool NuclearSegmentation::FileExists(std::string filename)
{
	FILE * pFile = fopen (filename.c_str(),"r");
	if (pFile==NULL)
	{
		return false;
	}
	fclose (pFile);
	return true;
}

string NuclearSegmentation::NumToString(double d)
{
	stringstream out;
	out << setprecision(2) << fixed << d;	//Default is to use 2 decimal places
	return out.str();
}

string NuclearSegmentation::NumToString(int i)
{
	stringstream out;
	out << i ;	 
	return out.str();
}

string NuclearSegmentation::NumToString(double d, int p)
{
	stringstream out;
	out << setprecision(p) << fixed << d;	
	return out.str();
}

 //end namespace ftk


void NuclearSegmentation::Cleandptr(unsigned short* p, vector<int> dim){
	int ctr =0;
	
if(dim.size() ==3) {
	for (int index1=0;index1<dim[2];index1++)
		{
			for(int index2=0;index2<dim[1];index2++)
				{
					for(int index3=0;index3<dim[0];index3++)
						{
						    //if(p[ctr]<0)klkl 
							if(p[ctr]==65535) 
							{
								p[ctr]=0;
								this->negativeseeds.push_back(ctr);									
							}
					ctr++;
						}
				}
		}
	}
else
{
for(int index1=0;index1<dim[1];index1++)
	{
	for(int index2=0;index2<dim[0];index2++)
		{
			if(p[ctr]==65535/*<0*/) 
				{
				p[ctr]=0;
				this->negativeseeds.push_back(ctr);									
				}
				ctr++;
		}
	}
}

}

void NuclearSegmentation::Restoredptr(unsigned short* p)
{
 	for(list<int>::iterator index =this->negativeseeds.begin();index!=this->negativeseeds.end();++index)
		{
	    		p[*index]=65535;//-1;					
		}
	this->negativeseeds.clear();
}

vector<Seed> NuclearSegmentation::getSeeds()
{
	return NucleusSeg->getSeedsList();
}
}

