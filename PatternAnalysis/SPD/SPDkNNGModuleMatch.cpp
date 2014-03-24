#include "SPDkNNGModuleMatch.h"
#include <QGridLayout>
#include <QLCDNumber>
#include <QFileDialog>
#include <QDir>
#include <fstream>
#include <QMessageBox>
//define NDEBUG
#include <assert.h>
#include "ClusClus/clusclus.h"
#include <cstdlib>
#include <ctime>

using std::ifstream;
using std::endl;

SPDkNNGModuleMatch::SPDkNNGModuleMatch(QWidget *parent) :
    QWidget(parent)
{
	setWindowTitle(QString("SPD by kNNG-module matching"));
	SPDModel = NULL;
	selection = NULL;
	selection2 = NULL;
	graph = NULL;
	simHeatmap = NULL;
	histo = NULL;
	originalHeatmap = NULL;
	progressionHeatmap = NULL;
	autoHeatmapWin = NULL;
	manualHeatmapWin = NULL;
	plot = NULL;
	connectedNum = 0;
	bconnected = true;
	int frameStyle = QFrame::Sunken | QFrame::Panel;

	rawHeatmapButton = new QPushButton(tr("Original Heatmap"), this);

    featureNumLabel = new QLabel(tr("Feature size:"), this);
    featureNum = new QLabel(this);
    featureNum->setFrameStyle(frameStyle);
    sampleNumLabel = new QLabel(tr("Sample size:"), this);
    sampleNum = new QLabel(this);
    sampleNum->setFrameStyle(frameStyle);

    clusterCoherenceLabel = new QLabel(tr("Feature Coherence(0.0 ~ 1.0):"), this);
    clusterCoherenceBox = new QDoubleSpinBox(this);
	clusterCoherenceBox->setValue(0.95);
	clusterCoherenceBox->setRange(0,1); 
	clusterCoherenceBox->setSingleStep(0.1);

    kNearestNeighborLabel = new QLabel(tr("Nearest Neighbor Number:"), this);
    kNearestNeighborBox = new QSpinBox(this);
	kNearestNeighborBox->setValue(4);
	kNearestNeighborBox->setMinimum (0);
	kNearestNeighborBox->setSingleStep(1);

	nBinLabel = new QLabel(tr("Bin size for histogram:"), this);
	nBinBox = new QSpinBox(this);
	nBinBox->setValue(20);
	nBinBox->setMinimum (2);
	nBinBox->setSingleStep(1);
	nsmButton = new QPushButton(tr("Generate NS Matrix"), this);

    autoSelButton = new QPushButton(tr("Select"), this);
	autoSelList = new QLabel(tr("Auto selection:"), this);
	QFont font1 = autoSelList->font();
	font1.setBold(true);
	autoSelList->setFont(font1);

	listbox = new QListWidget( this);
    autoTrendButton = new QPushButton(tr("View Auto Trend"), this);

	manualSelectionLabel = new QLabel(tr("Manual selection:"), this);
	QFont font2 = manualSelectionLabel->font();
	font2.setBold(true);
	manualSelectionLabel->setFont(font2);

	continSelectLabel = new QLabel(tr("Continous:"), this);
	continSelectCheck = new QCheckBox(this);
	psdModuleSelectBox = new QLineEdit(this);
	manualSelButton = new QPushButton(tr("Show NSM"), this);
	manualTrendButton = new QPushButton(tr("View Manual Trend"), this);

	heatmapButton = new QPushButton(tr("MST-ordered Heatmap"), this);
	testButton = new QPushButton(tr("Test"), this);

	nsmButton->setEnabled(true);
	autoSelButton->setEnabled(false);
	manualSelButton->setEnabled(false);
	autoTrendButton->setEnabled(false);
	manualTrendButton->setEnabled(false);
	heatmapButton->setEnabled(false);
	
    QGridLayout *mainLayout = new QGridLayout(this);

    for ( int col = 0; col<= 2; col++)
    {
        mainLayout->setColumnMinimumWidth(col,100);
        mainLayout->setColumnStretch(col, 1);
    }

     for ( int row = 0; row <= 13; row++)
    {
        mainLayout->setRowMinimumHeight(row,20);
        mainLayout->setRowStretch(row, 1);
    }

	mainLayout->addWidget(rawHeatmapButton, 0, 2);

    mainLayout->addWidget(featureNumLabel, 0, 0, 1, 1);
    mainLayout->addWidget(featureNum, 0, 1, 1, 1);

    mainLayout->addWidget(sampleNumLabel, 1, 0, 1, 1);
    mainLayout->addWidget(sampleNum, 1, 1, 1, 1);

	mainLayout->addWidget(clusterCoherenceLabel, 2, 0);
	mainLayout->addWidget(clusterCoherenceBox, 2, 1);

    mainLayout->addWidget(kNearestNeighborLabel, 3, 0);
    mainLayout->addWidget(kNearestNeighborBox, 3, 1);
	
	mainLayout->addWidget(nBinLabel, 4, 0);
	mainLayout->addWidget(nBinBox, 4, 1);
	mainLayout->addWidget(nsmButton,4, 2);

	mainLayout->addWidget(autoSelList, 6, 0);
	mainLayout->addWidget(autoSelButton, 6, 2);
	mainLayout->addWidget(listbox, 7, 0, 2, 2);
	mainLayout->addWidget(autoTrendButton, 8, 2);

	mainLayout->addWidget(manualSelectionLabel, 9, 0);
	mainLayout->addWidget(manualSelButton, 9, 2);
	mainLayout->addWidget(continSelectLabel, 10, 0);
	mainLayout->addWidget(continSelectCheck, 10, 1);
	mainLayout->addWidget(psdModuleSelectBox, 11, 0, 1, 2);
	mainLayout->addWidget(manualTrendButton, 11, 2);

	mainLayout->addWidget(testButton, 13, 0);
	mainLayout->addWidget(heatmapButton, 13, 2);

	mainLayout->setSizeConstraint( QLayout::SetFixedSize);
    setLayout(mainLayout);

	connect(rawHeatmapButton, SIGNAL(clicked()), this, SLOT(showOriginalHeatmap()));
	connect( kNearestNeighborBox, SIGNAL(editingFinished()), this, SLOT(editNearestNeighbor()));
	connect( nsmButton, SIGNAL(clicked()), this, SLOT(generateNSM()));
	connect( autoSelButton, SIGNAL(clicked()), this, SLOT(autoSelection()));
	connect( autoTrendButton, SIGNAL(clicked()), this, SLOT(autoClick()));
	connect( heatmapButton, SIGNAL(clicked()), this, SLOT(showTrendHeatmap()));
	connect( testButton, SIGNAL(clicked()), this, SLOT(TestTrend()));
	connect( manualSelButton, SIGNAL(clicked()), this, SLOT(showNSMForManualSelection()));
	connect( manualTrendButton, SIGNAL(clicked()), this, SLOT(manualClick()));

	SPDModel = new SPDAnalysisModel();
}

