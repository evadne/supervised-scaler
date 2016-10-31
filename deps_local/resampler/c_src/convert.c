// 
// VIPS Resampler (C Server).
// Portions from VIPS Thumbnailer, https://github.com/jcupitt/libvips/blob/master/tools/vipsthumbnail.c
// 

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <vips/vips.h>
#include <vips/vips.h>

// Linear vs. Perceptual. It would seem that linear vs. perceptual space is
// not a concern, since this program’s output will be solely used in a browser
// and not in further applications downstream.
#define LINEAR_PROCESSING 0
#define PROCESSING_COLOUR_SPACE (LINEAR_PROCESSING ? VIPS_INTERPRETATION_scRGB : VIPS_INTERPRETATION_sRGB)
#define EXPORTING_COLOUR_SPACE VIPS_INTERPRETATION_sRGB

// Define whether memory can be used between passes. By default this means a 100MB high water mark
// within each pass and no cached information to persist between passes. In the future this can be made tweakable
// since code can be deployed in many places. Also threading should be tweakable.
#define CACHE_BYTES_ALLOWED_WITHIN_EACH_PASS 104857600
#define CACHE_BYTES_ALLOWED_BETWEEN_EACH_PASS 0

double shrinkFactorForImage (VipsImage *fromImage, VipsAngle fromAngle, unsigned int toWidth, unsigned int toHeight) {
  unsigned int const fromWidth = vips_image_get_width(fromImage);
  unsigned int const fromHeight = vips_image_get_height(fromImage);
  switch (fromAngle) {
    case VIPS_ANGLE_D90:
    case VIPS_ANGLE_D270:
      return fmin(((double)fromHeight / (double)toWidth), ((double)fromWidth / (double)toHeight));
    default:
      return fmin(((double)fromWidth / (double)toWidth), ((double)fromHeight / (double)toHeight));
  }
}

unsigned int jpegShrinkFactorForFactor (double shrinkFactor) {
  if (LINEAR_PROCESSING) {
    return 1;
  } else {
    if (shrinkFactor >= 16) {
      return 8;
    } else if (shrinkFactor >= 8) {
      return 4;
    } else if (shrinkFactor >= 4) {
      return 2;
    } else {
      return 1;
    }
  }
}

void computeImageShrinkFactorAndAngle (VipsImage *image, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  *outAngle = vips_autorot_get_angle(image);
  *outShrinkFactor = shrinkFactorForImage(image, *outAngle, toWidth, toHeight);
}

VipsImage * newImageFromGenericPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, NULL);
  computeImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromJPEGPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *headerImage = newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  g_object_unref(headerImage);
  
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, "shrink", jpegShrinkFactorForFactor(*outShrinkFactor), NULL);
  computeImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromPDFPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *headerImage = newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  g_object_unref(headerImage);
  
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, "scale", (1.0 / (*outShrinkFactor)), NULL);
  computeImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromSVGPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  return newImageFromPDFPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
}

VipsImage * newImageFromWebPPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  VipsImage *headerImage = newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  g_object_unref(headerImage);
  
  VipsImage *loadedImage = vips_image_new_from_file(filePath, "access", VIPS_ACCESS_SEQUENTIAL, "shrink", *outShrinkFactor, NULL);
  computeImageShrinkFactorAndAngle(loadedImage, toWidth, toHeight, outShrinkFactor, outAngle);
  return loadedImage;
}

VipsImage * newImageFromPath (char *filePath, unsigned int toWidth, unsigned int toHeight, double *outShrinkFactor, VipsAngle *outAngle) {
  const char *loaderName = vips_foreign_find_load(filePath);
  if (!loaderName) {
    return NULL;
  } else if (0 == strcmp(loaderName, "VipsForeignLoadJpegFile")) {
    return newImageFromJPEGPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  } else if (0 == strcmp(loaderName, "VipsForeignLoadPdfFile")) {
    return newImageFromPDFPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  } else if (0 == strcmp(loaderName, "VipsForeignLoadSvgFile")) {
    return newImageFromSVGPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  } else if (0 == strcmp(loaderName, "VipsForeignLoadWebpFile")) {
    return newImageFromWebPPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  } else {
    return newImageFromGenericPath(filePath, toWidth, toHeight, outShrinkFactor, outAngle);
  }
}

