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

#include "SegmentationModel.h"
#include <QtGui/QMessageBox>
#include <algorithm>

SegmentationModel::SegmentationModel(ftk::NuclearSegmentation *segresult)
{
	segResult = segresult;

	model = new QStandardItemModel(0,0);
	selectionModel = new QItemSelectionModel(model);

	columnForID = -1;
	numFeatures = 0;
	numObjects = 0;
	columnForClass = -1;
	columnForOutliers = -1;
	columnForColor = -1;

	SyncModel();
}

SegmentationModel::~SegmentationModel()
{
	delete selectionModel;
	delete model;
	selectionModel = NULL;
	model = NULL;
}

void SegmentationModel::SyncModel()
{
	model->setColumnCount(0);
	model->setRowCount(0);

	std::vector<ftk::Object> *objects = segResult->GetObjectsPtr();
	std::vector<string> featureNames = segResult->GetFeatureNames();

	columnForID = 0;
	numObjects = (int)objects->size();
	numFeatures = (int)featureNames.size();
	columnForClass = numFeatures+1;

	model->setColumnCount(numFeatures+2);		//1 column for class, 1 for ID

	//Setup the Header info
	model->setHeaderData( columnForID, Qt::Horizontal, tr("id") );
	for (int r=0; r<numFeatures; ++r)
	{
		model->setHeaderData(r+1, Qt::Horizontal, QString::fromStdString(featureNames[r]));
	}
	model->setHeaderData( numFeatures+1, Qt::Horizontal, tr("class") );

	int currentRow = 0;
	//Fill in the data
	for (int obj=0; obj<numObjects; ++obj)
	{
		if (objects->at(obj).GetValidity() == false)
			continue;

		model->insertRow(currentRow);

		int id = objects->at(obj).GetId();
		model->setData(model->index(currentRow, columnForID, QModelIndex()), id);

		vector<float> features = objects->at(obj).GetFeatures();
		for(int f=0; f<numFeatures; ++f)
		{
			model->setData(model->index(currentRow, f+1, QModelIndex()), features[f]);
		}

		int clss = objects->at(obj).GetClass();
		model->setData(model->index(currentRow,columnForClass,QModelIndex()),clss);

		currentRow++;
	}
	updateMapping();
	columnForColor = columnForClass;
	UpdateColors();
}

void SegmentationModel::ShowOutliers(bool show)
{
	if( columnForColor != columnForOutliers && show == true )
	{
		columnForColor = columnForOutliers;
		UpdateColors();
		emit modelChanged();
	}
	else if( columnForColor != columnForClass && show == false )
	{
		columnForColor = columnForClass;
		UpdateColors();
		emit modelChanged();
	}
}

void SegmentationModel::SetOutliers( vector<int> outs )
{
	if(columnForOutliers == -1)
	{
		columnForOutliers = model->columnCount();
		model->insertColumn(columnForOutliers);			//Add Column for outliers		
		model->setHeaderData( columnForOutliers, Qt::Horizontal, tr("outlier?") );
	}

	int z = 0;
	int o = 1;
	for(int row = 0; row < model->rowCount(); ++row)  //Set all values to 0
	{
		model->setData(model->index(row, columnForOutliers), z);
		//QStandardItem *item = new QStandardItem( QString("%0").arg(0) );
        //model->setItem(row, columnForOutliers, item);         
	}

	for(unsigned int i = 0; i < outs.size(); ++i)				  //Set outliers to 1
	{
		int row = RowForID( outs.at(i) );
		model->setData(model->index(row, columnForOutliers), o);
		//QStandardItem *item = model->item(row,columnForOutliers);
		//item->setText( QString("%0").arg(1) );
	}

	emit modelChanged();
}

bool SegmentationModel::HasOutliers(void)
{
	if(columnForOutliers == -1)
		return false;
	else
		return true;
}

void SegmentationModel::UpdateColors(void)
{
	//Assumes that class info is in model
	//Check just-in-case
	if(columnForColor <= 0)
		return;

	//Check to be sure this row contains integers:
	QVariant val = model->data(model->index(0,columnForColor));
	string type = val.typeName();
	if(type != "int")
		return;
	
	//Now find possible classes(limit: 5 for now)
	vector<int> classId(0);
	for (int r=0; r<model->rowCount(); ++r)
	{
		int clss = model->data(model->index(r,columnForColor)).toInt();
		bool found = false;
		for(unsigned int c=0; c<classId.size(); ++c)
		{
			if(classId[c] == clss)
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			classId.push_back(clss);
			if(classId.size() >= 5)
				break;
		}
	}
	sort( classId.begin(), classId.end() );
	//Now assign colors to the classes
	QColor color[5]  = {Qt::cyan, Qt::magenta, Qt::red, Qt::blue, Qt::green};
	colorMap.clear();
	for(unsigned int c=0; c<classId.size(); ++c)
	{
		colorMap.insert(classId[c],color[c]);
	}
}

