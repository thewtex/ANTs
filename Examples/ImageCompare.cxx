#include "antsUtilities.h"
#include <algorithm>

#include "itkWin32Header.h"
#include <iostream>
#include <fstream>
#include "itkNumericTraits.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkRescaleIntensityImageFilter.h"
#include "itkExtractImageFilter.h"
#include "itkTestingComparisonImageFilter.h"

namespace ants
{
using namespace std;

#define ITK_TEST_DIMENSION_MAX 6

int
RegressionTestImage(const char *, const char *, int, bool);

// entry point for the library; parameter 'args' is equivalent to 'argv' in (argc,argv) of commandline parameters to
// 'main()'
int
ImageCompare(std::vector<std::string> args, std::ostream * /*out_stream = nullptr */)
{
  // put the arguments coming in as 'args' into standard (argc,argv) format;
  // 'args' doesn't have the command name as first, argument, so add it manually;
  // 'args' may have adjacent arguments concatenated into one argument,
  // which the parser should handle
  args.insert(args.begin(), "ImageCompare");

  int     argc = args.size();
  char ** argv = new char *[args.size() + 1];
  for (unsigned int i = 0; i < args.size(); ++i)
  {
    // allocate space for the string plus a null character
    argv[i] = new char[args[i].length() + 1];
    std::strncpy(argv[i], args[i].c_str(), args[i].length());
    // place the null character in the end
    argv[i][args[i].length()] = '\0';
  }
  argv[argc] = nullptr;
  // class to automatically cleanup argv upon destruction
  class Cleanup_argv
  {
  public:
    Cleanup_argv(char ** argv_, int argc_plus_one_)
      : argv(argv_)
      , argc_plus_one(argc_plus_one_)
    {}

    ~Cleanup_argv()
    {
      for (unsigned int i = 0; i < argc_plus_one; ++i)
      {
        delete[] argv[i];
      }
      delete[] argv;
    }

  private:
    char **      argv;
    unsigned int argc_plus_one;
  };
  Cleanup_argv cleanup_argv(argv, argc + 1);

  // antscout->set_stream( out_stream );

  if (argc < 3)
  {
    cerr << "Usage:" << endl;
    cerr << "testImage, baselineImage1, [baselineImage2, baselineImage3, ...]" << endl;
    cerr << "Note that if you supply more than one baselineImage, this test will pass if any" << endl;
    cerr << "of them match the testImage" << endl;
    if (argc >= 2 && (std::string(argv[1]) == std::string("--help") || std::string(argv[1]) == std::string("-h")))
    {
      return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
  }
  int bestBaselineStatus = 2001;
  int bestBaseline = 2;
  try
  {
    if (argc == 3)
    {
      bestBaselineStatus = RegressionTestImage(argv[1], argv[2], 0, false);
    }
    else
    {
      int currentStatus = 2001;
      for (int i = 2; i < argc; i++)
      {
        currentStatus = RegressionTestImage(argv[1], argv[i], 0, false);
        if (currentStatus < bestBaselineStatus)
        {
          bestBaselineStatus = currentStatus;
          bestBaseline = i;
        }
        if (bestBaselineStatus == 0)
        {
          break;
        }
      }
    }
    // generate images of our closest match
    if (bestBaselineStatus == 0)
    {
      RegressionTestImage(argv[1], argv[bestBaseline], 1, false);
    }
    else
    {
      RegressionTestImage(argv[1], argv[bestBaseline], 1, true);
    }
  }
  catch (const itk::ExceptionObject & e)
  {
    std::cout << "ITK test driver caught an ITK exception:\n";
    std::cout << e.GetFile() << ":" << e.GetLine() << ":\n" << e.GetDescription() << "\n";
    bestBaselineStatus = -1;
  }
  catch (const std::exception & e)
  {
    std::cout << "ITK test driver caught an exception:\n";
    std::cout << e.what() << "\n";
    bestBaselineStatus = -1;
  }
  catch (...)
  {
    std::cout << "ITK test driver caught an unknown exception!!!\n";
    bestBaselineStatus = -1;
  }
  cout << bestBaselineStatus << endl;
  return bestBaselineStatus;
}

// Regression Testing Code
int
RegressionTestImage(const char * testImageFilename,
                    const char * baselineImageFilename,
                    int          reportErrors,
                    bool         differences)
{
  // Use the factory mechanism to read the test and baseline files and convert them to double
  using ImageType = itk::Image<double, 6>;
  using OutputType = itk::Image<unsigned char, 6>;
  using DiffOutputType = itk::Image<unsigned char, 2>;
  using ReaderType = itk::ImageFileReader<ImageType>;

  // Read the baseline file
  ReaderType::Pointer baselineReader = ReaderType::New();
  baselineReader->SetFileName(baselineImageFilename);
  try
  {
    baselineReader->UpdateLargestPossibleRegion();
  }
  catch (itk::ExceptionObject & e)
  {
    std::cout << "Exception detected while reading " << baselineImageFilename << " : " << e.GetDescription();
    return 1000;
  }

  // Read the file generated by the test
  ReaderType::Pointer testReader = ReaderType::New();
  testReader->SetFileName(testImageFilename);
  try
  {
    testReader->UpdateLargestPossibleRegion();
  }
  catch (itk::ExceptionObject & e)
  {
    std::cout << "Exception detected while reading " << testImageFilename << " : " << e.GetDescription() << std::endl;
    return 1000;
  }

  // The sizes of the baseline and test image must match
  ImageType::SizeType baselineSize;
  baselineSize = baselineReader->GetOutput()->GetLargestPossibleRegion().GetSize();
  ImageType::SizeType testSize;
  testSize = testReader->GetOutput()->GetLargestPossibleRegion().GetSize();

  if (baselineSize != testSize)
  {
    std::cout << "The size of the Baseline image and Test image do not match!" << std::endl;
    std::cout << "Baseline image: " << baselineImageFilename << " has size " << baselineSize << std::endl;
    std::cout << "Test image:     " << testImageFilename << " has size " << testSize << std::endl;
    return EXIT_FAILURE;
  }

  // Now compare the two images
  using DiffType = itk::Testing::ComparisonImageFilter<ImageType, ImageType>;
  DiffType::Pointer diff = DiffType::New();
  diff->SetValidInput(baselineReader->GetOutput());
  diff->SetTestInput(testReader->GetOutput());
  diff->SetDifferenceThreshold(2.0);
  diff->UpdateLargestPossibleRegion();

  double status = diff->GetTotalDifference();

  if (reportErrors)
  {
    using RescaleType = itk::RescaleIntensityImageFilter<ImageType, OutputType>;
    using ExtractType = itk::ExtractImageFilter<OutputType, DiffOutputType>;
    using WriterType = itk::ImageFileWriter<DiffOutputType>;
    using RegionType = itk::ImageRegion<6>;
    OutputType::IndexType index;
    index.Fill(0);
    OutputType::SizeType size;
    size.Fill(0);

    RescaleType::Pointer rescale = RescaleType::New();
    rescale->SetOutputMinimum(itk::NumericTraits<unsigned char>::NonpositiveMin());
    rescale->SetOutputMaximum(itk::NumericTraits<unsigned char>::max());
    rescale->SetInput(diff->GetOutput());
    rescale->UpdateLargestPossibleRegion();

    RegionType region;
    region.SetIndex(index);

    size = rescale->GetOutput()->GetLargestPossibleRegion().GetSize();
    for (unsigned int i = 2; i < ITK_TEST_DIMENSION_MAX; i++)
    {
      size[i] = 0;
    }
    region.SetSize(size);

    ExtractType::Pointer extract = ExtractType::New();
    extract->SetInput(rescale->GetOutput());
    extract->SetExtractionRegion(region);

    WriterType::Pointer writer = WriterType::New();
    writer->SetInput(extract->GetOutput());
    if (differences)
    {
      // if there are discrepencies, create an diff image
      std::cout << R"(<DartMeasurement name="ImageError" type="numeric/double">)";
      std::cout << status;
      std::cout << "</DartMeasurement>" << std::endl;

      std::ostringstream diffName;
      diffName << testImageFilename << ".diff.png";
      try
      {
        rescale->SetInput(diff->GetOutput());
        rescale->Update();
      }
      catch (...)
      {
        std::cout << "Error during rescale of " << diffName.str() << std::endl;
      }
      writer->SetFileName(diffName.str().c_str());
      try
      {
        writer->Update();
      }
      catch (...)
      {
        std::cout << "Error during write of " << diffName.str() << std::endl;
      }

      std::cout << R"(<DartMeasurementFile name="DifferenceImage" type="image/png">)";
      std::cout << diffName.str();
      std::cout << "</DartMeasurementFile>" << std::endl;
    }
    std::ostringstream baseName;
    baseName << testImageFilename << ".base.png";
    try
    {
      rescale->SetInput(baselineReader->GetOutput());
      rescale->Update();
    }
    catch (...)
    {
      std::cout << "Error during rescale of " << baseName.str() << std::endl;
    }
    try
    {
      writer->SetFileName(baseName.str().c_str());
      writer->Update();
    }
    catch (...)
    {
      std::cout << "Error during write of " << baseName.str() << std::endl;
    }

    std::cout << R"(<DartMeasurementFile name="BaselineImage" type="image/png">)";
    std::cout << baseName.str();
    std::cout << "</DartMeasurementFile>" << std::endl;

    std::ostringstream testName;
    testName << testImageFilename << ".test.png";
    try
    {
      rescale->SetInput(testReader->GetOutput());
      rescale->Update();
    }
    catch (...)
    {
      std::cout << "Error during rescale of " << testName.str() << std::endl;
    }
    try
    {
      writer->SetFileName(testName.str().c_str());
      writer->Update();
    }
    catch (...)
    {
      std::cout << "Error during write of " << testName.str() << std::endl;
    }

    std::cout << R"(<DartMeasurementFile name="TestImage" type="image/png">)";
    std::cout << testName.str();
    std::cout << "</DartMeasurementFile>" << std::endl;
  }
  return (status != EXIT_SUCCESS) ? EXIT_FAILURE : EXIT_SUCCESS;
}
} // namespace ants