SPDkNNGModuleMatch::~SPDkNNGModuleMatch()
{
	delete(SPDModel);
	delete(selection);
	delete(selection2);
}

void SPDkNNGModuleMatch::setModels(vtkSmartPointer<vtkTable> table, ObjectSelection * sels, ObjectSelection * sels2)
{
	if(table == NULL)
	{
		return;
	}
	else
	{
		data = table;
		SPDModel->ParseTraceFile( data);
		this->featureNum->setText( QString::number(this->SPDModel->GetFeatureNum()));
		this->sampleNum->setText( QString::number(this->SPDModel->GetSampleNum()));
	}

	if( sels == NULL)
	{
		selection = new ObjectSelection();
	}
	else
	{
		selection = sels;
	}

	if( sels2 == NULL)
	{
		selection2 = new ObjectSelection();
	}
	else
	{
		selection2 = sels2;
	}

	if(this->simHeatmap)
	{
		delete this->simHeatmap;
	}
	this->simHeatmap = new OrderedHeatmap( this);
	connect( simHeatmap, SIGNAL( SelChanged()), this, SLOT( updateSelMod()));

	if(this->graph)
	{
		delete this->graph;
	}
	this->graph = new GraphWindow( this);

}

void SPDkNNGModuleMatch::showOriginalHeatmap()
{
	vtkSmartPointer<vtkTable> tableAfterCellCluster = SPDModel->GetDataTableAfterCellCluster();
	if( this->originalHeatmap)
	{
		delete this->originalHeatmap;
		this->originalHeatmap = NULL;
	}

	this->originalHeatmap = new TrendHeatmapWindow(tr("Original TrendHeatmapWindow"), this);
	std::vector< int> sampleOrder;
	for( int i = 0; i < tableAfterCellCluster->GetNumberOfRows(); i++)
	{
		sampleOrder.push_back(i);
	}
	std::vector< int> selOrder;
	for( int i = 0; i < tableAfterCellCluster->GetNumberOfColumns() - 1; i++)
	{
		selOrder.push_back(i);
	}
	std::vector< int> unselOrder;

	std::map< int, int> indexMap;
	SPDModel->GetClusterMapping(indexMap);

	this->originalHeatmap->setModelsforSPD( tableAfterCellCluster, selection, sampleOrder, selOrder, unselOrder, &indexMap);
	this->originalHeatmap->showGraphforSPD( selOrder.size(), unselOrder.size(), true);
}

