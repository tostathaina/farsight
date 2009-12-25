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
#ifndef _ASC_FEAT_AUX_FN_CPP_
#define _ASC_FEAT_AUX_FN_CPP_

#include <math.h>

#include "itkImage.h"
#include "itkScalarImageToHistogramGenerator.h"
#include "itkBinaryThresholdImageFilter.h"
#include "itkNumericTraits.h"
#include "itkOtsuMultipleThresholdsCalculator.h"
#include "itkLabelStatisticsImageFilter.h"

#include "ftkCommon/itkLabelGeometryImageFilter.h"

#define MM_PI		3.14159265358979323846
#define MM_PI_2		1.57079632679489661923

typedef unsigned short USPixelType;
typedef itk::Image< USPixelType, 3 > UShortImageType;

std::vector<float> compute_ec_features( UShortImageType::Pointer input_image,  UShortImageType::Pointer input_labeled, int number_of_rois ){

	std::vector<float> quantified_numbers;

	typedef itk::LabelGeometryImageFilter< UShortImageType > GeometryFilterType;
	typedef itk::LabelStatisticsImageFilter< UShortImageType,UShortImageType > StatisticsFilterType;
	typedef itk::CastImageFilter< UShortImageType, UShortImageType > CastUSUSType;
	typedef itk::ImageRegionIteratorWithIndex< UShortImageType > IteratorType;
	typedef GeometryFilterType::LabelIndicesType labelindicestype;

	//int size1 = input_image->GetLargestPossibleRegion().GetSize()[0];
	//int size2 = input_image->GetLargestPossibleRegion().GetSize()[1];

	GeometryFilterType::Pointer geomfilt1 = GeometryFilterType::New();

	StatisticsFilterType::Pointer statsfilt = StatisticsFilterType::New();
	statsfilt->UseHistogramsOn();

	geomfilt1->SetInput( input_labeled );

	statsfilt->SetInput( input_image );
	statsfilt->SetLabelInput( input_labeled );
	statsfilt->Update();

	CastUSUSType::Pointer castUSUSfilter = CastUSUSType::New();

	for( unsigned short i=0; (int)i <= statsfilt->GetNumberOfLabels(); ++i ){
		std::vector<float> quantified_numbers_cell;
		//assert(quantified_numbers_cell.size() == number_of_rois);
		for( int j=0; j<number_of_rois; ++j ) quantified_numbers_cell.push_back(0);
		if( statsfilt->HasLabel(i) ){
			double centroid_x = (double)(geomfilt1->GetCentroid(i)[0]);
			double centroid_y = (double)(geomfilt1->GetCentroid(i)[1]);
			if( !statsfilt->GetSum(i) ){
				IteratorType iterator ( input_labeled, input_labeled->GetRequestedRegion() );
				labelindicestype indices1;
				indices1 = geomfilt1->GetPixelIndices(i);
				labelindicestype::iterator itPixind = indices1.begin();
				for( int j=0; j<(int)indices1.size(); ++j, ++itPixind ){
					iterator.SetIndex( *itPixind );
					double x = (double)(iterator.GetIndex()[0]);
					double y = (double)(iterator.GetIndex()[1]);
					double angle = atan2((centroid_y-y),fabs(centroid_x-x));
					if( (centroid_x-x)>0 )
						angle += MM_PI_2;
					else
						angle = MM_PI+MM_PI-(angle+MM_PI_2);
					angle = ((number_of_rois-1)*angle)/(2*MM_PI);
					double angle_fraction[1];
					if( modf( angle, angle_fraction ) > 0.5 )
						angle = ceil( angle );
					else
						angle = floor( angle );
					IteratorType iterator1 ( input_image, input_image->GetRequestedRegion() );
					iterator1.SetIndex( *itPixind );
					quantified_numbers_cell[(int)angle] += iterator1.Get();
				}
			}
		}
		//POP from each quantified_numbers_cell and PUSH to quantified_numbers 
		for( int j=0; j<number_of_rois; j++ ) quantified_numbers.push_back(quantified_numbers_cell[j]);
	}
	return quantified_numbers;
}

unsigned short returnthresh( UShortImageType::Pointer input_image, int num_bin_levs, int num_in_fg ){
//Instantiate the different image and filter types that will be used
	typedef itk::ImageRegionIteratorWithIndex< UShortImageType > IteratorType;
	typedef itk::ImageRegionConstIterator< UShortImageType > ConstIteratorType;
	typedef itk::Statistics::ScalarImageToHistogramGenerator< UShortImageType > ScalarImageToHistogramGeneratorType;
	typedef ScalarImageToHistogramGeneratorType::HistogramType HistogramType;
	typedef itk::OtsuMultipleThresholdsCalculator< HistogramType > CalculatorType;

	ScalarImageToHistogramGeneratorType::Pointer scalarImageToHistogramGenerator = ScalarImageToHistogramGeneratorType::New();
	CalculatorType::Pointer calculator = CalculatorType::New();
	scalarImageToHistogramGenerator->SetNumberOfBins( 128 );
	calculator->SetNumberOfThresholds( num_bin_levs );
	scalarImageToHistogramGenerator->SetInput( input_image);
	scalarImageToHistogramGenerator->Compute();
	calculator->SetInputHistogram( scalarImageToHistogramGenerator->GetOutput() );
	calculator->Update();
	const CalculatorType::OutputType &thresholdVector = calculator->GetOutput(); 
	CalculatorType::OutputType::const_iterator itNum = thresholdVector.begin();

	USPixelType thresh;

	for(int i=0; i < num_in_fg; ++itNum, ++i)
		thresh = (static_cast<USPixelType>(*itNum));

	return thresh;

}

#endif