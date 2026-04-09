/**
 * RegisterCircles.cxx
 *
 * Registers two 2D binary circle images using ITK v6's registration framework.
 *
 * Algorithm
 * ---------
 *   Transform : itk::Similarity2DTransform  (scale + rotation + translation)
 *   Metric    : itk::MeanSquaresImageToImageMetricv4
 *   Optimizer : itk::RegularStepGradientDescentOptimizerv4
 *   Strategy  : Multi-resolution (3 levels, shrink factors 4/2/1)
 *
 * Expected result for the lab test images
 * ----------------------------------------
 *   Fixed  image: 30 mm circle at (50, 50) mm
 *   Moving image: 60 mm circle at (200, 200) mm
 *
 *   ITK registration finds the transform that maps fixed-space → moving-space:
 *     Scale       ≈ 2.0000  (moving circle is twice as large)
 *     Angle       ≈ 0.0000 deg
 *     Translation ≈ (100, 100) mm  (with rotation centre at origin)
 *
 * Usage
 * -----
 *   RegisterCircles <fixed.mha> <moving.mha> <output.mha>
 *
 * Requires ITK >= 6.
 */

#include "itkCastImageFilter.h"
#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkImageRegistrationMethodv4.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkMeanSquaresImageToImageMetricv4.h"
#include "itkRegularStepGradientDescentOptimizerv4.h"
#include "itkResampleImageFilter.h"
#include "itkSimilarity2DTransform.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>

// ── Type aliases ─────────────────────────────────────────────────────────────
using InputPixelType  = unsigned char;
using InternalPixelType = double;
static constexpr unsigned int Dimension = 2;

using InputImageType    = itk::Image<InputPixelType,    Dimension>;
using InternalImageType = itk::Image<InternalPixelType, Dimension>;

using TransformType   = itk::Similarity2DTransform<double>;
using OptimizerType   = itk::RegularStepGradientDescentOptimizerv4<double>;
using MetricType      = itk::MeanSquaresImageToImageMetricv4<InternalImageType, InternalImageType>;
using RegistrationType = itk::ImageRegistrationMethodv4<InternalImageType, InternalImageType, TransformType>;

// ── Observer: print optimizer progress each iteration ────────────────────────
class IterationObserver : public itk::Command
{
public:
  using Self      = IterationObserver;
  using Superclass = itk::Command;
  using Pointer   = itk::SmartPointer<Self>;
  itkNewMacro(Self);

protected:
  IterationObserver() = default;

public:
  using OptimizerPointer = const OptimizerType *;

  void
  Execute(itk::Object * caller, const itk::EventObject & event) override
  {
    Execute(static_cast<const itk::Object *>(caller), event);
  }

  void
  Execute(const itk::Object * caller, const itk::EventObject & event) override
  {
    if (!itk::IterationEvent().CheckEvent(&event))
    {
      return;
    }
    auto optimizer = static_cast<OptimizerPointer>(caller);
    std::cout << "  Iter " << std::setw(4) << optimizer->GetCurrentIteration()
              << "  metric = " << std::setw(12) << optimizer->GetValue()
              << "  step = " << optimizer->GetCurrentStepLength()
              << '\n';
  }
};

// ── Cast utility: InputImageType → InternalImageType ─────────────────────────
static InternalImageType::Pointer
CastToInternal(const InputImageType * img)
{
  using CastFilter = itk::CastImageFilter<InputImageType, InternalImageType>;
  auto caster = CastFilter::New();
  caster->SetInput(img);
  caster->Update();
  return caster->GetOutput();
}