void SPDkNNGModuleMatch::showHeatmapAfterFeatureClustering()
{
	vtkSmartPointer<vtkTable> tableAfterCellCluster = SPDModel->GetDataTableAfterCellCluster();
	if( this->originalHeatmap)
	{
		delete this->originalHeatmap;
		this->originalHeatmap = NULL;
	}

	this->originalHeatmap = new TrendHeatmapWindow(tr("Original TrendHeatmapWindow"),this);
	std::vector< int> sampleOrder;
	for( int i = 0; i < tableAfterCellCluster->GetNumberOfRows(); i++)
	{
		sampleOrder.push_back(i);
	}
	std::vector< int> selOrder;
	std::vector< unsigned int> inputSelOrder;
	for( unsigned int i = 0; i < tableAfterCellCluster->GetNumberOfColumns(); i++)
	{
		inputSelOrder.push_back(i);
	}
	GetFeatureOrder(inputSelOrder, selOrder, unselOrder);

	this->originalHeatmap->setModelsforSPD( tableAfterCellCluster, selection, sampleOrder, selOrder, unselOrder);
	this->originalHeatmap->showGraphforSPD( selOrder.size(), unselOrder.size(), true);
}

void SPDkNNGModuleMatch::clusterFunction()
{
	if ( this->SPDModel->GetFeatureNum() <= 0 && this->SPDModel->GetSampleNum() <= 0)
	{
		QMessageBox mes;
		mes.setText("You haven't loaded the data file!");
		mes.exec();
	}

	std::string clusterCor = this->clusterCoherenceBox->text().toStdString();
	
	try
	{
		this->SPDModel->ClusterAgglomerate( atof(clusterCor.c_str()), 0.9);
		//showHeatmapAfterFeatureClustering();
	}
	catch(...)
	{
		std::cout<< "Clustering exception, please try again!"<<endl;
	}
}

void SPDkNNGModuleMatch::editNearestNeighbor()
{
	QString str = this->kNearestNeighborBox->text();
	int numNeighbor = str.toInt();
	int kNum = this->SPDModel->GetKNeighborNum();
	if( kNum <= 0 || kNum != numNeighbor)
	{
		autoSelButton->setEnabled(TRUE);
		autoTrendButton->setEnabled(FALSE);
		heatmapButton->setEnabled(FALSE);
	}
}

void SPDkNNGModuleMatch::generateNSM()
{
	clusterFunction();
	std::string kNeighbor = this->kNearestNeighborBox->text().toStdString();
	std::string nBin = this->nBinBox->text().toStdString();
	this->SPDModel->ModuleCorrelationMatrixMatch(atoi(kNeighbor.c_str()), atoi(nBin.c_str()));
	autoSelButton->setEnabled(true);
	manualSelButton->setEnabled(true);
}

void SPDkNNGModuleMatch::autoSelection()
{
	double threshold = this->SPDModel->GetAutoSimilarityThreshold();
	std::cout<< "Auto Threshold: "<< threshold<<std::endl;

	std::vector<std::vector<unsigned int> > modID;
	SPDModel->GetSelectedFeaturesModulesByConnectedComponent(threshold, modID);
	
	listbox->clear();
	size_t maxIndex = 0;
	size_t maxSize = 0;
	size_t secMaxSize = 0;
	size_t secondMax = 0;
	for( size_t k = 0; k < modID.size(); k++)
	{
		if( modID[k].size() > maxSize)
		{
			secondMax = maxIndex;
			secMaxSize = maxSize;
			maxIndex = k;
			maxSize = modID[k].size();
		}
		else if( modID[k].size() > secMaxSize)
		{
			secondMax = k;
			secMaxSize = modID[k].size();
		}	

		QString str;
		for( size_t i = 0; i < modID[k].size(); i++)
		{
			str += QString::number(modID[k][i])+",";
		}
		listbox->addItem(str);
	}
	std::cout<< maxIndex << "\t"<< secondMax<<std::endl;
	if( maxIndex != secondMax)
	{
		QString mergedItem = listbox->item(maxIndex)->text() + listbox->item(secondMax)->text();
		listbox->addItem(mergedItem);
	}

	std::cout<< "Current Index: "<< maxIndex<<std::endl;
	listbox->setCurrentRow(maxIndex);
	autoTrendButton->setEnabled(true);
}