bool shouldImportColourProfileForImage (VipsImage *image) {
  const VipsInterpretation fromInterpretation = vips_image_get_interpretation(image);
  const VipsCoding fromCoding = vips_image_get_coding(image);
  const VipsBandFormat fromBandFormat = vips_image_get_format(image);
  
  if (LINEAR_PROCESSING || (fromInterpretation == VIPS_INTERPRETATION_CMYK)) {
    if (fromCoding == VIPS_CODING_NONE) {
      if ((fromBandFormat == VIPS_FORMAT_UCHAR) || (fromBandFormat == VIPS_FORMAT_USHORT)) {
        if (vips_image_get_typeof(image, VIPS_META_ICC_NAME)) {
          return true;
        }
      }
    }
  }
  
  return false;
}

bool shouldConvertToProcessingColourSpaceForImage (VipsImage *image) {
  const VipsInterpretation fromInterpretation = vips_image_get_interpretation(image);
  const VipsInterpretation toInterpretation = PROCESSING_COLOUR_SPACE;
  
  if (fromInterpretation != toInterpretation) {
    return true;
  } else {
    return false;
  }
}

bool shouldPremultiplyImage (VipsImage *image) {
  const int fromNumberOfBands = vips_image_get_bands(image);
  
  if (fromNumberOfBands == 2) {
    return true;
  }
  
  if (fromNumberOfBands == 4) {
    const VipsInterpretation fromInterpretation = vips_image_get_interpretation(image);
    if (fromInterpretation != VIPS_INTERPRETATION_CMYK) {
      return true;
    }
  }
  
  if (fromNumberOfBands == 5) {
    return true;
  }
  
  return false;
}

bool shouldConvertToExportingColourSpaceForImage (VipsImage *image) {
  VipsInterpretation const fromInterpretation = vips_image_get_interpretation(image);
  VipsInterpretation const toInterpretation = EXPORTING_COLOUR_SPACE;
  
  if (fromInterpretation != toInterpretation) {
    return true;
  } else {
    return false;
  }
}

VipsImage *newThumbnailImageFromImage (VipsObject *context, VipsImage *parentImage, VipsAngle angle, double shrinkFactor) {
  VipsImage **localImages = (VipsImage **)vips_object_local_array(context, 10);
  VipsImage *currentImage = parentImage;
  
  //
  // Special unpacking for RAD
  //
  if (VIPS_CODING_RAD == vips_image_get_coding(currentImage)) {
    VipsImage *unpackedImage = NULL;
    if (vips_rad2float(currentImage, &unpackedImage, NULL)) {
      return NULL;
    }
    currentImage = localImages[0] = unpackedImage;
  }
  
  bool havePremultiplied = false;
  VipsBandFormat imageBandFormatBeforePremultiplication = VIPS_FORMAT_NOTSET;
  
  if (shouldImportColourProfileForImage(currentImage)) {
    VipsImage *importedImage = NULL;
    if (vips_icc_import(currentImage, &importedImage, "embedded", TRUE, "pcs", VIPS_PCS_XYZ, NULL)) {
      return NULL;
    }
    currentImage = localImages[1] = importedImage;
  }
  
  if (shouldConvertToProcessingColourSpaceForImage(currentImage)) {
    VipsImage *convertedImage = NULL;
    if (vips_colourspace(currentImage, &convertedImage, PROCESSING_COLOUR_SPACE, NULL)) {
      return NULL;
    }
    currentImage = localImages[2] = convertedImage;
  }
  
  if (shouldPremultiplyImage(currentImage)) {
    imageBandFormatBeforePremultiplication = vips_image_get_format(currentImage);
    VipsImage *premultipliedImage = NULL;
    if (vips_premultiply(currentImage, &premultipliedImage, NULL)) {
      return NULL;
    }
    havePremultiplied = true;
    currentImage = localImages[3] = premultipliedImage;
  }
  
  VipsImage *resizedImage = NULL;
  if (vips_resize(currentImage, &resizedImage, (1.0 / shrinkFactor), "centre", TRUE, NULL)) {
    return NULL;
  }
  currentImage = localImages[4] = resizedImage;
  
  if (havePremultiplied) {
    VipsImage *unpremultipliedImage = NULL;
    if (vips_unpremultiply(currentImage, &unpremultipliedImage, NULL)) {
      return NULL;
    }
    currentImage = localImages[5] = unpremultipliedImage;
    
    VipsImage *castImage = NULL;
    if (vips_cast(currentImage, &castImage, imageBandFormatBeforePremultiplication, NULL)) {
      return NULL;
    }
    currentImage = localImages[6] = unpremultipliedImage;
  }
  
  if (shouldConvertToExportingColourSpaceForImage(currentImage)) {
    VipsImage *exportedImage = NULL;
    if (vips_colourspace(currentImage, &exportedImage, EXPORTING_COLOUR_SPACE, NULL)) {
      return NULL;
    }
    currentImage = localImages[7] = exportedImage;
  }
  
  if (vips_image_get_typeof(currentImage, VIPS_META_ICC_NAME)) {
    if (!vips_image_remove(currentImage, VIPS_META_ICC_NAME)) { 
      return NULL;
    }
  }
  
  return currentImage;
}