// ── main ─────────────────────────────────────────────────────────────────────
int
main(int argc, char * argv[])
{
  if (argc != 4)
  {
    std::cerr << "Usage: " << argv[0]
              << " <fixed.mha> <moving.mha> <output.mha>\n"
              << "\n"
              << "  fixed.mha  — reference image (30 mm circle)\n"
              << "  moving.mha — image to register (60 mm circle)\n"
              << "  output.mha — resampled moving image after registration\n";
    return EXIT_FAILURE;
  }

  const std::string fixedPath  = argv[1];
  const std::string movingPath = argv[2];
  const std::string outputPath = argv[3];

  // ── Read input images ─────────────────────────────────────────────────────
  using ReaderType = itk::ImageFileReader<InputImageType>;

  InternalImageType::Pointer fixedImage;
  InternalImageType::Pointer movingImage;

  try
  {
    auto fixedReader = ReaderType::New();
    fixedReader->SetFileName(fixedPath);
    fixedReader->Update();
    fixedImage = CastToInternal(fixedReader->GetOutput());

    auto movingReader = ReaderType::New();
    movingReader->SetFileName(movingPath);
    movingReader->Update();
    movingImage = CastToInternal(movingReader->GetOutput());
  }
  catch (const itk::ExceptionObject & err)
  {
    std::cerr << "Error reading input images:\n" << err << '\n';
    return EXIT_FAILURE;
  }

  // ── Initialise transform ──────────────────────────────────────────────────
  // ITK's registration finds the transform mapping fixed-space → moving-space.
  // We start from the identity (scale=1, angle=0, translation=(0,0)).
  // The rotation centre is left at the origin so that the recovered translation
  // has a clean physical interpretation:
  //   T(x) = scale * x + translation
  //   T(50,50) = 2*(50,50) + (100,100) = (200,200)  [circle centres align]
  auto transform = TransformType::New();
  transform->SetIdentity();

  // ── Configure metric ──────────────────────────────────────────────────────
  auto metric = MetricType::New();
  // Use all pixels (no sampling) — images are small and binary
  metric->SetUseMovingImageGradientFilter(false);

  // ── Configure optimizer ───────────────────────────────────────────────────
  auto optimizer = OptimizerType::New();
  optimizer->SetLearningRate(4.0);
  optimizer->SetMinimumStepLength(0.001);
  optimizer->SetRelaxationFactor(0.5);
  optimizer->SetNumberOfIterations(300);
  optimizer->SetGradientMagnitudeTolerance(1e-4);
  optimizer->ReturnBestParametersAndValueOn();

  // Attach iteration observer for verbose output
  auto observer = IterationObserver::New();
  optimizer->AddObserver(itk::IterationEvent(), observer);

  // ── Configure registration ────────────────────────────────────────────────
  auto registration = RegistrationType::New();
  registration->SetFixedImage(fixedImage);
  registration->SetMovingImage(movingImage);
  registration->SetMetric(metric);
  registration->SetOptimizer(optimizer);
  registration->SetInitialTransform(transform);
  registration->InPlaceOn(); // update transform in place

  // Multi-resolution: 3 levels (coarse → fine)
  constexpr unsigned int NumberOfLevels = 3;

  RegistrationType::ShrinkFactorsArrayType shrinkFactors(NumberOfLevels);
  shrinkFactors[0] = 4;
  shrinkFactors[1] = 2;
  shrinkFactors[2] = 1;

  RegistrationType::SmoothingSigmasArrayType smoothingSigmas(NumberOfLevels);
  smoothingSigmas[0] = 2.0;
  smoothingSigmas[1] = 1.0;
  smoothingSigmas[2] = 0.0;

  registration->SetNumberOfLevels(NumberOfLevels);
  registration->SetShrinkFactorsPerLevel(shrinkFactors);
  registration->SetSmoothingSigmasPerLevel(smoothingSigmas);
  registration->SmoothingSigmasAreSpecifiedInPhysicalUnitsOn();

  // ── Run registration ──────────────────────────────────────────────────────
  std::cout << "Starting registration...\n";
  try
  {
    registration->Update();
  }
  catch (const itk::ExceptionObject & err)
  {
    std::cerr << "Registration failed:\n" << err << '\n';
    return EXIT_FAILURE;
  }

  // ── Report results ────────────────────────────────────────────────────────
  const TransformType * finalTransform = registration->GetModifiableTransform();
  const double          scale          = finalTransform->GetScale();
  const double          angle          = finalTransform->GetAngle();
  const auto            translation    = finalTransform->GetTranslation();
  const double          metricValue    = optimizer->GetValue();

  constexpr double radToDeg = 180.0 / itk::Math::pi;

  std::cout << "\n=== Registration Result ===\n"
            << "Scale        : " << std::fixed << std::setprecision(4) << scale
            << "  (expected 2.0000)\n"
            << "Angle (deg)  : " << std::setprecision(4) << angle * radToDeg
            << "  (expected 0.0000)\n"
            << "Translation  : [" << std::setprecision(2) << translation[0]
            << ", " << translation[1] << "] mm"
            << "  (expected [100.00, 100.00] mm)\n"
            << "Metric value : " << std::setprecision(6) << metricValue << '\n';

  // ── Resample moving image into fixed image space ──────────────────────────
  using ResampleFilter = itk::ResampleImageFilter<InternalImageType, InternalImageType>;
  auto resample = ResampleFilter::New();
  resample->SetTransform(finalTransform);
  resample->SetInput(movingImage);
  resample->SetSize(fixedImage->GetLargestPossibleRegion().GetSize());
  resample->SetOutputOrigin(fixedImage->GetOrigin());
  resample->SetOutputSpacing(fixedImage->GetSpacing());
  resample->SetOutputDirection(fixedImage->GetDirection());
  resample->SetDefaultPixelValue(0.0);

  // Cast result back to unsigned char for writing
  using OutputImageType = InputImageType;
  using CastFilter      = itk::CastImageFilter<InternalImageType, OutputImageType>;
  auto caster = CastFilter::New();
  caster->SetInput(resample->GetOutput());

  using WriterType = itk::ImageFileWriter<OutputImageType>;
  auto writer = WriterType::New();
  writer->SetFileName(outputPath);
  writer->SetInput(caster->GetOutput());

  try
  {
    writer->Update();
    std::cout << "Wrote registered image: " << outputPath << '\n';
  }
  catch (const itk::ExceptionObject & err)
  {
    std::cerr << "Error writing output image:\n" << err << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