void SPDkNNGModuleMatch::showNSMForManualSelection()
{
	clusclus *clus1 = new clusclus();
	clusclus *clus2 = new clusclus();

	vnl_matrix<double> iterMat;
	this->SPDModel->EMDMatrixIteration(iterMat);

	vnl_vector<double> diagnalVec;
	this->SPDModel->GetBiClusData(iterMat, clus1, &diagnalVec);
	optimalleaforder.set_size(clus1->num_samples);
	clus2->optimalleaforder = new int[clus1->num_samples];
	clus2->num_samples = clus1->num_samples;
	for( int i = 0; i < clus1->num_samples; i++)
	{
		optimalleaforder[i] = clus1->optimalleaforder[i];
		clus2->optimalleaforder[i] = clus1->optimalleaforder[i];
	}

	//vtkSmartPointer<vtkTable> tableAfterCellCluster = SPDModel->GetDataTableAfterCellCluster();
	this->simHeatmap->setModels();
	this->simHeatmap->setDataForSimilarMatrixHeatmap(clus1->features, clus1->optimalleaforder, clus2->optimalleaforder, clus1->num_samples, clus2->num_samples);	
	this->simHeatmap->creatDataForSimilarMatrixHeatmap(diagnalVec.data_block());
	this->simHeatmap->showSimilarMatrixGraph();

	delete clus1;
	delete clus2;
}

void SPDkNNGModuleMatch::autoClick()
{
	std::cout<< "View auto trend."<<std::endl;
	viewTrendAuto(true);
}

void SPDkNNGModuleMatch::manualClick()
{
	std::cout<< "View manual trend."<<std::endl;
	viewTrendAuto(false);
}

void SPDkNNGModuleMatch::viewTrendAuto(bool bAuto)
{
	UpdateConnectedNum();  // update connected component
	std::cout<< connectedNum<<std::endl;

	if(connectedNum <= 1e-9)
	{
		return;
	}

	//if( this->originalHeatmap)
	//{
	//	delete this->originalHeatmap;
	//	this->originalHeatmap = NULL;
	//}

	std::string selectModulesID = "";

	if( bAuto)
	{
		selectModulesID = listbox->currentItem()->text().toStdString();
	}
	else
	{
		selectModulesID = psdModuleSelectBox->text().toStdString();
	}

	std::vector< unsigned int> selModuleID;
	std::vector< int> clusterSize;
	selFeatureID.clear();
	selOrder.clear();
	unselOrder.clear();

	split( selectModulesID, ',', selModuleID);
	std::cout<< "Selected Module Size:" << selModuleID.size() <<std::endl;
	SPDModel->GetFeatureIdbyModId(selModuleID, selFeatureID);
	std::cout<< "Selected Features:" << selFeatureID.size() <<std::endl;

	//for( size_t i = 0; i < selModuleID.size(); i++)
	//{
	//	std::cout<< selModuleID[i]<< "\t";
	//}
	//std::cout<<std::endl;

	// For validation:
	vnl_vector<int> validationVec;
	SPDModel->GetValidationVec(validationVec);
	if( validationVec.max_value() > 0)
	{
		vnl_matrix<double> clusAverageMat;
		SPDModel->GetDataMatrix(clusAverageMat);
		std::vector<int> clusterNum(1);
		clusterNum[0] = clusAverageMat.rows();
		vtkSmartPointer<vtkTable> treeTable = SPDModel->GenerateMST( clusAverageMat, selFeatureID, clusterNum);

		std::vector<std::string> headers;
		SPDModel->GetTableHeaders( headers);

		vnl_matrix<double> betweenDis;
		vnl_vector<double> accVec;
		vnl_vector<double> aggDegree;

		GraphWindow::GetTreeNodeBetweenDistance(treeTable, headers[0], headers[1], headers[2], betweenDis);
		double aggDegreeValue = 0;
		double averConnectionAccuracy = SPDModel->GetConnectionAccuracy(treeTable, betweenDis, accVec, aggDegree, aggDegreeValue, 1, 0);
		std::cout<< "ConnectionAccuracy: "<< averConnectionAccuracy<<std::endl;
		std::cout<< accVec<<std::endl;
		std::cout<< "ClusteringAccuracy: "<< aggDegreeValue<<std::endl;
		std::cout<< aggDegree<<std::endl;
	}
	// end valdiation

	GetFeatureOrder( selFeatureID, selOrder, unselOrder);

	// write graph to gdf file.
	//SPDModel->WriteGraphToGDF(selFeatureID);

	if(bAuto)
	{
		SPDModel->SaveSelectedFeatureNames("AutoSelFeatures.txt", selOrder);
		SPDModel->SaveNormalizedTableAfterFeatureSelection("AutoSelNormalizedFeatureTable", selOrder);
	}
	else
	{
		SPDModel->SaveSelectedFeatureNames("ManualSelFeatures.txt", selOrder);
		SPDModel->SaveNormalizedTableAfterFeatureSelection("ManualSelNormalizedFeatureTable", selOrder);
	}
	//SPDModel->WriteKNNGConnectionMatrix( "kNNGC.txt", selFeatureID);

	vtkSmartPointer<vtkTable> tableAfterCellCluster = SPDModel->GetDataTableAfterCellCluster();

	connect(selection, SIGNAL( thresChanged()), this, SLOT( regenerateTrendTree()));
	connect(selection, SIGNAL( ItemDeleted()), this, SLOT( ReRunSPDAnlysis()));
	//connect(HeatmapWin, SIGNAL(columnToColorChanged(int)), this, SLOT( ReColorTrendTree(int)));

	std::map< int, int> indexMap;
	SPDModel->GetClusterMapping(indexMap);

	vnl_matrix<double> subTreeDistance;
	SPDModel->GetComponentMinDistance(selFeatureID, connectedComponent, connectedNum, subTreeDistance);

	if( bAuto)
	{
		if( this->autoHeatmapWin)
		{
			delete this->autoHeatmapWin;
		}
		this->autoHeatmapWin = new TrendHeatmapWindow(tr("Auto Selection Heatmap"), this);
		this->autoHeatmapWin->setModelsforSPD( tableAfterCellCluster, selection, selOrder, unselOrder, &indexMap,\
										&connectedComponent, connectedNum, &subTreeDistance);
		this->autoHeatmapWin->showGraphforSPD( selOrder.size(), unselOrder.size());
	}
	else
	{
		if( this->manualHeatmapWin)
		{
			delete this->manualHeatmapWin;
		}
		this->manualHeatmapWin = new TrendHeatmapWindow(tr("Manual Selection Heatmap"), this);
		this->manualHeatmapWin->setModelsforSPD( tableAfterCellCluster, selection, selOrder, unselOrder, &indexMap,\
										&connectedComponent, connectedNum, &subTreeDistance);
		this->manualHeatmapWin->showGraphforSPD( selOrder.size(), unselOrder.size());
	}
}