//*****************************************************************************************
// LabelToRowMap is a mapping from cell ID to its row in the dataModel
// I use the mapping to help speed up the connection between clicking on a label
// and finding the cell information in the model.
//*****************************************************************************************
//*****************************************************************************************
void SegmentationModel::updateMapping(void)
{
	if(!segResult)
		return;

	QModelIndex index;
	int ID;
	LabelToRowMap.clear();
	for (int row = 0; row < model->rowCount(); row++)
	{
		//Do ID mapping
		index = model->index(row, columnForID);
		ID = model->data(index).toInt();
		LabelToRowMap.insert(ID,row);
	}
}

int SegmentationModel::RowForID(int id)
{
	return LabelToRowMap.value(id);
}

//Call this slot when trying to delete objects
void SegmentationModel::deleteTrigger()
{
	//Extract a list of IDs
	QModelIndexList selIndices = selectionModel->selectedRows();
	vector<int> ids(0);
	QString idlist;
	for (int selIndex = 0; selIndex < selIndices.size(); ++selIndex) 
	{
		int row = selIndices.at(selIndex).row();
		int id = model->data( model->index(row, columnForID) ).toInt();
		ids.push_back(id);
		idlist.append(QString::number(id));
		idlist.append("  ");
	}

	QString msg = tr("Do you want to delete these ") + QString::number(selIndices.size()) + tr(" objects:\n");
	msg.append(idlist);
	msg.append("?");

	//Verify operation:
	QMessageBox::StandardButton button = QMessageBox::information ( 0, tr("Delete"), \
		msg, QMessageBox::Yes | QMessageBox::No , QMessageBox::NoButton );

	if(button != QMessageBox::Yes)
		return;

	ftk::NuclearSegmentation *nucseg = (ftk::NuclearSegmentation*)segResult;
	//Attempt Delete:
	if( nucseg->Delete(ids) )
	{
		//Remove from table
		for (unsigned int id=0; id<ids.size(); ++id)
		{
			int row = RowForID( ids.at(id) );
			QList<QStandardItem *> items = model->takeRow(row);
			for(int i=0; i<items.size(); ++i)
			{
				delete items.at(i);
			}
			updateMapping();
		}
	}
	emit modelChanged();

}
void SegmentationModel::splitTrigger()
{
	//do nothing for now
	int xx=1;
	QModelIndexList selIndices = selectionModel->selectedRows();
	vector< int > ids(0);
	QString idlist(0);

	for (int selIndex = 0; selIndex < selIndices.size(); ++selIndex) 
	{
		int row = selIndices.at(selIndex).row();
		int id = model->data( model->index(row, columnForID) ).toInt();
		idlist.append(QString::number(id));
		idlist.append(" ");
		ids.push_back(id);
	}

	if(ids.size() < 1)
		return;

	QString msg = tr("Do you want to split the objects with these IDs:\n");
	msg.append(idlist);
	msg.append("?");

	//Verify operation:
	QMessageBox::StandardButton button = QMessageBox::information ( 0, tr("Split"), \
		msg, QMessageBox::Yes | QMessageBox::No , QMessageBox::NoButton );

	if(button != QMessageBox::Yes)
		return;

	ftk::NuclearSegmentation *nucseg = (ftk::NuclearSegmentation*)segResult;
	
	//Attempt split:
	for(unsigned int group = 0; group < ids.size(); ++group)
	{
		std::vector< int > newIDs = nucseg->Split( ids.at(group) );
		//need to add code that updates the table
		//1-Remove the old cell
		//2-Add the two new cells

		//Remove from table the old id (remove the splitted object)			
		int row = RowForID(ids.at(group));
		QList<QStandardItem *> items = model->takeRow(row);
		for(int i=0; i<items.size(); ++i)
		{
			delete items.at(i);
		}
		updateMapping();

		if( newIDs.size()==2 )
		{
			// Add the cell-ids of 2 new (result of splitting) cells
			for (unsigned int i=0;i<2;i++) {
				//Add into table the new object
				int currentRow = model->rowCount();
				model->insertRow(currentRow);
				model->setData(model->index(currentRow, columnForID, QModelIndex()), newIDs[i]);
				vector<float> features = segResult->GetObjectPtr(newIDs[i])->GetFeatures();
				for(int f=0; f<(int)features.size(); ++f) {
					model->setData(model->index(currentRow, f+1, QModelIndex()), features[f]);
				}
	
				int clss = segResult->GetObjectPtr(newIDs[i])->GetClass();
				model->setData(model->index(currentRow,columnForClass,QModelIndex()),clss);
				updateMapping();
			}

	//		//Remove from table merged IDs
	//		for (unsigned int id=0; id < ids.at(group).size(); ++id)
	//		{
	//			int row = RowForID( ids.at(group).at(id) );
	//			QList<QStandardItem *> items = model->takeRow(row);
	//			for(int i=0; i<items.size(); ++i)
	//			{
	//				delete items.at(i);
	//			}
	//			updateMapping();
	//		}
	//		//Add into table the new object
	//		int currentRow = model->rowCount();
	//		model->insertRow(currentRow);
	//		model->setData(model->index(currentRow, columnForID, QModelIndex()), newID);
	//		vector<float> features = segResult->GetObjectPtr(newID)->GetFeatures();
	//		for(int f=0; f<(int)features.size(); ++f)
	//		{
	//			model->setData(model->index(currentRow, f+1, QModelIndex()), features[f]);
	//		}
	//		int clss = segResult->GetObjectPtr(newID)->GetClass();
	//		model->setData(model->index(currentRow,columnForClass,QModelIndex()),clss);
	//		updateMapping();
		}
	}

	//The next two lines need to be checked
	//UpdateColors();
	//emit modelChanged();
}
//Call this slot when trying to merge objects
void SegmentationModel::mergeTrigger()
{
	//Extract a list of IDs
	QModelIndexList selIndices = selectionModel->selectedRows();
	vector< vector<int> > ids(0);
	QString idlist(0);
	for (int selIndex = 0; selIndex < selIndices.size(); ++selIndex) 
	{
		int row = selIndices.at(selIndex).row();
		int id = model->data( model->index(row, columnForID) ).toInt();


		//Go through previous ids and look for neighbors, not 100% working, but works in most cases (may leave orphans that should be merged)
		bool found = false;
		for( unsigned int group = 0; group < ids.size(); group++)
		{
			for (unsigned int m = 0; m < ids.at(group).size(); m++)
			{
				ftk::Object *o1 = segResult->GetObjectPtr( ids.at(group).at(m) );
				ftk::Object *o2 = segResult->GetObjectPtr( id );
				bool neighb = neighbor( o1, o2 );
				if (neighb == true)
				{
					ids.at(group).push_back( id );
					found = true;
				}
				if(found) break;
			}
			if(found) break;
		}
	
		//Didn't find it so start a new list
		if(!found)
		{
			vector<int> nv(0);
			nv.push_back( id );
			ids.push_back(nv);
		}
		idlist.append(QString::number(id));
		idlist.append(" ");
	}

	if(ids.size() < 1)
		return;

	QString msg = tr("Do you want to merge the objects with these IDs:\n");
	msg.append(idlist);
	msg.append("?");

	//Verify operation:
	QMessageBox::StandardButton button = QMessageBox::information ( 0, tr("Merge"), \
		msg, QMessageBox::Yes | QMessageBox::No , QMessageBox::NoButton );

	if(button != QMessageBox::Yes)
		return;

	ftk::NuclearSegmentation *nucseg = (ftk::NuclearSegmentation*)segResult;
	//Attempt Merge:
	for(unsigned int group = 0; group < ids.size(); ++group)
	{
		if(ids.at(group).size() <= 1)
			return;

		int newID = nucseg->Merge( ids.at(group) );
		if( newID )
		{
			//Remove from table merged IDs
			for (unsigned int id=0; id < ids.at(group).size(); ++id)
			{
				int row = RowForID( ids.at(group).at(id) );
				QList<QStandardItem *> items = model->takeRow(row);
				for(int i=0; i<items.size(); ++i)
				{
					delete items.at(i);
				}
				updateMapping();
			}
			//Add into table the new object
			int currentRow = model->rowCount();
			model->insertRow(currentRow);
			model->setData(model->index(currentRow, columnForID, QModelIndex()), newID);
			vector<float> features = segResult->GetObjectPtr(newID)->GetFeatures();
			for(int f=0; f<(int)features.size(); ++f)
			{
				model->setData(model->index(currentRow, f+1, QModelIndex()), features[f]);
			}
			int clss = segResult->GetObjectPtr(newID)->GetClass();
			model->setData(model->index(currentRow,columnForClass,QModelIndex()),clss);
			updateMapping();
		}
	}
	UpdateColors();
	emit modelChanged();

}

//Crude neighbor test
bool SegmentationModel::neighbor(ftk::Object *obj1, ftk::Object *obj2)
{
	ftk::Object::Point c1 = obj1->GetCenters().at(0);
	ftk::Object::Point c2 = obj2->GetCenters().at(0);
	ftk::Object::Box b1 = obj1->GetBounds().at(0);
	ftk::Object::Box b2 = obj2->GetBounds().at(0);

	//check t
	if(c1.t != c2.t)
		return false;

	QRect r1(QPoint(b1.min.x,b1.min.y),QPoint(b1.max.x,b1.max.y));
	QRect r2(QPoint(b2.min.x,b2.min.y),QPoint(b2.max.x,b2.max.y));

	if( r1.intersects(r2) )
		return true;

	return false;
}
