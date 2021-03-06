/** \file
 *  \ingroup commandLineTools
 */

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkResampleImageFilter.h"
#include "itkImageSeriesReader.h"
#include "itkGDCMImageIO.h"
#include "itkGDCMSeriesFileNames.h"
#include <fstream>
#include "itkImageRegionIteratorWithIndex.h"
#include "itkImageRegionIterator.h"
#include "itkRegionOfInterestImageFilter.h"
#include "itkCIPExtractChestLabelMapImageFilter.h"
#include "itkRegularStepGradientDescentOptimizer.h"
#include "itkImageRegistrationMethod.h"
#include "itkCenteredTransformInitializer.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkKappaStatisticImageToImageMetric.h"
#include "itkAffineTransform.h"
#include "itkTransformFileWriter.h"
#include "itkIdentityTransform.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xinclude.h>
#include <libxml/xmlIO.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include "RegisterLabelMaps3DCLP.h"
#include "cipChestConventions.h"
#include "cipHelper.h"
#include <sstream>

namespace
{
#define MY_ENCODING "ISO-8859-1"
  
  typedef itk::ResampleImageFilter< cip::LabelMapType, cip::LabelMapType >                          ResampleFilterType;
  typedef itk::ImageFileWriter< cip::LabelMapType >                                                 ImageWriterType;
  typedef itk::RegularStepGradientDescentOptimizer                                                  OptimizerType;
  typedef itk::ImageRegistrationMethod< cip::LabelMapType, cip::LabelMapType >                      RegistrationType;
  typedef itk::KappaStatisticImageToImageMetric< cip::LabelMapType, cip::LabelMapType >             MetricType;
  typedef itk::NearestNeighborInterpolateImageFunction< cip::LabelMapType, double >                 InterpolatorType;
  typedef OptimizerType::ScalesType                                                                 OptimizerScalesType;
  typedef itk::ImageRegionIteratorWithIndex< cip::LabelMapType >                                    IteratorType;
  typedef itk::RegionOfInterestImageFilter< cip::LabelMapType, cip::LabelMapType >                  RegionOfInterestType;
  typedef itk::ResampleImageFilter< cip::LabelMapType, cip::LabelMapType >                          ResampleType;
  typedef itk::IdentityTransform< double, 3 >                                                       IdentityType;
  typedef itk::CIPExtractChestLabelMapImageFilter< 3 >                                              LabelMapExtractorType;
  typedef itk::ImageSeriesReader< cip::CTType >                                                     CTSeriesReaderType;
  typedef itk::GDCMImageIO                                                                          ImageIOType;
  typedef itk::GDCMSeriesFileNames                                                                  NamesGeneratorType;
  typedef itk::ImageFileReader< cip::CTType >                                                       CTFileReaderType;
  typedef itk::ImageRegionIteratorWithIndex< cip::LabelMapType >                                    LabelMapIteratorType;
  typedef itk::AffineTransform<double, 3 >                                                          TransformType;
  typedef itk::CenteredTransformInitializer< TransformType, cip::LabelMapType, cip::LabelMapType >  InitializerType;
  typedef itk::ImageFileReader<cip::LabelMapType >                                                  LabelMapReaderType;

  struct REGIONTYPEPAIR
  {
    unsigned char region;
    unsigned char type;
  };

  struct REGISTRATION_XML_DATA
  {
    std::string registrationID;
    float similarityValue;
    std::string transformationLink;
    std::string sourceID;
    std::string destID;
    std::string similarityMeasure;
    std::string image_type;
    int transformationIndex;
  };

  void WriteTransformFile( TransformType::Pointer transform, char* fileName )
  {
    itk::TransformFileWriter::Pointer transformWriter = itk::TransformFileWriter::New();
      transformWriter->SetInput( transform );
      transformWriter->SetFileName( fileName );
    try
      {
      transformWriter->Update();
      }
    catch ( itk::ExceptionObject &excp )
      {
      std::cerr << "Exception caught while updating transform writer:";
      std::cerr << excp << std::endl;
      }
  }