void SPDkNNGModuleMatch::split(std::string& s, char delim, std::vector< unsigned int>& indexVec)
{
	size_t last = 0;
	size_t index = s.find_first_of(delim,last);
	std::vector< std::string > stringVec;
	while( index!=std::string::npos)
	{
		stringVec.push_back(s.substr(last,index-last));
		last = index+1;
		index=s.find_first_of(delim,last);
	}
	if( index-last>0)
	{
		stringVec.push_back(s.substr(last,index-last));
	}

	for( int i = 0; i < stringVec.size(); i++)
	{
		if( stringVec[i].length() > 0)
		{
			unsigned int index = atoi( stringVec[i].c_str());
			indexVec.push_back( index);
		}
	}
} 

void SPDkNNGModuleMatch::GetFeatureOrder(std::vector< unsigned int> &selID, std::vector<int> &selIdOrder, std::vector<int> &unselIdOrder)
{
	SPDModel->HierachicalClustering();
	std::vector< Tree> FeatureTreeData = SPDModel->PublicTreeData;
	double **ftreedata = new double*[FeatureTreeData.size()];

	for(int i = 0; i < FeatureTreeData.size(); i++)
	{
		ftreedata[i] = new double[4];
		ftreedata[i][0] = FeatureTreeData[i].first;
		ftreedata[i][1] = FeatureTreeData[i].second;
		ftreedata[i][2] = (1 - FeatureTreeData[i].cor + 0.01) * 100;
		ftreedata[i][3] = FeatureTreeData[i].parent;
	}

	clusclus *cc2 = new clusclus();
	cc2->Initialize(ftreedata, FeatureTreeData.size() + 1);
	cc2->GetOptimalLeafOrderD();

	for( int i = 0; i < cc2->num_samples; i++)
	{
		if( IsExist(selID, (unsigned int)cc2->optimalleaforder[i]))
		{
			selIdOrder.push_back( cc2->optimalleaforder[i]);
		}
		else
		{
			unselIdOrder.push_back( cc2->optimalleaforder[i]);
		}
	}

	//ofstream ofs("FeatureOrder.txt");
	//ofs<< "feature optimal order:"<<endl;
	//for( int i = 0; i < cc2->num_samples; i++)
	//{
	//	ofs<< cc2->optimalleaforder[i]<<"\t";
	//}
	//ofs<<endl;
	//ofs<< "Selected features optimal order:"<<endl;
	//for( int i = 0; i < selIdOrder.size(); i++)
	//{
	//	ofs<< selIdOrder[i]<<"\t";
	//}
	//ofs<<endl;
	//ofs<< "UnSelected features optimal order:"<<endl;
	//for( int i = 0; i < unselIdOrder.size(); i++)
	//{
	//	ofs<< unselIdOrder[i]<<"\t";
	//}
	//ofs<<endl;
	//ofs.close();

	for( int i = 0; i < FeatureTreeData.size(); i++)
	{
		delete ftreedata[i];
	}
	delete ftreedata;
	delete cc2;
}

