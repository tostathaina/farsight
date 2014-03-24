#ifndef SPDKNNGMODULEMATCH_H
#define SPDKNNGMODULEMATCH_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include "SPDAnalysisModel.h"
#include "ftkGUI/GraphWindow.h"
#include "ftkGUI/HistoWindow.h"
#include "ftkGUI/PlotWindow.h"
#include "TrendHeatmapWindow.h"
#include "OrderedHeatmapWindow.h"
#include <QListWidget>

class SPDkNNGModuleMatch : public QWidget
{
    Q_OBJECT

public:
    SPDkNNGModuleMatch(QWidget *parent = 0);
	void setModels(vtkSmartPointer<vtkTable> table = NULL, ObjectSelection * sels = NULL, ObjectSelection * sels2 = NULL);
    ~SPDkNNGModuleMatch();
	void GetTrendTreeOrder(std::vector<long int> &order);
	vtkSmartPointer<vtkTable> NormalizeTable(vtkSmartPointer<vtkTable> table);

protected:
	void split(std::string& s, char delim, std::vector< unsigned int>& indexVec);
	void GetFeatureOrder(std::vector< unsigned int> &selID, std::vector< int> &selIdOrder, std::vector< int> &unselIdOrder);
	void showHeatmapAfterFeatureClustering();
	bool IsExist(std::vector< unsigned int> vec, unsigned int value);
	virtual void closeEvent(QCloseEvent *event);
	void closeSubWindows();
	vtkSmartPointer<vtkTable> GetSubTableExcludeItems( vtkSmartPointer<vtkTable> table, std::set<long int> &selItems);
	void viewTrendAuto(bool bAuto = true);

protected slots:
	void showOriginalHeatmap();
    void clusterFunction();
	void generateNSM();
	void autoSelection();
	void showNSMForManualSelection();
	void updateSelMod();
	void editNearestNeighbor();
	void showTrendHeatmap();
	void regenerateTrendTree();
	void ReRunSPDAnlysis();
	void ReColorTrendTree(int nfeature);
	void UpdateConnectedNum();
	void TestTrend();
	void autoClick();
	void manualClick();

private:
	SPDAnalysisModel *SPDModel;

	QPushButton *rawHeatmapButton;
	
    QLabel *featureNumLabel;
    QLabel *featureNum;
    QLabel *sampleNumLabel;
    QLabel *sampleNum;

    QLabel *clusterCoherenceLabel;
    QDoubleSpinBox *clusterCoherenceBox;
    QLabel *kNearestNeighborLabel;
    QSpinBox *kNearestNeighborBox;
	QLabel *nBinLabel;
    QSpinBox *nBinBox;
	QPushButton *nsmButton;

	QLabel *autoSelList;
    QPushButton *autoSelButton;
	QPushButton *testButton;
    QPushButton *autoTrendButton;
	QPushButton *heatmapButton;  // show progression heatmap  // now shows the progression over distance to device

	QLabel *manualSelectionLabel;   
	QLabel *continSelectLabel;   
	QCheckBox *continSelectCheck;
	QPushButton *manualSelButton;
	QLineEdit *psdModuleSelectBox;  // select similar modules 
	QPushButton *manualTrendButton;

	//QLabel *connectedGraphLabel;
	//QLineEdit *connectedGraphEdit;
	//QPushButton *updateConnectedNumButton;  // update connected component number
	QListWidget *listbox;

	QString FileName;
	GraphWindow *graph;
	OrderedHeatmap *simHeatmap;
	HistoWindow *histo;
	TrendHeatmapWindow *originalHeatmap;
	TrendHeatmapWindow *progressionHeatmap;
	TrendHeatmapWindow *autoHeatmapWin;
	TrendHeatmapWindow *manualHeatmapWin;
	PlotWindow *plot;

	vnl_vector<int> optimalleaforder;

	vtkSmartPointer<vtkTable> data;
	std::set<long int> excludedIds;
	ObjectSelection *selection;  /** selection for threshold and IDs */
	ObjectSelection *selection2; 

	std::vector< unsigned int> selFeatureID;
	std::vector< int> selOrder;
	std::vector< int> unselOrder;
	vtkSmartPointer<vtkTable> tableAfterCellCluster;
	std::map< int, int> indexMap;
	std::vector<int> connectedComponent;
	int connectedNum;
	bool bconnected;
	std::set<unsigned int> selMod;
};

#endif // SPDKNNGMODULEMATCH_H