  cip::LabelMapType::Pointer ReadLabelMapFromFile( std::string labelMapFileName )
  {
    std::cout << "Reading label map..." << std::endl;
    cip::LabelMapReaderType::Pointer reader = cip::LabelMapReaderType::New();
      reader->SetFileName( labelMapFileName );
    try
      {
      reader->Update();
      }
    catch ( itk::ExceptionObject &excp )
      {
      std::cerr << "Exception caught reading label map:";
      std::cerr << excp << std::endl;
      }

    return reader->GetOutput();
  }

  void WriteRegistrationXML(const char *file, REGISTRATION_XML_DATA &theXMLData)
  {
    std::cout<<"Writing registration XML file"<<std::endl;
    xmlDocPtr doc = NULL;       /* document pointer */
    xmlNodePtr root_node = NULL; /* Node pointers */
    xmlDtdPtr dtd = NULL;       /* DTD pointer */

    doc = xmlNewDoc(BAD_CAST "1.0");
    root_node = xmlNewNode(NULL, BAD_CAST "Registration");
    xmlDocSetRootElement(doc, root_node);

    dtd = xmlCreateIntSubset(doc, BAD_CAST "root", NULL, BAD_CAST "RegistrationOutput_v2.dtd");

    time_t timer;
    time( &timer );
    std::stringstream tempStream;
    tempStream << timer;
    theXMLData.registrationID.assign("Registration");
    theXMLData.registrationID.append(tempStream.str());
    theXMLData.registrationID.append("_");
    theXMLData.registrationID.append(theXMLData.sourceID.c_str());
    theXMLData.registrationID.append("_to_");
    theXMLData.registrationID.append(theXMLData.destID.c_str());

    xmlNewProp(root_node, BAD_CAST "Registration_ID", BAD_CAST (theXMLData.registrationID.c_str()));

    // xmlNewChild() creates a new node, which is "attached"
    // as child node of root_node node.
    std::ostringstream similaritString;
    //std::string tempsource;
    similaritString << theXMLData.similarityValue;
    std::ostringstream transformationIndexString;    
    transformationIndexString <<theXMLData.transformationIndex;
    
    xmlNewChild(root_node, NULL, BAD_CAST "image_type", BAD_CAST
                (theXMLData.image_type.c_str()));
    xmlNewChild(root_node, NULL, BAD_CAST "transformation", BAD_CAST
		(theXMLData.transformationLink.c_str()));
    xmlNewChild(root_node, NULL, BAD_CAST "transformation_index", BAD_CAST
                (transformationIndexString.str().c_str()));
    xmlNewChild(root_node, NULL, BAD_CAST "movingID", BAD_CAST
		(theXMLData.sourceID.c_str()));
    xmlNewChild(root_node, NULL, BAD_CAST "fixedID", BAD_CAST
		(theXMLData.destID.c_str()));
    xmlNewChild(root_node, NULL, BAD_CAST "SimilarityMeasure", BAD_CAST
		(theXMLData.similarityMeasure.c_str()));
    xmlNewChild(root_node, NULL, BAD_CAST "SimilarityValue", BAD_CAST
		(similaritString.str().c_str()));
    xmlSaveFormatFileEnc(file, doc, "UTF-8", 1);
    xmlFreeDoc(doc);
  }
} //end namespace