bool SPDkNNGModuleMatch::IsExist(std::vector< unsigned int> vec, unsigned int value)
{
	for( int i = 0; i < vec.size(); i++)
	{
		if( value == vec[i])
		{
			return true;
		}
	}
	return false;
}

void SPDkNNGModuleMatch::regenerateTrendTree()
{
	heatmapButton->setEnabled(true);
	if( selection && (this->autoHeatmapWin || this->manualHeatmapWin))
	{

		std::cout<< "rerender progression view"<<endl;
		selection->clear();
		std::vector< std::vector< long int> > sampleIndex;
		selection->GetSampleIndex( sampleIndex);
        //std::vector< std::vector< long int> > clusIndex;
        //selection->GetClusterIndex( clusIndex);

		double homogeneity = SPDModel->GetANOVA(sampleIndex, selFeatureID);
		std::cout<< "Homogeneity evaluation: "<< homogeneity<<std::endl;

		vnl_matrix<double> clusAverageMat;
		std::vector< double> colorVec;
		std::vector< double> percentVec;
        SPDModel->GetSingleLinkageClusterAverage(sampleIndex, clusAverageMat);

		std::vector<int> clusterNum(1);
		clusterNum[0] = clusAverageMat.rows();

		vtkSmartPointer<vtkTable> newtable = SPDModel->GenerateMST( clusAverageMat, selFeatureID, clusterNum);
        //vtkSmartPointer<vtkTable> newtable = SPDModel->GenerateSubGraph( clusAverageMat, clusIndex, selFeatureID, clusterNum);

		/** graph window set models */
		std::vector<int> index;
		SPDModel->GetSingleLinkageClusterMapping(sampleIndex, index);
		vtkSmartPointer<vtkTable> dataTable = vtkSmartPointer<vtkTable>::New();
		SPDModel->GetCombinedDataTable(dataTable);
		this->graph->setModels(dataTable, selection, &index);

		std::vector<std::string> headers;
		SPDModel->GetTableHeaders( headers);
		this->graph->SetTreeTable( newtable, headers[0], headers[1], headers[2]);
		//this->graph->SetGraphTableToPassThrough( newtable, sampleIndex.size(), headers[0], headers[1], headers[2], &colorVec, &percentVec);
		try
		{
			this->graph->ShowGraphWindow();
		}
		catch(...)
		{
			std::cout<< "Graph window error!"<<endl;
		}
	}
}

void SPDkNNGModuleMatch::ReColorTrendTree(int nfeature)
{
	if( this->graph)
	{
		std::vector< std::vector< long int> > clusIndex;
		vnl_vector<double> featureValue;
		std::string featureName;
		selection->GetClusterIndex( clusIndex);
		SPDModel->GetClusterFeatureValue(clusIndex, nfeature, featureValue, featureName);
		this->graph->ColorTreeAccordingToFeatures(featureValue, featureName.c_str());
	}
}

void SPDkNNGModuleMatch::updateSelMod()   
{
	int r1 = 0;
	int r2 = 0;
	int c1 = 0;
	int c2 = 0;
	int size = optimalleaforder.size();

	this->simHeatmap->GetSelRowCol(r1, c1, r2, c2);
	//this->simHeatmap->SetSelRowCol(r1, size - 1 - r1, r2, size - 1 - r2);   // make the selection block symetric

	int num = abs(r1 - r2) + 1;
	int max = r1 > r2 ? r1 : r2;

	if( num <= size)   
	{
		if(!continSelectCheck->isChecked())
		{
			selMod.clear();
		}
		for( int i = 0; i < num; i++)
		{
			int index = size - 1 - max + i;
			if( index >= 0 && index < size)
			{
				selMod.insert(optimalleaforder[index]);
			}
		}
		QString str;
		std::set<unsigned int>::iterator iter = selMod.begin();
		for(; iter != selMod.end(); iter++)
		{
			str += QString::number(*iter)+",";
		}
		psdModuleSelectBox->setText(str);
		manualTrendButton->setEnabled(true);
	}
}

void SPDkNNGModuleMatch::UpdateConnectedNum()
{
	std::string selectModulesID = this->listbox->currentItem()->text().toStdString();
	std::vector< unsigned int> selModuleID;
	split( selectModulesID, ',', selModuleID);
	SPDModel->GetFeatureIdbyModId(selModuleID, selFeatureID);

	connectedNum = this->SPDModel->GetConnectedComponent(selFeatureID, connectedComponent);
}

