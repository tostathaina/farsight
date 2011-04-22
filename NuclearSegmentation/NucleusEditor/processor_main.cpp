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
#include "ftkProjectProcessor.h"

void usage(const char *funcName);

int main(int argc, char *argv[])
{ 
	if( argc < 5 )
	{
		usage(argv[0]);
		std::cerr << "PRESS ENTER TO EXIT\n";
		getchar();
		return EXIT_FAILURE;
	}

	std::string MyName        = argv[0];					//In case the CWD is not the path of the executable
	std::string inputFilename = argv[1];					// Name of the input image;
	std::string labelFilename = argv[2];					// Name of the label image to apply
	std::string tableFilename = argv[3];					// Name of the table file;
	std::string definitionFilename = argv[4];				// Name of the process definition file

	
	//Try to load the input image:
	ftk::Image::Pointer myImg = NULL;
	if( ftk::GetExtension(inputFilename) == "xml" )
	{			
		myImg = ftk::LoadXMLImage(inputFilename);
	}
	else
	{
		myImg = ftk::Image::New();
		if( !myImg->LoadFile(inputFilename) )
			myImg = NULL;
	}
	
	if(!myImg)
	{
		std::cerr << "COULD NOT LOAD INPUT IMAGE!!\n";
		usage(argv[0]);
		std::cerr << "PRESS ENTER TO EXIT\n";
		getchar();
		return EXIT_FAILURE;
	}
	
	//Try to load the Label image:
	ftk::Image::Pointer labImg;// = NULL;
	if( ftk::FileExists(labelFilename) )
	{
		if( ftk::GetExtension(labelFilename) == "xml" )
		{
			labImg = ftk::LoadXMLImage(labelFilename);
		}
		else
		{
			labImg = ftk::Image::New();
			if( !labImg->LoadFile(labelFilename) )
				labImg = NULL;
		}
	}

	//Try to load the table:
	vtkSmartPointer<vtkTable> table = NULL;
	if( ftk::FileExists(tableFilename) )
	{
		table = ftk::LoadTable(tableFilename);
	}

	//Load up the definition
	ftk::ProjectDefinition projectDef;
	if( !projectDef.Load(definitionFilename) )
	{
		std::cerr << "COULD NOT LOAD PROCESS DEFINITION FILE!!\n";
		usage(argv[0]);
		std::cerr << "PRESS ENTER TO EXIT\n";
		getchar();
		return EXIT_FAILURE;
	}

	//Do processing:
	ftk::ProjectProcessor * pProc = new ftk::ProjectProcessor();
	pProc->SetExecPath( ftk::GetFilePath( MyName ) );
	std::cout<<"The executable says my path is: "<<ftk::GetFilePath( MyName )<<std::endl;
	pProc->SetInputImage(myImg);
	pProc->SetPath( ftk::GetFilePath(inputFilename) );
	if(labImg)
		pProc->SetOutputImage(labImg);
	if(table)
		pProc->SetTable(table);
	pProc->SetDefinition(&projectDef);
	pProc->Initialize();

	while(!pProc->DoneProcessing())
		pProc->ProcessNext();

	labImg = pProc->GetOutputImage();
	table = pProc->GetTable();

	//Save results:
	if( ftk::GetExtension(labelFilename) == "xml" )
		ftk::SaveXMLImage(labelFilename, labImg);
	else
		labImg->SaveChannelAs(0, ftk::SetExtension(labelFilename, ""), ftk::GetExtension(labelFilename));

	ftk::SaveTable( tableFilename, table );

	projectDef.Write(definitionFilename);

	delete pProc;

	return EXIT_SUCCESS;

}

void usage(const char *funcName)
{
	std::cout << "USAGE:\n";
	std::cout << " " << funcName << " InputImage LabelImage Table ProcessDefinition \n";
	std::cout << "  All four inputs are filenames\n";
}