int main( int argc, char *argv[] )
{
  PARSE_ARGS;

  std::vector< unsigned char >  regionVec;
  std::vector< unsigned char >  typeVec;
  std::vector< unsigned char >  regionPairVec;
  std::vector< unsigned char >  typePairVec;
      
  //Read in region and type pair
  std::vector< REGIONTYPEPAIR > regionTypePairVec;

  for ( unsigned int i=0; i<regionVecArg.size(); i++ )
    {
      regionVec.push_back(regionVecArg[i]);
    }
  for ( unsigned int i=0; i<typeVecArg.size(); i++ )
    {
      typeVec.push_back( typeVecArg[i] );
    }
  if (regionPairVec.size() == typePairVecArg.size())
    {
      for ( unsigned int i=0; i<regionPairVecArg.size(); i++ )
	{
	  REGIONTYPEPAIR regionTypePairTemp;

	  regionTypePairTemp.region = regionPairVecArg[i];
	  argc--; argv++;
	  regionTypePairTemp.type   = typePairVecArg[i];

	  regionTypePairVec.push_back( regionTypePairTemp );
	}
    }

  //Read in fixed image label map from file and subsample
  cip::LabelMapType::Pointer fixedLabelMap = cip::LabelMapType::New();
  cip::LabelMapType::Pointer subSampledFixedImage = cip::LabelMapType::New();

  if ( strcmp( fixedImageFileName.c_str(), "q") != 0 )
    {
      std::cout << "Reading label map from file..." << std::endl;

      fixedLabelMap  = ReadLabelMapFromFile( fixedImageFileName.c_str() );
      if (fixedLabelMap.GetPointer() == NULL)
	{
	  return cip::LABELMAPREADFAILURE;
	}	
    }
  else
    {
      std::cerr <<"Error: No lung label map specified"<< std::endl;
      return cip::EXITFAILURE;
    }

  std::cout << "Subsampling fixed image by a factor of " << downsampleFactor << "..." << std::endl;
  subSampledFixedImage = cip::DownsampleLabelMap(downsampleFactor, fixedLabelMap);

  //Read in moving image label map from file and subsample
  cip::LabelMapType::Pointer movingLabelMap = cip::LabelMapType::New();
  cip::LabelMapType::Pointer subSampledMovingImage = cip::LabelMapType::New();
  if ( strcmp( movingImageFileName.c_str(), "q") != 0 )
    {
      std::cout << "Reading label map from file..." << std::endl;
      movingLabelMap = ReadLabelMapFromFile( movingImageFileName.c_str() );

      if (movingLabelMap.GetPointer() == NULL)
	{
	  return cip::LABELMAPREADFAILURE;
	}
    }
  else
    {
      std::cerr << "Error: No lung label map specified" << std::endl;
      return cip::EXITFAILURE;
    }

  std::cout << "Subsampling moving image by a factor of " << downsampleFactor << "..." << std::endl;
  subSampledMovingImage = cip::DownsampleLabelMap(downsampleFactor, movingLabelMap);

  if ( regionVec.size() > 0 || typeVec.size() > 0 || regionTypePairVec.size() > 0)
    {
      std::cout << "Extracting region and type from moving and fixed images..." << std::endl;
      LabelMapExtractorType::Pointer fixedExtractor = LabelMapExtractorType::New();
        fixedExtractor->SetInput( subSampledFixedImage );
      
      LabelMapExtractorType::Pointer movingExtractor = LabelMapExtractorType::New();
        movingExtractor->SetInput( subSampledMovingImage );
      
      for ( unsigned int i=0; i<regionVec.size(); i++ )
	{
	  fixedExtractor->SetChestRegion( regionVec[i] );
	  movingExtractor->SetChestRegion( regionVec[i] );
	}
      if ( typeVec.size() > 0 )
	{
	  for ( unsigned int i=0; i<typeVec.size(); i++ )
	{
	  fixedExtractor->SetChestType( typeVec[i] );
	  movingExtractor->SetChestType( typeVec[i] );
	}
	}
      if ( regionTypePairVec.size()>0 )
	{
	  for ( unsigned int i=0; i<regionTypePairVec.size(); i++ )
	    {
	      fixedExtractor->SetRegionAndType( regionTypePairVec[i].region,regionTypePairVec[i].type );
	      movingExtractor->SetRegionAndType( regionTypePairVec[i].region,regionTypePairVec[i].type );
	    }
	}

      fixedExtractor->Update();
      movingExtractor->Update();

      LabelMapIteratorType feIt( fixedExtractor->GetOutput(), fixedExtractor->GetOutput()->GetBufferedRegion() );
      LabelMapIteratorType sfIt( subSampledFixedImage, subSampledFixedImage->GetBufferedRegion() );

      feIt.GoToBegin();
      sfIt.GoToBegin();
      while ( !sfIt.IsAtEnd() )
	{
	  sfIt.Set( feIt.Get() );

	  ++feIt;
	  ++sfIt;
	}

      LabelMapIteratorType meIt( movingExtractor->GetOutput(), movingExtractor->GetOutput()->GetBufferedRegion() );
      LabelMapIteratorType smIt( subSampledMovingImage, subSampledMovingImage->GetBufferedRegion() );

      meIt.GoToBegin();
      smIt.GoToBegin();
      while ( !smIt.IsAtEnd() )
	{
	  smIt.Set( meIt.Get() );

	  ++meIt;
	  ++smIt;
	}
    }

  LabelMapIteratorType it( subSampledFixedImage, subSampledFixedImage->GetBufferedRegion() );

  it.GoToBegin();
  while ( !it.IsAtEnd() )
    {
      if ( it.Get() != 0 )
	{
	  it.Set( 1 );
	}
      ++it;
    }

  LabelMapIteratorType itmoving( subSampledMovingImage, subSampledMovingImage->GetBufferedRegion() );

  itmoving.GoToBegin();
  while ( !itmoving.IsAtEnd() )
    {
      if ( itmoving.Get() != 0 )
	{
	  itmoving.Set( 1 );
	}

      ++itmoving;
    }

  MetricType::Pointer metric = MetricType::New();
    metric->SetForegroundValue( 1 );  // Because we are minimizing as opposed to maximizing

  TransformType::Pointer transform = TransformType::New();
    
  std::cout << "Initializing transform..." << std::endl;
  InitializerType::Pointer initializer = InitializerType::New();
    initializer->SetTransform( transform );
    initializer->SetFixedImage( subSampledFixedImage );
    initializer->SetMovingImage( subSampledMovingImage );
    initializer->MomentsOn();
    initializer->InitializeTransform();
      
  OptimizerScalesType optimizerScales( transform->GetNumberOfParameters());
    optimizerScales[0]  =  1.0;   
    optimizerScales[1]  =  1.0;
    optimizerScales[2]  =  1.0;
    optimizerScales[3]  =  1.0;   
    optimizerScales[4]  =  1.0;
    optimizerScales[5]  =  1.0;
    optimizerScales[6]  =  1.0;   
    optimizerScales[7]  =  1.0;
    optimizerScales[8]  =  1.0;
    optimizerScales[9]  =  translationScale;
    optimizerScales[10] =  translationScale;
    optimizerScales[11] =  translationScale;

  OptimizerType::Pointer optimizer = OptimizerType::New();
    optimizer->SetScales( optimizerScales );
    optimizer->SetMaximumStepLength( maxStepLength );
    optimizer->SetMinimumStepLength( minStepLength );
    optimizer->SetNumberOfIterations( numberOfIterations );

  InterpolatorType::Pointer interpolator = InterpolatorType::New();

  std::cout << "Starting registration..." << std::endl;
  RegistrationType::Pointer registration = RegistrationType::New();
    registration->SetMetric( metric );
    registration->SetOptimizer( optimizer );
    registration->SetInterpolator( interpolator );
    registration->SetTransform( transform );          
    registration->SetFixedImage( subSampledFixedImage );
    registration->SetMovingImage( subSampledMovingImage );
    registration->SetFixedImageRegion( subSampledFixedImage->GetBufferedRegion() );
    registration->SetInitialTransformParameters( transform->GetParameters());      
  try
    {
    registration->Initialize();
    registration->Update();
    }
  catch( itk::ExceptionObject &excp )
    {
    std::cerr << "ExceptionObject caught while executing registration" << std::endl;
    std::cerr << excp << std::endl;
    }

  //get all params to output to file
  numberOfIterations = optimizer->GetCurrentIteration();
  //  The value of the image metric corresponding to the last set of parameters
  const double bestValue = optimizer->GetValue();
  std::cout << "Final metric value: " << optimizer->GetValue() << std::endl;
  OptimizerType::ParametersType finalParams = registration->GetLastTransformParameters();

  TransformType::Pointer finalTransform = TransformType::New();
    finalTransform->SetParameters( finalParams );
    finalTransform->SetCenter( transform->GetCenter() );

  if ( strcmp(outputTransformFileName.c_str(), "q") != 0 )
    {
      std::string infoFilename = outputTransformFileName;
      int result = infoFilename.find_last_of('.');
      if (std::string::npos != result)
	{
	  infoFilename.erase(result);
	}
      // append extension:
      infoFilename.append(".xml");

      REGISTRATION_XML_DATA labelMapRegistrationXMLData;
      labelMapRegistrationXMLData.similarityValue = (float)(bestValue);
      const char *similarity_type = metric->GetNameOfClass();
      labelMapRegistrationXMLData.similarityMeasure.assign(similarity_type);
        
      //labelMapRegistrationXMLData.image_type.assign("leftLungRightLung");
      labelMapRegistrationXMLData.transformationIndex = 0;
        
      // TODO: See if xml file is empty and change the transformation
      // index accordingly
        
      //if the patient IDs are specified  as args, use them,
      //otherwise, NA

      int pathLength = 0, pos=0, next=0;

      if ( strcmp(movingImageID.c_str(), "q") != 0 )
	{
	  labelMapRegistrationXMLData.sourceID.assign(movingImageID);
	}
      else
	{
	  labelMapRegistrationXMLData.sourceID.assign("N/A");
	}	
    
      if ( strcmp(fixedImageID.c_str(), "q") != 0 )
	{
	  labelMapRegistrationXMLData.destID.assign(fixedImageID);
	}
      else
	{
	  labelMapRegistrationXMLData.destID.assign("N/A");
	}

      // Remove path from output transformation file before storing in xml
      pos=0;
      next=0;
      for (int i = 0; i < (pathLength);i++)
        {
	  pos = next+1;
	  next = outputTransformFileName.find('/', next+1);
        }

      labelMapRegistrationXMLData.transformationLink.assign(outputTransformFileName.c_str());
      labelMapRegistrationXMLData.transformationLink.erase(0,pos);
      WriteRegistrationXML(infoFilename.c_str(),labelMapRegistrationXMLData);

      std::cout << "Writing transform..." << std::endl;
      itk::TransformFileWriter::Pointer transformWriter = itk::TransformFileWriter::New();
        transformWriter->SetInput( finalTransform );   
	transformWriter->SetFileName( outputTransformFileName );
	transformWriter->Update();
    }

  if ( strcmp(outputImageFileName.c_str(), "q") != 0 )
    {
      std::cout << "Resampling moving image..." << std::endl;        
      ResampleFilterType::Pointer resample = ResampleFilterType::New();
        resample->SetTransform( finalTransform );
	resample->SetInput( movingLabelMap);
	resample->SetSize(fixedLabelMap->GetLargestPossibleRegion().GetSize());
	resample->SetOutputOrigin( fixedLabelMap->GetOrigin() );
	resample->SetOutputSpacing( fixedLabelMap->GetSpacing());
	resample->SetOutputDirection( fixedLabelMap->GetDirection() );
	resample->SetInterpolator( interpolator );
	resample->SetDefaultPixelValue( 0 );
	resample->Update();

      //ImageType::Pointer upsampledImage = ImageType::New();
      //std::cout << "Upsampling to original size..." << std::endl;
      //ResampleImage( resample->GetOutput(), upsampledImage,1.0/downsampleFactor );
      //upsampledImage=cip::UpsampleLabelMap(downsampleFactor,resample->GetOutput());

      cip::LabelMapWriterType::Pointer writer = cip::LabelMapWriterType::New();
        writer->SetInput(resample->GetOutput());
	writer->SetFileName( outputImageFileName.c_str() );
	writer->UseCompressionOn();
      try
	{
	writer->Update();
	}
      catch ( itk::ExceptionObject &excp )
	{
	std::cerr << "Exception caught writing output image:";
	std::cerr << excp << std::endl;
	}
    }

  std::cout << "DONE." << std::endl;

  return 0;
}