void SPDkNNGModuleMatch::GetTrendTreeOrder(std::vector<long int> &order)
{
	this->graph->GetTrendTreeOrder(order);
}

void SPDkNNGModuleMatch::showTrendHeatmap()
{
	//ofstream ofs("SPDHeatmapOptimalOrder.txt");
	if( this->progressionHeatmap)
	{
		delete this->progressionHeatmap;
	}
	this->progressionHeatmap = new TrendHeatmapWindow(tr("MST-ordered Heatmap"),this);
	
	std::vector<long int> TreeOrder;
	this->graph->GetTrendTreeOrder(TreeOrder);   // order of the cluster 
	if( TreeOrder.size() <=0)
	{          
		std::cout<< "progression tree hasn't been built yet"<<endl;
		return;
	}

	std::vector< std::vector< long int> > sampleIndex;
	selection->GetSampleIndex( sampleIndex);
	std::vector< std::vector< long int> > clusIndex;
	selection->GetClusterIndex( clusIndex);
	std::vector< int> clusterOrder;
	SPDModel->GetClusterOrder(clusIndex, TreeOrder, clusterOrder);
	//SPDModel->GetValidationOrder(clusterOrder);

	vtkSmartPointer<vtkTable> tableForAverModulePlot = SPDModel->GetAverModuleTable(clusIndex, TreeOrder, selOrder, unselOrder);
	if( plot)
	{
		delete plot;
	}
	plot = new PlotWindow(this);
	plot->setModels(tableForAverModulePlot, selection);
	plot->show();

	// progression heatmap
	vtkSmartPointer<vtkTable> tableAfterCellCluster = SPDModel->GetDataTableAfterCellCluster();

	std::map< int, int> indexMap;
	SPDModel->GetClusterMapping(indexMap);
	this->progressionHeatmap->setModelsforSPD( tableAfterCellCluster, selection, clusterOrder, selOrder, unselOrder, &indexMap);
	this->progressionHeatmap->showGraphforSPD( selOrder.size(), unselOrder.size(), true);
}

void SPDkNNGModuleMatch::closeEvent(QCloseEvent *event)
{
	closeSubWindows();
	event->accept();
}

void SPDkNNGModuleMatch::closeSubWindows()
{
	if(originalHeatmap)
	{
		originalHeatmap->close();
		delete originalHeatmap;
		originalHeatmap = NULL;
	}
	if(graph)
	{
		graph->close();
		delete graph;
		graph = NULL;
	}
	if(simHeatmap)
	{
		simHeatmap->close();
		delete simHeatmap;
		simHeatmap = NULL;
	}
	if(progressionHeatmap)
	{
		progressionHeatmap->close();
		delete progressionHeatmap;
		progressionHeatmap = NULL;
	}
	if(autoHeatmapWin)
	{
		autoHeatmapWin->close();
		delete autoHeatmapWin;
		autoHeatmapWin = NULL;
	}
	if(manualHeatmapWin)
	{
		manualHeatmapWin->close();
		delete manualHeatmapWin;
		manualHeatmapWin = NULL;
	}
	if(plot)
	{
		plot->close();
		delete plot;
		plot = NULL;
	}
	if(histo)
	{
		histo->close();
		delete histo;
		histo = NULL;
	}
	if(SPDModel)
	{
		delete SPDModel;
		SPDModel = NULL;
	}
}

void SPDkNNGModuleMatch::ReRunSPDAnlysis()
{
	closeSubWindows();
	std::set<long int> selItems = this->selection->getSelections();
	disconnect(selection, SIGNAL( thresChanged()), this, SLOT( regenerateTrendTree()));
	disconnect(selection, SIGNAL( ItemDeleted()), this, SLOT( ReRunSPDAnlysis()));
	this->selection->clear();
	vtkSmartPointer<vtkTable> table = GetSubTableExcludeItems(data, selItems);
	setModels( table, this->selection);
}

vtkSmartPointer<vtkTable> SPDkNNGModuleMatch::GetSubTableExcludeItems(vtkSmartPointer<vtkTable> table, std::set<long int> &IDs)
{
	excludedIds = IDs;
	vtkSmartPointer<vtkTable> newTable = vtkSmartPointer<vtkTable>::New();
	for( vtkIdType i = 0; i < table->GetNumberOfColumns(); i++)
	{
		vtkSmartPointer<vtkVariantArray> column = vtkSmartPointer<vtkVariantArray>::New();
		column->SetName( table->GetColumnName(i));
		newTable->AddColumn(column);
    }

	for( vtkIdType i = 0; i < table->GetNumberOfRows(); i++)
	{
		long int id = table->GetValue(i, 0).ToLong();
		if( IDs.find(id) == IDs.end())
		{
			newTable->InsertNextRow( table->GetRow(i));
		}
	}
	return newTable;
}