char * pathWithWrittenDataForImage(VipsImage *image) {
  //
  // Find a temporary file and create it directly using mkstemps,
  // add a path extension so libVIPS knows which format to use.
  // Note: in the future we should allow configuring the server for PNG, JPEG and perhaps WebM.
  //
  static char * const fileTemplate = "/tmp/converter-XXXXXX.jpg";
  char *filePath = (char *)malloc(strlen(fileTemplate) + 1);
  strcpy(filePath, fileTemplate);
  
  if (!(mkstemps(filePath, 4))) {
    fprintf(stderr, "ERROR - Unable to open exclusive temporary file\n");
    free(filePath);
    return NULL;
  }
  
  //
  // Save the file. Try to catch any sort of error if needed.
  //
  if (0 != vips_image_write_to_file(image, filePath, "strip", TRUE, NULL)) {
    return NULL;
  }
  
  return filePath;
}

void processLine (char *lineBuffer, VipsObject *context) {
  //
  // Attempt to scan information from input string.
  //
  char fromFilePath[4096];
  unsigned int toWidth = 0;
  unsigned int toHeight = 0;
  if (3 != sscanf(lineBuffer, "%u %u %[^\n]s", &toWidth, &toHeight, fromFilePath)) {
    fprintf(stderr, "ERROR - line can not be parsed\n");
    return;
  }
  
  //
  // Try to open file.
  // If file can not be opened, fail via STDERR.
  //
  VipsAngle fromAngle = VIPS_ANGLE_D0;
  double shrinkFactor = 1.0;
  VipsImage *fromImage = newImageFromPath(fromFilePath, toWidth, toHeight, &shrinkFactor, &fromAngle);
  if (!fromImage) {
    fprintf(stderr, "ERROR - Unable to open file\n");
    return;
  }
  vips_object_local(context, fromImage);
  
  VipsImage *thumbnailImage = newThumbnailImageFromImage(context, fromImage, fromAngle, shrinkFactor);
  if (!thumbnailImage) {
    fprintf(stderr, "ERROR - Unable to resize image\n");
    return;
  }
  
  char *toFilePath = pathWithWrittenDataForImage(thumbnailImage);
  if (toFilePath) {
    printf("%s\n", toFilePath);
    free(toFilePath);
  } else {
    fprintf(stderr, "ERROR - Unable to write file\n");
  }
}

int main (int argc, char **argv) {
  //
  // Start libVIPS at the beginning of the program so once it starts accepting input
  // it is actually ready.
  //
  if (VIPS_INIT(argv[0])) {
    vips_error_exit(NULL);
  }

  //
  // Spin up an infinite loop to accept conversion requests from STDIN
  // with the format: <width> <height> <file path>.
  //
  // Note: fgets blocks, without burning CPU
  //
  char lineBuffer[4096];
  while (fgets(lineBuffer, 4096, stdin)) {
    //
    // Start the main process loop again.
    //
    VipsObject *context = VIPS_OBJECT(vips_image_new()); 
    processLine(lineBuffer, context);
    vips_error_clear();
    g_object_unref(context);
    
    //
    // Empty cache but allow a higher high water mark within each pass,
    // if so desired.
    //
    vips_cache_set_max_mem(CACHE_BYTES_ALLOWED_BETWEEN_EACH_PASS);
    vips_cache_set_max_mem(CACHE_BYTES_ALLOWED_WITHIN_EACH_PASS);
  }
  
  vips_shutdown();
  return 0;
}
