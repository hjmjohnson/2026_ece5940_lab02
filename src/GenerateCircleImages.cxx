/**
 * GenerateCircleImages.cxx
 *
 * Generates two 2D binary circle images for use in the registration lab:
 *
 *   Fixed  image: 30 mm diameter circle, centred at (50, 50) mm
 *   Moving image: 60 mm diameter circle, centred at (200, 200) mm
 *
 * Both images are stored as MetaImage (.mha) files with full spatial metadata
 * so that ITK registration operates correctly in physical space.
 *
 * Usage:
 *   GenerateCircleImages <fixed_output.mha> <moving_output.mha>
 *
 * Requires ITK >= 6.
 */

#include "itkImage.h"
#include "itkImageFileWriter.h"
#include "itkImageRegionIteratorWithIndex.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>

// ── Type aliases ─────────────────────────────────────────────────────────────
using PixelType = unsigned char;
static constexpr unsigned int Dimension = 2;
using ImageType = itk::Image<PixelType, Dimension>;

// ── Helper: create a binary circle image ─────────────────────────────────────
/**
 * CreateCircleImage
 *
 * @param sizeX      Image width  in pixels
 * @param sizeY      Image height in pixels
 * @param spacing    Isotropic pixel spacing in mm/pixel
 * @param centerMm   Physical-space centre of the circle {x_mm, y_mm}
 * @param radiusMm   Radius of the circle in mm
 * @return           Newly allocated ITK image
 */
static ImageType::Pointer
CreateCircleImage(
  itk::SizeValueType sizeX,
  itk::SizeValueType sizeY,
  double             spacing,
  std::array<double, 2> centerMm,
  double             radiusMm)
{
  // ── Allocate image ────────────────────────────────────────────────────────
  auto image = ImageType::New();

  ImageType::SizeType size;
  size[0] = sizeX;
  size[1] = sizeY;

  ImageType::IndexType start;
  start.Fill(0);

  ImageType::RegionType region(start, size);
  image->SetRegions(region);

  ImageType::SpacingType sp;
  sp.Fill(spacing);
  image->SetSpacing(sp);

  // Origin at (0, 0) — physical coordinates start at the corner of pixel 0,0
  ImageType::PointType origin;
  origin.Fill(0.0);
  image->SetOrigin(origin);

  image->Allocate(true); // fill with zeros

  // ── Fill pixels inside the circle ─────────────────────────────────────────
  itk::ImageRegionIteratorWithIndex<ImageType> it(image, region);
  for (it.GoToBegin(); !it.IsAtEnd(); ++it)
  {
    ImageType::IndexType idx = it.GetIndex();

    // Convert pixel index to physical-space point (centre of pixel)
    ImageType::PointType pt;
    image->TransformIndexToPhysicalPoint(idx, pt);

    double dx = pt[0] - centerMm[0];
    double dy = pt[1] - centerMm[1];
    if (std::sqrt(dx * dx + dy * dy) <= radiusMm)
    {
      it.Set(255);
    }
  }

  return image;
}

// ── main ─────────────────────────────────────────────────────────────────────
int
main(int argc, char * argv[])
{
  if (argc != 3)
  {
    std::cerr << "Usage: " << argv[0]
              << " <fixed_output.mha> <moving_output.mha>\n"
              << "\n"
              << "  fixed_output.mha  — 30 mm diameter circle at (50, 50) mm\n"
              << "  moving_output.mha — 60 mm diameter circle at (200, 200) mm\n";
    return EXIT_FAILURE;
  }

  const std::string fixedPath  = argv[1];
  const std::string movingPath = argv[2];

  // Pixel spacing: 0.5 mm per pixel for both images
  constexpr double spacing = 0.5;

  // ── Fixed image: 30 mm diameter circle, centre (50, 50) mm ───────────────
  // 256 x 256 pixels @ 0.5 mm/px → field of view 128 x 128 mm
  // Circle fits well within the field of view.
  auto fixedImage = CreateCircleImage(
    /* sizeX    */ 256,
    /* sizeY    */ 256,
    /* spacing  */ spacing,
    /* center   */ { 50.0, 50.0 },
    /* radius   */ 15.0); // 30 mm diameter → 15 mm radius

  // ── Moving image: 60 mm diameter circle, centre (200, 200) mm ────────────
  // 512 x 512 pixels @ 0.5 mm/px → field of view 256 x 256 mm
  // Circle fits well within the field of view.
  auto movingImage = CreateCircleImage(
    /* sizeX    */ 512,
    /* sizeY    */ 512,
    /* spacing  */ spacing,
    /* center   */ { 200.0, 200.0 },
    /* radius   */ 30.0); // 60 mm diameter → 30 mm radius

  // ── Write images ──────────────────────────────────────────────────────────
  using WriterType = itk::ImageFileWriter<ImageType>;

  try
  {
    auto writer = WriterType::New();

    writer->SetFileName(fixedPath);
    writer->SetInput(fixedImage);
    writer->Update();
    std::cout << "Wrote fixed  image: " << fixedPath  << '\n';

    writer->SetFileName(movingPath);
    writer->SetInput(movingImage);
    writer->Update();
    std::cout << "Wrote moving image: " << movingPath << '\n';
  }
  catch (const itk::ExceptionObject & err)
  {
    std::cerr << "ITK exception caught while writing images:\n"
              << err << '\n';
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