vtkSmartPointer<vtkTable> SPDkNNGModuleMatch::NormalizeTable(vtkSmartPointer<vtkTable> table)
{
	SPDModel->ParseTraceFile( table, false);
	vtkSmartPointer<vtkTable> normalTable = SPDModel->GetDataTableAfterCellCluster();
	return normalTable;
}

void SPDkNNGModuleMatch::TestTrend()
{
	QString str = featureNum->text() + "_" + sampleNum->text() + "_" + clusterCoherenceBox->text() + "_" + kNearestNeighborBox->text();
	std::string logName = "TestLog_"+ str.toStdString() + ".txt";
	std::string accName = "Accuracy_" + str.toStdString() + ".txt";

	std::ofstream ofs(logName.c_str(), std::ofstream::out);
	std::ofstream acc(accName.c_str(), std::ofstream::out);
	acc<< "Threshold"<< "\t"<< /*"Index"<< "\t"<<*/"Module_size"<<"\t"<<"Coonection_Accuracy"<<"\t"<<"Aggregation_Degree"<<std::endl;
	//srand((unsigned)time(0));
	for(double selThreshold = 0.9; selThreshold >= 0.3; selThreshold = selThreshold - 0.01)
	{
		std::cout<< selThreshold<<"\t";
		std::vector<std::vector<unsigned int> > modID;
		SPDModel->GetSelectedFeaturesModulesByConnectedComponent(selThreshold, modID);
		
		//for(int count = 0; count < 22; count++)
		//{
		//	int randomN = rand() % 1291;
		//	std::cout<< randomN<<"\t";
		//	tmp.push_back(randomN);
		//}
		//std::cout<<std::endl;
		//modID.push_back(tmp);

		//std::vector<unsigned int> allMods;
		//for( size_t k = 0; k < modID.size(); k++)
		//{
		//	for( size_t i = 0; i < modID[k].size(); i++)
		//	{
		//		allMods.push_back(modID[k][i]);
		//	}
		//}
		//modID.clear();
		//modID.push_back(allMods);
	
		if( modID.size() > 0)
		{
			size_t maxSize = 0;
			size_t maxIndex = 0;
			for( size_t k = 0; k < modID.size(); k++)
			{
				if(modID[k].size() > maxSize)
				{
					maxSize = modID[k].size();
					maxIndex = k;
				}
			}
			std::vector<unsigned int > tmp = modID[maxIndex];

			ofs<< selThreshold<< /*"\t"<< k<< "\t"<< modID[k].size() <<*/std::endl;
			for(unsigned int i = 0; i < tmp.size(); i++)
			{
				ofs<< tmp[i]<<"\t";
			}
			ofs<<std::endl;

			std::vector< unsigned int> selFeatureID;
			SPDModel->GetFeatureIdbyModId(tmp, selFeatureID);

			vnl_matrix<double> clusAverageMat;
			SPDModel->GetDataMatrix(clusAverageMat);
			std::vector<int> clusterNum(1);
			clusterNum[0] = clusAverageMat.rows();

			vtkSmartPointer<vtkTable> treeTable = SPDModel->GenerateMST( clusAverageMat, selFeatureID, clusterNum);

			std::vector<std::string> headers;
			SPDModel->GetTableHeaders( headers);

			vnl_matrix<double> betweenDis;
			vnl_vector<double> accVec;
			vnl_vector<double> aggDegree;

			GraphWindow::GetTreeNodeBetweenDistance(treeTable, headers[0], headers[1], headers[2], betweenDis);
			double aggDegreeValue = 0;
			double averConnectionAccuracy = SPDModel->GetConnectionAccuracy(treeTable, betweenDis, accVec, aggDegree, aggDegreeValue, 1, 0);
			std::cout<< averConnectionAccuracy<< "\t"<< aggDegreeValue<<std::endl;

			ofs<< averConnectionAccuracy<<std::endl;
			ofs<< accVec<<std::endl;
			ofs<< aggDegree<<std::endl<<std::endl;
			acc<< selThreshold<< "\t"<< /*k<< "\t"<<*/tmp.size()<<"\t"<<averConnectionAccuracy<<"\t"<<aggDegreeValue<<std::endl;
		}
		else
		{
			acc<< selThreshold<< "\t"<< /*k<< "\t"<<*/0<<"\t"<<0<<"\t"<<0<<std::endl;
		}
	}
	ofs.close();
	acc.close();
}